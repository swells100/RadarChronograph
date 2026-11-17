#include "transport.h"
#include "radar_config.h"
#include "radar_types.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if RADAR_OUT_ETH
#include <QNEthernet.h>
using namespace qindesign::network;
#endif

// Packet ring in RAM2 (plenty of room there; keeps DTCM free for the FFT).
// One ring feeds both outputs; each output has its own read position.
DMAMEM static uint8_t ring[TX_SLOTS][PACKET_BYTES] __attribute__((aligned(32)));
static uint32_t head = 0;               // next slot to fill

static constexpr uint16_t TEXT_Q = 1024;
static constexpr uint16_t TEXT_LINE = 120;

struct Sink {
  bool active = false;
  uint32_t tail = 0, count = 0, off = 0;     // packet read position / pending packets / bytes of tail already written
  char q[TEXT_Q];                            // pending text lines ('#' ... '\n')
  uint16_t qh = 0, qt = 0;                   // read / write positions
  uint32_t sent = 0;
};
static Sink sink_usb, sink_eth;
static uint32_t dropped = 0;
static bool usb_stream = false;

// ---------------------------------------------------------------- text queue
static uint16_t q_used(const Sink &s) { return (s.qt + TEXT_Q - s.qh) % TEXT_Q; }

static void push_line(Sink &s, const char *msg) {
  size_t n = strlen(msg);
  while (n && (msg[n - 1] == '\n' || msg[n - 1] == '\r')) n--;
  if (n > TEXT_LINE) n = TEXT_LINE;
  if ((size_t)(TEXT_Q - 1 - q_used(s)) < n + 2) return;            // queue full: drop the line
  s.q[s.qt] = '#';
  s.qt = (s.qt + 1) % TEXT_Q;
  for (size_t i = 0; i < n; i++) { s.q[s.qt] = msg[i]; s.qt = (s.qt + 1) % TEXT_Q; }
  s.q[s.qt] = '\n';
  s.qt = (s.qt + 1) % TEXT_Q;
}

// Copy the next complete line into out; returns its length (0 if none).
static uint16_t peek_line(const Sink &s, char *out) {
  uint16_t used = q_used(s), i = 0;
  while (i < used && i < TEXT_LINE + 2) {
    char ch = s.q[(s.qh + i) % TEXT_Q];
    out[i++] = ch;
    if (ch == '\n') return i;
  }
  return 0;
}

static void sink_reset(Sink &s) {
  s.tail = head;
  s.count = 0;
  s.off = 0;
  s.qh = s.qt = 0;
}

// ---------------------------------------------------------------- pumping
typedef int (*RoomFn)();
typedef size_t (*WriteFn)(const uint8_t *, size_t);

static void pump(Sink &s, RoomFn room, WriteFn wr) {
  for (;;) {
    int r = room();
    if (r <= 0) return;
    if (s.off == 0 && s.qh != s.qt) {                     // text goes only between packets
      char line[TEXT_LINE + 4];
      uint16_t n = peek_line(s, line);
      if (!n) { s.qh = s.qt = 0; continue; }               // corrupt queue: clear
      if (r < (int)n) return;
      wr((const uint8_t *)line, n);
      s.qh = (s.qh + n) % TEXT_Q;
      continue;
    }
    if (s.count == 0) return;
    uint32_t remain = PACKET_BYTES - s.off;
    uint32_t n = remain < (uint32_t)r ? remain : (uint32_t)r;
    size_t w = wr(&ring[s.tail][s.off], n);
    s.off += w;
    if (s.off >= PACKET_BYTES) {
      s.tail = (s.tail + 1) % TX_SLOTS;
      s.off = 0;
      s.count--;
      s.sent++;
    }
    if (w < n) return;
  }
}

// ---------------------------------------------------------------- USB sink
#if RADAR_OUT_USB
static int usb_room() { return Serial.availableForWrite(); }
static size_t usb_write(const uint8_t *p, size_t n) { return Serial.write(p, n); }
#endif

// ---------------------------------------------------------------- Ethernet sink
#if RADAR_OUT_ETH
static EthernetClient client;
static IPAddress dest_ip(NET_DEST_IP[0], NET_DEST_IP[1], NET_DEST_IP[2], NET_DEST_IP[3]);
static uint32_t last_attempt = 0;
static uint32_t attempts = 0;
static bool had_conn = false;
static int eth_room() { return client.availableForWrite(); }
static size_t eth_write(const uint8_t *p, size_t n) { return client.write(p, n); }
#endif

