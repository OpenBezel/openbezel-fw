#!/usr/bin/env python3
"""rtdmon - talk to the OpenBezel firmware debug monitor over DDC.

Channel B (mailbox at DDC 0x4A regs 0x80-0x9F) requires OpenBezel firmware
with the monitor running; Channel A commands (autopsy, sreg) work on any
firmware state, including dead firmware, because they use the hardware
scaler-register window.

Examples:
  rtdmon.py --bus 9 ping
  rtdmon.py --bus 9 status
  rtdmon.py --bus 9 watch
  rtdmon.py --bus 9 peek 0xD700 32
  rtdmon.py --bus 9 poke 0xFF85 0x12
  rtdmon.py --bus 9 sreg 0x3A02          # read a scaler reg (Channel A)
  rtdmon.py --bus 9 autopsy pages.txt    # full scaler snapshot to file
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rtdlib import Bus, XFR, Scaler
from rtdlib.monitor import Monitor


def hexdump(addr, data):
    for i in range(0, len(data), 16):
        row = data[i:i + 16]
        hexes = " ".join(f"{b:02X}" for b in row)
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        print(f"{addr + i:04X}: {hexes:<47} {text}")


def write_snapshot(scaler, f, pages=range(256)):
    for p in pages:
        data = scaler.read_page(p)
        f.write(f"Page {p:02X}\n")
        for i in range(0, 256, 16):
            f.write(" ".join(f"{b:02X}" for b in data[i:i + 16]) + "\n")


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--bus", type=int,
                   default=int(os.environ.get("RTD_I2C_BUS", -1)))
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("ping", help="is an OpenBezel monitor alive?")
    sub.add_parser("status", help="magic/heartbeat/checkpoint/key polls")
    w = sub.add_parser("watch", help="poll status continuously")
    w.add_argument("--interval", type=float, default=0.5)
    pk = sub.add_parser("peek", help="read XDATA via the firmware")
    pk.add_argument("addr", type=lambda x: int(x, 0))
    pk.add_argument("length", type=lambda x: int(x, 0), nargs="?", default=16)
    po = sub.add_parser("poke", help="write XDATA via the firmware")
    po.add_argument("addr", type=lambda x: int(x, 0))
    po.add_argument("bytes", type=lambda x: int(x, 0), nargs="+")
    pi = sub.add_parser("peek-iram", help="read IRAM via the firmware")
    pi.add_argument("addr", type=lambda x: int(x, 0))
    pi.add_argument("length", type=lambda x: int(x, 0), nargs="?", default=16)
    pc = sub.add_parser("peek-code", help="read CODE via the firmware")
    pc.add_argument("addr", type=lambda x: int(x, 0))
    pc.add_argument("length", type=lambda x: int(x, 0), nargs="?", default=16)
    sub.add_parser("reset", help="ask the firmware to self-reset")
    sr = sub.add_parser("sreg", help="read scaler reg(s) via hardware window "
                                     "(works with dead firmware)")
    sr.add_argument("sreg", type=lambda x: int(x, 0), help="0xPPOO page:offset")
    sr.add_argument("count", type=lambda x: int(x, 0), nargs="?", default=1)
    sw = sub.add_parser("sreg-write", help="write a scaler reg via hardware window")
    sw.add_argument("sreg", type=lambda x: int(x, 0))
    sw.add_argument("bytes", type=lambda x: int(x, 0), nargs="+")
    au = sub.add_parser("autopsy", help="full 256-page scaler snapshot to file")
    au.add_argument("file")

    args = p.parse_args()
    if args.bus < 0:
        p.error("--bus is required (or set RTD_I2C_BUS)")

    bus = Bus(args.bus)
    mon = Monitor(bus)
    xfr = XFR(bus)

    from rtdlib.monitor import MonitorError
    try:
        run_command(args, mon, xfr)
    except MonitorError as e:
        print(f"error: {e}")
        sys.exit(1)


def run_command(args, mon, xfr):
    if args.cmd == "ping":
        st = mon.status()
        if st["magic"] == 0x5A:
            rate = mon.heartbeat_rate()
            print(f"alive: heartbeat {rate:.0f}/s, "
                  f"checkpoint {st['checkpoint']}, ui {st['ui_mode']}")
        else:
            print(f"no monitor (magic=0x{st['magic']:02X})")
            sys.exit(1)
    elif args.cmd == "status":
        for k, v in mon.status().items():
            print(f"{k}: 0x{v:02X}")
    elif args.cmd == "watch":
        while True:
            st = mon.status()
            print(f"\rhb={st['heartbeat']:3d} cp={st['checkpoint']:3d} "
                  f"polls={st['key_polls']:3d} minA0=0x{st['key_min_a0']:02X} ui={st['ui_mode']}  ",
                  end="", flush=True)
            time.sleep(args.interval)
    elif args.cmd == "peek":
        hexdump(args.addr, mon.peek_xdata(args.addr, args.length))
    elif args.cmd == "poke":
        mon.poke_xdata(args.addr, args.bytes)
    elif args.cmd == "peek-iram":
        hexdump(args.addr, mon.peek_iram(args.addr, args.length))
    elif args.cmd == "peek-code":
        hexdump(args.addr, mon.peek_code(args.addr, args.length))
    elif args.cmd == "reset":
        mon.reset()
        print("reset requested")
    elif args.cmd == "sreg":
        data = Scaler(xfr).read(args.sreg, args.count)
        hexdump(args.sreg, bytes(data))
    elif args.cmd == "sreg-write":
        Scaler(xfr).write(args.sreg, args.bytes)
    elif args.cmd == "autopsy":
        with open(args.file, "w") as f:
            write_snapshot(Scaler(xfr), f)
        print(f"snapshot written to {args.file}")


if __name__ == "__main__":
    main()
