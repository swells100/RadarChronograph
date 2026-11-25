"""Showcase mode for receiver_plot.py: a full-screen visualizer in a web browser.

    python receiver_plot.py --showcase --serial auto        # Teensy over USB
    python receiver_plot.py --showcase --port 5000          # Teensy over Ethernet
    python receiver_plot.py --showcase --demo               # simulated radar, no hardware

The program reads the radar packets exactly like the engineering plot does, turns each pass of the
detector into one "shot" (speed measured on the strongest block of the track), and serves a web page
(http://127.0.0.1:8080 by default) that shows it: LED-style speed readout, slow-motion dart replay,
speed ruler with everyday comparisons, leaderboard, an arm button with the 10 s countdown, and a live
scrolling spectrogram ("what the radar hears"). Everything is plain HTML/JS served from this process:
no internet and no extra Python packages are needed (numpy, plus pyserial for USB).

The page talks to this program with Server-Sent Events (/events) and one POST endpoint (/cmd).
"""
import json
import math
import queue
import random
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

import numpy as np

FLAG_ARMED, FLAG_TRACKING, FLAG_NEW_BULLET = 1, 2, 4
C_LIGHT = 299792458.0
CARRIER_HZ = 24.125e9
MPS_PER_HZ = C_LIGHT / (2.0 * CARRIER_HZ)          # v = f * c / (2 f0), about 1/161 m/s per Hz
FRAME_S = 0.03                                      # about 33 chart columns per second
HTML = Path(__file__).with_name("showcase.html")


class ShotTracker:
    """Turns the per-block detector output into one event per shot.

    The firmware sets FLAG_TRACKING only in blocks where the tracked peak is present, and keeps following
    it through up to DET_LOST_BLOCKS (8) blocks without it. So a shot starts at the first tracked block and
    ends when END_GAP blocks in a row had no peak (or the device was disarmed). The reported speed is that of
    the strongest block, which is when the dart is closest to the middle of the radar beam."""

    END_GAP = 9

    def __init__(self, min_speed_mps):
        self.min_speed = min_speed_mps
        self.active = False
        self.samples = []
        self.gap = 0
        self.first_seq = 0
        self.block_s = 0.004096

    def reset(self):
        self.active, self.samples, self.gap = False, [], 0

    def _finish(self):
        samples, first_seq = self.samples, self.first_seq
        self.reset()
        if not samples:
            return None
        best = max(samples, key=lambda s: s[1])
        speed, mag, freq = best
        if speed < self.min_speed:
            return None
        return dict(speed=float(speed), freq=float(freq), dbfs=20 * math.log10(max(mag, 1e-9)),
                    blocks=len(samples), dur_ms=len(samples) * self.block_s * 1e3, seq=first_seq)

    def update(self, m):
        """m: packet metadata (as returned by receiver_plot.read_packet). Returns a shot dict or None."""
        self.block_s = m["n"] / m["fs"]
        flags = m["flags"]
        if not flags & FLAG_ARMED:
            return self._finish() if self.active else None
        out = None
        if self.active and flags & FLAG_NEW_BULLET:        # a second bullet started: close the first one
            out = self._finish()
        if flags & FLAG_TRACKING:
            if not self.active:
                self.active, self.first_seq, self.gap = True, m["seq"], 0
            self.gap = 0
            self.samples.append((m["track_speed"], m["track_mag"], m["track_freq"]))
        elif self.active:
            self.gap += 1
            if self.gap >= self.END_GAP:
                out = out or self._finish()
        return out


class Hub:
    """Fan-out of JSON messages to every connected browser."""

    def __init__(self):
        self.lock = threading.Lock()
        self.clients = set()

    def add(self):
        q = queue.Queue(maxsize=300)
        with self.lock:
            self.clients.add(q)
        return q

    def remove(self, q):
        with self.lock:
            self.clients.discard(q)

    def publish(self, msg):
        data = json.dumps(msg, separators=(",", ":"))
        with self.lock:
            clients = list(self.clients)
        for q in clients:
            try:
                q.put_nowait(data)
            except queue.Full:                             # slow browser: drop its oldest message
                try:
                    q.get_nowait()
                    q.put_nowait(data)
                except (queue.Empty, queue.Full):
                    pass


