#!/usr/bin/env python3
"""Receive RAD3 packets from the Teensy 4.1, plot them live and log bullets.

Two ways to connect (same data, same plots):
  Ethernet (default): run this first, it listens; the Teensy connects to it
      python receiver_plot.py --port 5000
  USB serial: read straight from the Teensy plugged into this laptop (no Ethernet needed)
      python receiver_plot.py --serial auto          # finds the Teensy (USB vendor 0x16C0)
      python receiver_plot.py --serial COM5          # or /dev/ttyACM0, /dev/cu.usbmodem...
      python receiver_plot.py --list-ports
  USB mode needs pyserial (pip install pyserial) and firmware with RADAR_OUT_USB 1 (default).
  The tool sends "stream usb" to the Teensy, which then sends packets instead of readable
  text; close this tool (or the port) and it goes back to text for the serial monitor.
  Close any other program holding the serial port (Arduino/PlatformIO monitor) first.

Plots (the waveform panel needs firmware RADAR_DEBUG_WAVEFORM 1, the default):
  1. Incoming Doppler waveform (A10), raw volts at the pin; blue shading = device armed
  2. Spectrum of the latest block (dBFS) with the tracked bullet marked

Zoom / view controls (click the plot window first):
  mouse wheel, + / - (or up / down)   zoom the time axis of panel 1 (keeps the newest sample at the right edge)
  p or space                          pause / resume the view (data keeps being received)
  left / right                        pan back / forward in time (pauses automatically)
  mouse wheel while paused            zooms around the mouse pointer
  0 (or home)                         reset zoom and go live again
  Start with a different window using --history-ms (e.g. --history-ms 2 shows ~2 ms = ~500 samples).
  Zoomed in far enough the individual samples are drawn as dots, so every period of a
  high-frequency signal can be made out.

Showcase mode: add --showcase for a full-screen visualizer in the browser (speed readout, dart replay,
leaderboard, live "what the radar hears" chart); --showcase --demo needs no hardware. See the README.

Arming: the device starts disarmed. Press  a  to arm (pin 10 goes LOW for 10 s, then it
disarms itself),  d  to disarm,  s  for a status line. --arm arms as soon as the Teensy connects.

Other options: --buffer-ms (how much waveform history is kept, default 2000), --record FILE.npz
(save raw waveforms for offline analysis), --log FILE.csv (one line per detected bullet),
--save PNG (headless).
"""
import argparse, collections, os, queue, socket, struct, sys, threading, time, warnings
import numpy as np

HDR_BODY = struct.Struct("<IIIIIfffI")   # the 36 bytes after the 4-byte magic "RAD3"
WAVE = struct.Struct("<B3x")             # adc_bits + 3 reserved bytes
FLAG_ARMED, FLAG_TRACKING, FLAG_NEW_BULLET, FLAG_HAS_WAVE = 1, 2, 4, 16
VREF = 3.3


class SerialConn:
    """Socket-like wrapper around a pyserial port (recv / sendall / close)."""

    def __init__(self, ser):
        self.ser = ser
        self.last_data = time.time()
        self.warned = False

    def recv(self, n):
        import serial
        while True:
            try:
                want = max(1, min(n, self.ser.in_waiting))
                data = self.ser.read(want)
            except (serial.SerialException, OSError) as e:
                raise ConnectionError(f"serial port error: {e}")
            if data:
                self.last_data = time.time()
                self.warned = False
                return data
            if not self.warned and time.time() - self.last_data > 3.0:
                print("waiting for data from the Teensy ... (is the firmware running, "
                      "RADAR_OUT_USB 1, and no other program holding the port?)")
                self.warned = True

    def sendall(self, data):
        self.ser.write(data)
        self.ser.flush()

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass


class Reader:
    """Exact-length reads on top of a socket-like object, with push-back."""

    def __init__(self, raw):
        self.raw = raw
        self.buf = bytearray()

    def read(self, n):
        while len(self.buf) < n:
            chunk = self.raw.recv(max(n - len(self.buf), 4096))
            if not chunk:
                raise ConnectionError("closed")
            self.buf += chunk
        out = bytes(self.buf[:n])
        del self.buf[:n]
        return out

    def unread(self, data):
        self.buf[0:0] = data