void transport_init() {
#if RADAR_OUT_ETH
#if RADAR_USE_DHCP
  Ethernet.begin();
#else
  IPAddress ip(NET_LOCAL_IP[0], NET_LOCAL_IP[1], NET_LOCAL_IP[2], NET_LOCAL_IP[3]);
  IPAddress mask(NET_MASK[0], NET_MASK[1], NET_MASK[2], NET_MASK[3]);
  IPAddress gw(NET_GATEWAY[0], NET_GATEWAY[1], NET_GATEWAY[2], NET_GATEWAY[3]);
  Ethernet.begin(ip, mask, gw);
#endif
  client.setConnectionTimeoutEnabled(false);   // connect() must not block the DMA consumer
#endif
}

void transport_service() {
#if RADAR_OUT_ETH
  {
    const bool up = client.connected();
    if (!up) {
      if (had_conn) {                       // connection just died: discard half-sent data
        client.abort();
        had_conn = false;
      }
      if (sink_eth.active) { sink_eth.active = false; sink_reset(sink_eth); }
      if (!client.connecting() && Ethernet.linkState() &&
          (millis() - last_attempt) >= NET_RECONNECT_MS) {
        last_attempt = millis();
        attempts++;
        client.connectNoWait(dest_ip, NET_DEST_PORT);
      }
    } else {
      if (!had_conn) {
        client.setNoDelay(true);
        had_conn = true;
      }
      if (!sink_eth.active) { sink_reset(sink_eth); sink_eth.active = true; }
      pump(sink_eth, eth_room, eth_write);
      client.flush();
    }
  }
#endif
#if RADAR_OUT_USB
  {
    if (usb_stream && !Serial) {            // host closed the port: back to plain text
      usb_stream = false;
    }
    if (usb_stream) {
      if (!sink_usb.active) { sink_reset(sink_usb); sink_usb.active = true; }
      pump(sink_usb, usb_room, usb_write);
      Serial.send_now();
    } else if (sink_usb.active) {
      sink_usb.active = false;
      sink_reset(sink_usb);
    }
  }
#endif
}

uint8_t *transport_acquire_slot() {
  const bool any = sink_eth.active || sink_usb.active;
  if (!any ||
      (sink_eth.active && sink_eth.count >= TX_SLOTS) ||
      (sink_usb.active && sink_usb.count >= TX_SLOTS)) {
    dropped++;
    return nullptr;
  }
  return ring[head];
}

void transport_commit() {
  head = (head + 1) % TX_SLOTS;
  if (sink_eth.active) sink_eth.count++;
  if (sink_usb.active) sink_usb.count++;
}

int transport_read_byte(CmdSource *src) {
#if RADAR_OUT_USB
  if (Serial.available()) { *src = SRC_USB; return Serial.read(); }
#endif
#if RADAR_OUT_ETH
  if (client.connected() && client.available() > 0) { *src = SRC_ETH; return client.read(); }
#endif
  return -1;
}

void transport_reply(CmdSource src, const char *msg) {
  if (src == SRC_USB) {
    if (usb_stream) push_line(sink_usb, msg);
    else Serial.println(msg);
  } else {
    if (sink_eth.active) push_line(sink_eth, msg);
    radar_logf("%s", msg);                  // also show locally
  }
}

void radar_logf(const char *fmt, ...) {
  char line[TEXT_LINE + 8];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  size_t n = strlen(line);
  while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
#if RADAR_OUT_USB
  if (usb_stream) { push_line(sink_usb, line); return; }
#endif
  Serial.println(line);
}

bool transport_usb_enable(bool on) {
#if RADAR_OUT_USB
  if (on && !usb_stream) { sink_reset(sink_usb); sink_usb.active = true; }
  usb_stream = on;
  return true;
#else
  (void)on;
  return false;
#endif
}

bool transport_usb_streaming() { return usb_stream; }

bool transport_eth_connected() {
#if RADAR_OUT_ETH
  return client.connected();
#else
  return false;
#endif
}

uint32_t transport_eth_attempts() {
#if RADAR_OUT_ETH
  return attempts;
#else
  return 0;
#endif
}

bool transport_eth_link() {
#if RADAR_OUT_ETH
  return Ethernet.linkState();
#else
  return false;
#endif
}

uint32_t transport_dropped() { return dropped; }
uint32_t transport_sent_packets() { return sink_eth.sent + sink_usb.sent; }