class Show:
    """All state of the showcase: configuration, shots, link status, and the radar -> browser pipeline."""

    def __init__(self, args):
        self.args = args
        self.hub = Hub()
        self.lock = threading.Lock()
        self.shots = []
        self.link, self.link_text = "waiting", ""
        self.conn = None                                   # raw connection to the Teensy (not in demo mode)
        self.demo_cmd = None                               # callable(str) in demo mode
        self.tracker = ShotTracker(args.min_speed)
        self.fs, self.n = 250000.0, 1024
        self.nb = 0
        self.cfg = {}
        self._recompute_cfg()
        self.maxhold = np.full(self.nb, -200.0)
        self.last_flush = 0.0
        self.armed, self.ms_left = False, 0
        self.track_best = None                             # (mag, speed) of the strongest tracked block this frame
        self.logf = None
        if args.log:
            self.logf = open(args.log, "a")
            if self.logf.tell() == 0:
                self.logf.write("pc_time,bullet_count,seq,freq_hz,speed_mps,mag_dbfs\n")

    # ---- configuration sent to the page
    def _recompute_cfg(self):
        a = self.args
        bin_mps = self.fs / self.n * MPS_PER_HZ
        nb = int(math.ceil(a.max_speed / bin_mps)) + 1
        nb = max(8, min(nb, self.n // 2))
        changed = nb != self.nb
        self.nb = nb
        self.cfg = dict(bins=nb, bin_mps=bin_mps, max_mps=a.max_speed, arm_ms=int(a.arm_seconds * 1000),
                        floor_db=a.scope_range[0], ceil_db=a.scope_range[1], demo=bool(a.demo))
        return changed

    def hello(self):
        with self.lock:
            shots = list(self.shots)
        return dict(type="hello", cfg=self.cfg, shots=shots, link=self.link, text=self.link_text)

    def set_link(self, state, text=""):
        self.link, self.link_text = state, text
        self.hub.publish(dict(type="link", state=state, text=text or None))

    # ---- commands from the page
    def command(self, cmd):
        if self.args.demo:
            if self.demo_cmd:
                self.demo_cmd(cmd)
            return True
        conn = self.conn
        if conn is None:
            return False
        try:
            conn.sendall(cmd.encode() + b"\n")
            return True
        except OSError:
            return False

    # ---- radar packets
    def on_packet(self, m, c):
        if m["fs"] != self.fs or m["n"] != self.n:
            self.fs, self.n = float(m["fs"]), int(m["n"])
            self.tracker.block_s = self.n / self.fs
            if self._recompute_cfg():
                self.maxhold = np.full(self.nb, -200.0)
            self.hub.publish(self.hello())
        shot = self.tracker.update(m)
        if shot:
            self._record_shot(shot, m)
        db = 20.0 * np.log10(np.maximum(np.abs(c[:self.nb]), 1e-9))
        np.maximum(self.maxhold, db, out=self.maxhold)
        self.armed = bool(m["flags"] & FLAG_ARMED)
        self.ms_left = int(m["armed_ms"])
        if m["flags"] & FLAG_TRACKING and (self.track_best is None or m["track_mag"] > self.track_best[0]):
            self.track_best = (m["track_mag"], m["track_speed"])
        now = time.time()
        if now - self.last_flush >= FRAME_S:
            self.flush(now)

    def flush(self, now=None):
        lo, hi = self.cfg["floor_db"], self.cfg["ceil_db"]
        col = np.clip((self.maxhold - lo) / (hi - lo) * 255.0, 0, 255).astype(int).tolist()
        self.hub.publish(dict(type="frame", armed=self.armed, ms_left=self.ms_left,
                              speed=None if self.track_best is None else round(float(self.track_best[1]), 2), col=col))
        self.maxhold[:] = -200.0
        self.track_best = None
        self.last_flush = now or time.time()

    def _record_shot(self, shot, m):
        with self.lock:
            sid = len(self.shots) + 1
            s = dict(type="shot", id=sid, t=time.time(), speed=round(shot["speed"], 3), freq=round(shot["freq"], 1),
                     dbfs=round(shot["dbfs"], 1), dur_ms=round(shot["dur_ms"], 1))
            self.shots.append({k: v for k, v in s.items() if k != "type"})
        print(f"SHOT #{sid}: {shot['speed']:.1f} m/s = {shot['speed'] * 2.236936:.1f} mph = "
              f"{shot['speed'] * 3.28084:.0f} ft/s  ({shot['freq']:.0f} Hz, {shot['dbfs']:.1f} dBFS, "
              f"{shot['blocks']} blocks)")
        if self.logf:
            self.logf.write(f"{time.time():.3f},{sid},{shot['seq']},{shot['freq']:.1f},{shot['speed']:.2f},"
                            f"{shot['dbfs']:.2f}\n")
            self.logf.flush()
        self.hub.publish(s)

    def disarmed_or_lost(self):
        self.tracker.reset()
        self.armed, self.ms_left, self.track_best = False, 0, None


def make_handler(show):
    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *a):                         # keep the console for radar messages
            pass

        def _send(self, code, body=b"", ctype="text/plain; charset=utf-8"):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            if body:
                self.wfile.write(body)

        def do_GET(self):
            path = urlparse(self.path).path
            if path in ("/", "/index.html"):
                try:
                    self._send(200, HTML.read_bytes(), "text/html; charset=utf-8")
                except OSError:
                    self._send(500, b"showcase.html is missing next to showcase.py")
            elif path == "/events":
                self._events()
            else:
                self._send(404, b"not found")

        def do_POST(self):
            u = urlparse(self.path)
            if u.path != "/cmd":
                return self._send(404, b"not found")
            cmd = (parse_qs(u.query).get("c") or [""])[0]
            if cmd not in ("arm", "disarm", "status", "fire"):
                return self._send(400, b"unknown command")
            ok = show.command(cmd)
            self._send(200 if ok else 503, b"ok" if ok else b"radar not connected")

        def _events(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Connection", "keep-alive")
            self.end_headers()
            q = show.hub.add()
            try:
                self.wfile.write(b"retry: 1000\n\n")
                self.wfile.write(b"data: " + json.dumps(show.hello(), separators=(",", ":")).encode() + b"\n\n")
                self.wfile.flush()
                while True:
                    try:
                        data = q.get(timeout=10)
                        self.wfile.write(b"data: " + data.encode() + b"\n\n")
                    except queue.Empty:
                        self.wfile.write(b": keepalive\n\n")
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            finally:
                show.hub.remove(q)

    return Handler


# ----------------------------------------------------------------------------- demo (simulated radar)
class Demo:
    """Generates RAD3-like packets in real time so the page can be rehearsed without hardware.
    While armed it fires a simulated dart every few seconds (and sometimes none, to show the timeout);
    press X in the page to fire one at once."""

    def __init__(self, show):
        self.show = show
        self.rng = np.random.default_rng()
        self.armed_until = 0.0
        self.fire_at = None
        self.dart = None                                   # dict(blocks_left, speed, mag)
        self.bullets = 0
        self.seq = 0
        self._started = False
        show.demo_cmd = self.command

    def command(self, cmd):
        now = time.time()
        if cmd == "arm":
            self.armed_until = now + self.show.args.arm_seconds
            self.fire_at = now + random.uniform(1.5, 4.5) if random.random() < 0.85 else None
        elif cmd == "disarm":
            self.armed_until, self.fire_at, self.dart = 0.0, None, None
        elif cmd == "fire" and now < self.armed_until:
            self.fire_at = now

    def packet(self):
        s = self.show
        n, fs, nbins = 1024, 250000.0, 512
        now = time.time()
        armed = now < self.armed_until
        if not armed and self.armed_until:
            self.armed_until, self.fire_at, self.dart = 0.0, None, None
        if armed and self.fire_at is not None and now >= self.fire_at and self.dart is None:
            self.dart = dict(left=random.randint(2, 4), speed=random.uniform(13, 34), mag=10 ** (random.uniform(-58, -46) / 20))
            self.fire_at = None
        c = (self.rng.normal(size=nbins) + 1j * self.rng.normal(size=nbins)) * 10 ** (-102 / 20)
        c[:4] *= 3                                           # DC residue
        flags = FLAG_ARMED if armed else 0
        tf = tm = ts = 0.0
        if self.dart:
            d = self.dart
            f = d["speed"] / MPS_PER_HZ
            k0 = f / (fs / n)
            for k in range(max(0, int(k0) - 3), min(nbins, int(k0) + 5)):
                c[k] += d["mag"] * math.exp(-0.5 * (k - k0) ** 2 / 1.3) * np.exp(1j * self.rng.uniform(0, 6.28))
            flags |= FLAG_TRACKING
            if not self._started:
                flags |= FLAG_NEW_BULLET
                self.bullets += 1
                self._started = True
            tf, tm, ts = f, d["mag"], d["speed"]
            d["speed"] *= 0.993
            d["left"] -= 1
            if d["left"] <= 0:
                self.dart, self._started = None, False
        left_ms = max(0, int((self.armed_until - now) * 1000)) if armed else 0
        m = dict(seq=self.seq, n=n, fs=fs, flags=flags, bullets=self.bullets, track_freq=tf, track_mag=tm,
                 track_speed=ts, armed_ms=left_ms)
        self.seq += 1
        return m, c

    def run(self):
        s = self.show
        s.set_link("connected", "Simulated radar")
        block = 1024 / 250000.0
        t0, produced = time.time(), 0
        while True:
            due = int((time.time() - t0) / block)
            while produced < due:
                m, c = self.packet()
                s.on_packet(m, c)
                produced += 1
            time.sleep(0.002)


# ----------------------------------------------------------------------------- entry point
def run(args, make_link):
    import receiver_plot as rp                              # shares the packet parser and the serial/TCP link code

    show = Show(args)
    httpd = ThreadingHTTPServer((args.web_host, args.web_port), make_handler(show))
    httpd.daemon_threads = True
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    shown = "localhost" if args.web_host in ("127.0.0.1", "0.0.0.0") else args.web_host
    url = f"http://{shown}:{args.web_port}/"
    print(f"showcase page: {url}   (press F11 or F in the page for full screen)")
    if not args.no_browser:
        threading.Timer(0.8, lambda: webbrowser.open(url)).start()

    try:
        if args.demo:
            print("DEMO MODE: the radar is simulated. Arm it in the page; a dart is fired for you a few seconds later.")
            Demo(show).run()
            return
        srv, open_source = make_link(args)
        use_serial = srv is None
        while True:
            show.set_link("waiting")
            conn, desc = open_source()
            print("connected:", desc)
            show.conn = conn
            show.set_link("connected", None)
            rd = rp.Reader(conn)
            q = queue.Queue()
            stop = threading.Event()
            threading.Thread(target=rp.reader_thread, args=(rd, q, stop), daemon=True).start()
            if use_serial:
                show.command("stream usb")
            for c in args.cmd:
                show.command(c)
            if args.arm:
                show.command("arm")
            got, last_cmd = 0, time.time()
            try:
                while True:
                    try:
                        items = [q.get(timeout=0.1)]
                        while True:
                            items.append(q.get_nowait())
                    except queue.Empty:
                        pass
                    for kind, payload in items:
                        if kind == "end":
                            raise ConnectionError(payload)
                        m, c = payload
                        if args.sample_rate:
                            m["fs"] = args.sample_rate
                        got += 1
                        show.on_packet(m, c)
                    if use_serial and got == 0 and time.time() - last_cmd > 1.0:
                        last_cmd = time.time()
                        show.command("stream usb")
            except (ConnectionError, ValueError) as e:
                print("connection ended:", e)
            finally:
                stop.set()
                show.conn = None
                conn.close()
                show.disarmed_or_lost()
                show.set_link("lost")
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        httpd.shutdown()
