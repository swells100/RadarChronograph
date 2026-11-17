// transport.h - sends packets to the PC over Ethernet (TCP) and/or USB serial,
// receives text commands from either, and carries text messages to the PC.
//
// Wire framing (identical on both links): binary packets starting with "RAD2",
// and text lines starting with '#' that appear only BETWEEN packets.
#pragma once
#include <stdint.h>

enum CmdSource { SRC_USB = 0, SRC_ETH = 1 };

void transport_init();

// Call every loop(): connections, pumping queued bytes out.
void transport_service();

// Reserve the next transmit slot (PACKET_BYTES of storage). nullptr if no link
// is up or the ring is full (the block is counted as dropped). Then call transport_commit().
uint8_t *transport_acquire_slot();
void transport_commit();

// One received command byte from either link (sets *src), or -1 if none.
int transport_read_byte(CmdSource *src);

// Text reply to the link a command came from.
void transport_reply(CmdSource src, const char *msg);

// Log line: plain text on USB serial normally; a '#' line in the packet stream while USB streaming.
void radar_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// USB streaming mode (command "stream usb" / "stream off").
bool transport_usb_enable(bool on);   // false if USB output is compiled out
bool transport_usb_streaming();

bool     transport_eth_connected();
bool     transport_eth_link();
uint32_t transport_eth_attempts();   // TCP connection attempts started since power-up
uint32_t transport_dropped();
uint32_t transport_sent_packets();