def read_packet(rd):
    """Next binary packet. Text lines from the Teensy ('#...') are printed; stray bytes
    (e.g. readable text sent before USB streaming started) are skipped to re-synchronise."""
    skipped = bytearray()
    while True:
        b = rd.read(1)
        if b == b"#":
            line = bytearray()
            while True:
                ch = rd.read(1)
                if ch == b"\n":
                    break
                if len(line) < 400:
                    line += ch
            print("teensy:", line.decode(errors="replace"))
            continue
        if b == b"R":
            rest = rd.read(3)
            if rest == b"AD3":
                break
            rd.unread(rest)                      # not a header after all; rescan these bytes
        skipped += b
        if b == b"\n":
            txt = skipped.decode(errors="replace").strip()
            if txt and sum(c.isprintable() for c in txt) >= 0.9 * len(txt):
                print("teensy (text):", txt)
            skipped.clear()
        elif len(skipped) > 400:
            skipped.clear()
    (seq, n, fs, flags, bullets, tf, tm, ts, armed_ms) = HDR_BODY.unpack(rd.read(HDR_BODY.size))
    if n == 0 or n > 65536:
        raise ValueError(f"bad fft size {n}")
    sp = np.frombuffer(rd.read((n // 2) * 8), dtype="<f4")
    meta = dict(seq=seq, n=n, fs=fs, flags=flags, bullets=bullets,
                track_freq=tf, track_mag=tm, track_speed=ts, armed_ms=armed_ms)
    if flags & FLAG_HAS_WAVE:
        (bits,) = WAVE.unpack(rd.read(WAVE.size))
        raw = np.frombuffer(rd.read(n * 2), dtype="<u2")
        meta["sig"] = raw * (VREF / ((1 << bits) - 1))      # volts at the pin
    return meta, sp[0::2] + 1j * sp[1::2]


def reader_thread(rd, q, stop):
    """Parse packets continuously so a slow plot never lets the link buffers overflow."""
    try:
        while not stop.is_set():
            q.put(("pkt", read_packet(rd)))
    except (ConnectionError, ValueError, OSError) as e:
        q.put(("end", str(e)))


def print_local_addresses(expect="192.168.77.100"):
    """Ethernet mode helper: show this PC's IPv4 addresses. The Teensy connects to NET_DEST_IP
    (default 192.168.77.100), so one interface must have exactly that address."""
    import subprocess
    addrs = []
    try:
        out = subprocess.run(["ip", "-4", "-o", "addr", "show"], capture_output=True, text=True, timeout=3).stdout
        for line in out.splitlines():
            f = line.split()
            if len(f) >= 4 and f[2] == "inet":
                addrs.append((f[1], f[3]))
    except (OSError, subprocess.SubprocessError):
        return
    if not addrs:
        return
    print("this PC's IPv4 addresses: " + ", ".join(f"{n} {a}" for n, a in addrs))
    if not any(a.split("/")[0] == expect for _, a in addrs):
        print(f"WARNING: none of these is {expect}, the address the Teensy connects to (NET_DEST_IP in "
              f"radar_config.h). Give the wired interface that address, e.g.\n"
              f"    sudo ip addr add {expect}/24 dev <interface>\n"
              f"or change NET_DEST_IP to one of the addresses above and reflash.")


def find_teensy_port():
    from serial.tools import list_ports
    ports = [p for p in list_ports.comports() if p.vid == 0x16C0]
    return ports[0].device if ports else None


def make_link(args):
    """Create the link to the Teensy for the chosen mode. Returns (listening socket or None, open_source)
    where open_source() blocks until a link is available and returns (raw connection, description)."""
    srv = None
    if args.serial:
        try:
            import serial  # noqa: F401
        except ImportError:
            sys.exit("USB mode needs pyserial:  pip install pyserial")
    else:
        srv = socket.socket()
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((args.host, args.port))
        srv.listen(1)
        print(f"listening on {args.host}:{args.port}")
        print_local_addresses()

    def open_source():
        if srv is not None:
            conn, addr = srv.accept()
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            return conn, f"Ethernet {addr[0]}:{addr[1]}"
        import serial
        announced = False
        while True:
            port = args.serial
            if port == "auto":
                port = find_teensy_port()
                if port is None:
                    if not announced:
                        print("no Teensy found yet (python receiver_plot.py --list-ports shows what is "
                              "connected); waiting ...")
                        announced = True
                    time.sleep(1.0)
                    continue
            try:
                ser = serial.Serial(port, args.baud, timeout=0.2)
                return SerialConn(ser), f"USB serial {port}"
            except (serial.SerialException, OSError) as e:
                if not announced:
                    print(f"cannot open {port}: {e}  (is another program using it?) retrying ...")
                    announced = True
                time.sleep(1.0)

    return srv, open_source


def decimate_minmax(t, y, maxpts=6000):
    """Keep the min and max of each chunk so peaks survive when many samples are shown."""
    n = len(t)
    if n <= maxpts:
        return t, y
    k = int(np.ceil(n / (maxpts / 2)))
    m = n // k * k
    tt, yy = t[:m].reshape(-1, k), y[:m].reshape(-1, k)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        lo, hi = np.nanmin(yy, axis=1), np.nanmax(yy, axis=1)
    return np.repeat(tt.mean(axis=1), 2), np.column_stack((lo, hi)).ravel()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="0.0.0.0", help="Ethernet mode: address to listen on")
    ap.add_argument("--port", type=int, default=5000, help="Ethernet mode: TCP port to listen on")
    ap.add_argument("--serial", nargs="?", const="auto", metavar="PORT",
                    help="read from the Teensy over USB serial instead of Ethernet "
                         "(PORT like COM5 or /dev/ttyACM0, or 'auto' to find it)")
    ap.add_argument("--baud", type=int, default=115200, help="ignored by Teensy USB, but pyserial needs one")
    ap.add_argument("--list-ports", action="store_true", help="list serial ports and exit")
    ap.add_argument("--sample-rate", type=float, default=None, help="override header value")
    ap.add_argument("--history-ms", type=float, default=250.0,
                    help="initial width of the waveform window in ms (zoom live with wheel / + / -)")
    ap.add_argument("--buffer-ms", type=float, default=2000.0, help="waveform history kept for zoom-out/pan")
    ap.add_argument("--save", metavar="PNG", help="headless: save a frame to PNG and exit")
    ap.add_argument("--save-after", type=int, default=1, help="with --save: packets to receive first")
    ap.add_argument("--log", metavar="CSV", help="append one line per detected bullet")
    ap.add_argument("--record", metavar="NPZ", help="save raw waveforms + flags when the stream ends")
    ap.add_argument("--arm", action="store_true", help="send the arm command when the Teensy connects")
    ap.add_argument("--floor", type=float, default=-110.0, help="spectrum floor in dBFS")
    ap.add_argument("--cmd", action="append", default=[], metavar="TEXT",
                    help='send this command to the Teensy when it connects (repeatable), e.g. --cmd "band 1000 0"')
    ap.add_argument("--showcase", action="store_true",
                    help="show the full-screen showcase visualizer in a web browser instead of the engineering plots "
                         "(no matplotlib needed; see the README)")
    ap.add_argument("--demo", action="store_true", help="with --showcase: simulated radar, no hardware needed")
    ap.add_argument("--web-port", type=int, default=8080, help="showcase: web page port")
    ap.add_argument("--web-host", default="127.0.0.1",
                    help="showcase: address for the web page (127.0.0.1 = this computer only, 0.0.0.0 = whole network)")
    ap.add_argument("--no-browser", action="store_true", help="showcase: do not open the browser automatically")
    ap.add_argument("--min-speed", type=float, default=6.0, help="showcase: ignore detections slower than this (m/s)")
    ap.add_argument("--max-speed", type=float, default=45.0, help="showcase: top of the speed scale (m/s)")
    ap.add_argument("--arm-seconds", type=float, default=10.0, help="showcase: arm time of the firmware (ARM_DURATION_MS)")
    ap.add_argument("--scope-range", type=float, nargs=2, default=(-100.0, -40.0), metavar=("FLOOR", "CEIL"),
                    help="showcase: dBFS range mapped to the chart colours")
    args = ap.parse_args()

    if args.showcase:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import showcase
        return showcase.run(args, make_link)

    if args.list_ports:
        from serial.tools import list_ports
        for p in list_ports.comports():
            tag = "   <-- Teensy" if p.vid == 0x16C0 else ""
            print(f"{p.device}  {p.description}  vid:pid={p.vid and hex(p.vid)}:{p.pid and hex(p.pid)}{tag}")
        return

    import matplotlib
    if args.save:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    # Free the keys this tool uses (matplotlib binds s=save, p=pan, left/right=history, ...).
    for k in ("keymap.save", "keymap.pan", "keymap.back", "keymap.forward", "keymap.home",
              "keymap.zoom", "keymap.xscale", "keymap.yscale", "keymap.grid", "keymap.fullscreen"):
        if k in plt.rcParams:
            plt.rcParams[k] = []

    srv, open_source = make_link(args)

    fig = None
    # View state shared with the key / mouse handlers.
    state = dict(conn=None, window_ms=args.history_ms, offset_ms=0.0, paused=False, frozen=None,
                 dirty=False, history=collections.deque(), last_mc=None, fs=args.sample_rate or 250000.0, n=1024)

    def send_cmd(text):
        conn = state.get("conn")
        if conn is None:
            print("not connected")
            return
        try:
            conn.sendall(text.encode() + b"\n")
            print("sent:", text)
        except OSError as e:
            print("send failed:", e)

    # ---- zoom / pan / pause -------------------------------------------------
    def span_ms():
        blocks = state["frozen"][1] if state["paused"] and state["frozen"] else list(state["history"])
        return len(blocks) * state["n"] / state["fs"] * 1e3

    def clamp_view():
        min_w = 16.0 / state["fs"] * 1e3                     # never fewer than ~16 samples
        max_w = max(min_w, min(args.buffer_ms, max(span_ms(), min_w)))
        state["window_ms"] = float(min(max(state["window_ms"], min_w), max_w))
        state["offset_ms"] = float(min(max(state["offset_ms"], 0.0), max(0.0, span_ms() - state["window_ms"])))
        state["dirty"] = True

    def zoom(factor, anchor=None):
        old_w = state["window_ms"]
        state["window_ms"] = old_w * factor
        if state["paused"] and anchor is not None:           # keep the point under the mouse fixed
            hi = -state["offset_ms"]
            state["offset_ms"] = -(anchor + (hi - anchor) * factor)
        clamp_view()
        print(f"window {state['window_ms']:.3f} ms")

    def set_paused(p):
        if p and not state["paused"]:
            if state["last_mc"] is None:
                return
            state["frozen"] = (state["last_mc"], list(state["history"]))
            state["paused"] = True
        elif not p:
            state["paused"] = False
            state["frozen"] = None
            state["offset_ms"] = 0.0
        state["dirty"] = True

    def pan(frac):
        set_paused(True)
        if state["paused"]:
            state["offset_ms"] += frac * state["window_ms"]
            clamp_view()

    def on_key(event):
        k = event.key
        if k in ("+", "=", "up"):
            zoom(1 / 1.25)
        elif k in ("-", "_", "down"):
            zoom(1.25)
        elif k in ("p", " "):
            set_paused(not state["paused"])
        elif k == "left":
            pan(+0.25)
        elif k == "right":
            pan(-0.25)
        elif k in ("0", "home"):
            state["window_ms"] = args.history_ms
            set_paused(False)
            clamp_view()
        else:
            cmd = {"a": "arm", "d": "disarm", "s": "status"}.get(k)
            if cmd:
                send_cmd(cmd)

    def on_scroll(event):
        if event.inaxes is not state.get("a1"):
            return
        zoom(1 / 1.25 if event.step > 0 or event.button == "up" else 1.25, anchor=event.xdata)

    # ---- figure -------------------------------------------------------------
    def make_figure(debug):
        nonlocal fig
        state["debug"] = debug
        if debug:
            fig, (a1, a4) = plt.subplots(2, 1, figsize=(11, 8), constrained_layout=True,
                                         gridspec_kw=dict(height_ratios=[1.2, 1]))
            l1, = a1.plot([], [], lw=0.9, color="tab:blue")
            a1.set_ylabel("Volts at the pin")
            a1.set_xlabel("ms (0 = newest sample)   |   wheel or + / - zoom   p pause   left/right pan   0 reset")
            a1.grid(True)
            state.update(a1=a1, l1=l1, spans=[])
        else:
            fig, a4 = plt.subplots(figsize=(10, 4), constrained_layout=True)
            state["a1"] = None
        fig.canvas.mpl_connect("key_press_event", on_key)
        fig.canvas.mpl_connect("scroll_event", on_scroll)
        line, = a4.plot([], [], lw=1)
        mark, = a4.plot([], [], "rv", ms=9, label="tracked bullet")
        a4.set_xlabel("Frequency (Hz)")
        a4.set_ylabel("Magnitude (dBFS)")
        a4.set_ylim(args.floor, 6)
        a4.grid(True)
        a4.legend(loc="upper right")
        title = a4.set_title("waiting for data")
        state.update(a4=a4, line=line, mark=mark, title=title)
        if not args.save:
            plt.ion()
            plt.show(block=False)

    def redraw(m, c, blocks):
        st = state
        n, fs = m["n"], m["fs"]
        if st["debug"] and blocks:
            a1 = st["a1"]
            t_end = (blocks[-1]["seq"] + 1) * n
            x_hi = -st["offset_ms"]
            x_lo = x_hi - st["window_ms"]
            blk_ms = n / fs * 1e3
            vis = []
            for b in blocks:
                x0 = (b["seq"] * n - t_end) / fs * 1e3
                if x0 + blk_ms >= x_lo and x0 <= x_hi:
                    vis.append((x0, b))
            if vis:
                ts, ys, prev = [], [], None
                for x0, b in vis:
                    if prev is not None and b["seq"] != prev + 1:    # missing block(s): break the line
                        ts.append(np.array([np.nan])); ys.append(np.array([np.nan]))
                    prev = b["seq"]
                    ts.append(x0 + np.arange(n) / fs * 1e3)
                    ys.append(b["sig"])
                t, y = np.concatenate(ts), np.concatenate(ys)
                sel = (t >= x_lo) & (t <= x_hi)
                t, y = t[sel], y[sel]
                if len(t):
                    few = len(t) <= 600                               # show the samples themselves
                    st["l1"].set_marker("." if few else "")
                    st["l1"].set_markersize(3)
                    td, yd = decimate_minmax(t, y)
                    st["l1"].set_data(td, yd)
                    lo, hi = np.nanmin(y), np.nanmax(y)
                    pad = max(0.02, 0.1 * (hi - lo))
                    a1.set_ylim(lo - pad, hi + pad)
                a1.set_xlim(x_lo, x_hi)
                for art in st["spans"]:
                    art.remove()
                st["spans"] = []
                run = None                                            # merge neighbouring armed blocks
                for x0, b in vis + [(None, None)]:
                    armed = b is not None and (b["flags"] & FLAG_ARMED)
                    if armed and run is None:
                        run = [x0, x0 + blk_ms]
                    elif armed:
                        run[1] = x0 + blk_ms
                    elif run is not None:
                        st["spans"].append(a1.axvspan(run[0], run[1], color="tab:blue", alpha=0.10, lw=0))
                        run = None
            mode = (f"PAUSED, {st['offset_ms']:.1f} ms back" if st["paused"] else "LIVE")
            a1.set_title(f"1. Incoming Doppler waveform (A10), raw   window {st['window_ms']:.3g} ms   [{mode}]   "
                         f"(blue = armed, pin 10 low)")
        mag = 20 * np.log10(np.maximum(np.abs(c), 1e-9))
        freqs = np.arange(len(c)) * fs / n
        st["line"].set_data(freqs, mag)
        st["a4"].set_xlim(0, freqs[-1])
        if m["flags"] & FLAG_TRACKING:
            st["mark"].set_data([m["track_freq"]], [20 * np.log10(max(m["track_mag"], 1e-9))])
        else:
            st["mark"].set_data([], [])
        st["title"].set_text(f"2. Spectrum of the latest block   bullets {m['bullets']}  "
                             f"{m['track_speed']:.0f} m/s  "
                             + (f"ARMED {m['armed_ms'] / 1000:.1f} s left (pin 10 low)" if m["flags"] & FLAG_ARMED
                                else "disarmed (pin 10 high)  - press 'a' to arm"))

    logf = open(args.log, "a") if args.log else None
    if logf and logf.tell() == 0:
        logf.write("pc_time,bullet_count,seq,freq_hz,speed_mps,mag_dbfs\n")

    while True:
        conn, desc = open_source()
        print("connected:", desc)
        state["conn"] = conn
        rd = Reader(conn)
        q = queue.Queue()
        stop = threading.Event()
        threading.Thread(target=reader_thread, args=(rd, q, stop), daemon=True).start()
        use_serial = srv is None
        if use_serial:
            send_cmd("stream usb")          # ask the Teensy to send packets instead of text
        for c in args.cmd:
            send_cmd(c)
        if args.arm:
            send_cmd("arm")
        last_seq, got, t0, last_draw, last_stream_cmd = None, 0, time.time(), 0.0, time.time()
        history = state["history"] = collections.deque()
        state["last_mc"] = None
        state["paused"], state["frozen"], state["offset_ms"] = False, None, 0.0
        rec = collections.defaultdict(list)
        fs = args.sample_rate or 250000.0
        try:
            while True:
                items = []
                try:
                    items.append(q.get(timeout=0.05))
                    while True:
                        items.append(q.get_nowait())
                except queue.Empty:
                    pass

                new_data = False
                for kind, payload in items:
                    if kind == "end":
                        raise ConnectionError(payload)
                    m, c = payload
                    fs = args.sample_rate or m["fs"]
                    m["fs"] = fs
                    state["fs"], state["n"] = fs, m["n"]
                    if last_seq is not None and m["seq"] != last_seq + 1:
                        print(f"gap: {last_seq} -> {m['seq']} ({m['seq'] - last_seq - 1} blocks missing)")
                    last_seq = m["seq"]
                    got += 1
                    if "sig" in m:
                        history.append(m)
                        keep = int(args.buffer_ms * 1e-3 * fs / m["n"]) + 2
                        while len(history) > keep:
                            history.popleft()
                        if args.record:
                            rec["seq"].append(m["seq"]); rec["flags"].append(m["flags"])
                            rec["sig"].append(m["sig"])
                    if m["flags"] & FLAG_NEW_BULLET:
                        d = 20 * np.log10(max(m["track_mag"], 1e-9))
                        print(f"BULLET #{m['bullets']}: {m['track_speed']:.1f} m/s "
                              f"({m['track_speed'] * 3.28084:.0f} ft/s), {m['track_freq']:.0f} Hz, {d:.1f} dBFS")
                        if logf:
                            logf.write(f"{time.time():.3f},{m['bullets']},{m['seq']},"
                                       f"{m['track_freq']:.1f},{m['track_speed']:.2f},{d:.2f}\n")
                            logf.flush()
                    if got % 40 == 1:
                        mg = 20 * np.log10(np.maximum(np.abs(c), 1e-9))
                        k = int(np.argmax(mg[4:])) + 4
                        print(f"seq={m['seq']} strongest {k * fs / m['n']:.0f} Hz {mg[k]:.1f} dBFS "
                              f"({got / (time.time() - t0):.0f} blocks/s) bullets={m['bullets']}")
                    state["last_mc"] = (m, c)
                    new_data = True

                # USB mode: repeat the stream request until the first packet shows up
                if use_serial and got == 0 and time.time() - last_stream_cmd > 1.0:
                    last_stream_cmd = time.time()
                    conn.sendall(b"stream usb\n")

                if state["last_mc"] is not None:
                    m, c = state["last_mc"]
                    debug = "sig" in m
                    if fig is None or state.get("debug") != debug:
                        if fig is not None:
                            plt.close(fig)
                        make_figure(debug)
                    if args.save:
                        if got >= args.save_after:
                            clamp_view()
                            redraw(m, c, list(history))
                            fig.savefig(args.save, dpi=100)
                            print("saved", args.save)
                            return
                    elif (new_data and not state["paused"] or state["dirty"]) and time.time() - last_draw > 0.05:
                        if not plt.fignum_exists(fig.number):
                            return
                        last_draw = time.time()
                        if state["paused"] and state["frozen"]:
                            (fm, fc), fblocks = state["frozen"]
                            redraw(fm, fc, fblocks)
                        else:
                            redraw(m, c, list(history))
                        state["dirty"] = False
                        fig.canvas.draw_idle()
                if fig is not None and not args.save:
                    if not plt.fignum_exists(fig.number):
                        return
                    fig.canvas.flush_events()
        except (ConnectionError, ValueError) as e:
            print("connection ended:", e)
        finally:
            stop.set()
            state["conn"] = None
            conn.close()
            if args.record and rec["seq"]:
                np.savez_compressed(args.record, seq=np.array(rec["seq"]), flags=np.array(rec["flags"]),
                                    sig=np.array(rec["sig"]), sample_rate=fs)
                print(f"recorded {len(rec['seq'])} blocks to {args.record}")
                rec.clear()


if __name__ == "__main__":
    sys.exit(main())
