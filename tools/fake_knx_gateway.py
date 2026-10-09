#!/usr/bin/env python3
"""
Virtueller KNXnet/IP-Tunneling-Server mit einem simulierten Schaltaktor.
Zum Testen des Displays ohne echte KNX-Installation.

  - akzeptiert CONNECT / CONNECTIONSTATE / DISCONNECT / TUNNELLING_REQUEST
  - Schreibzugriff auf --switch schaltet die virtuelle Lampe und meldet den Zustand
    auf --status zurueck (wie ein Aktor mit Rueckmeldeobjekt)
  - GroupValueRead auf --status wird mit GroupValueResponse beantwortet
  - --toggle-after N: simuliert nach N s einen Wandtaster (Lampe wird extern geschaltet)

Aufruf: tools/fake_knx_gateway.py [--port 3671] [--switch 1/0/1] [--status 1/0/2]
"""
import argparse
import socket
import struct
import sys
import time


def ga(s):
    a, b, c = (int(x) for x in s.split("/"))
    return (a << 11) | (b << 8) | c


def ga_str(v):
    return f"{v >> 11}/{(v >> 8) & 7}/{v & 0xFF}"


def hdr(svc, total):
    return struct.pack(">BBHH", 0x06, 0x10, svc, total)


def cemi_write(dst, value, msg=0x29):
    # L_Data.ind, Quelle 1.1.5, 1-Bit-Wert
    return bytes([msg, 0x00, 0xBC, 0xE0, 0x11, 0x05]) + struct.pack(">H", dst) + bytes([0x01, 0x00, 0x80 | value])


def cemi_response(dst, value):
    return bytes([0x29, 0x00, 0xBC, 0xE0, 0x11, 0x05]) + struct.pack(">H", dst) + bytes([0x01, 0x00, 0x40 | value])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=3671)
    ap.add_argument("--switch", default="1/0/1")
    ap.add_argument("--status", default="1/0/2")
    ap.add_argument("--toggle-after", type=float, default=0)
    args = ap.parse_args()
    sw, st = ga(args.switch), ga(args.status)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", args.port))
    sock.settimeout(0.2)
    print(f"[gw] lausche auf UDP {args.port}, Schalten={args.switch} Rueckmeldung={args.status}", flush=True)

    lamp = 0
    client = None
    channel = 1
    seq_out = 0
    seq_in_expected = 0
    t0 = time.time()
    toggled = False

    def send_ind(cemi):
        nonlocal seq_out
        if not client:
            return
        body = bytes([0x04, channel, seq_out, 0x00]) + cemi
        sock.sendto(hdr(0x0420, 6 + len(body)) + body, client)
        seq_out = (seq_out + 1) & 0xFF

    while True:
        if args.toggle_after and not toggled and client and time.time() - t0 > args.toggle_after:
            toggled = True
            lamp ^= 1
            print(f"[gw] WANDTASTER: Lampe -> {lamp}", flush=True)
            send_ind(cemi_write(sw, lamp))
            send_ind(cemi_write(st, lamp))
        try:
            data, addr = sock.recvfrom(512)
        except socket.timeout:
            continue
        if len(data) < 6:
            continue
        svc = struct.unpack(">H", data[2:4])[0]
        if svc == 0x0205:  # CONNECT_REQUEST
            client = addr
            seq_out = 0
            seq_in_expected = 0
            body = bytes([channel, 0x00]) + bytes([8, 1, 127, 0, 0, 1]) + struct.pack(">H", args.port) + bytes([4, 4, 0x11, 0xFA])
            sock.sendto(hdr(0x0206, 6 + len(body)) + body, addr)
            print(f"[gw] CONNECT von {addr}", flush=True)
        elif svc == 0x0207:  # CONNECTIONSTATE_REQUEST
            sock.sendto(hdr(0x0208, 8) + bytes([channel, 0x00]), addr)
            print("[gw] Heartbeat", flush=True)
        elif svc == 0x0209:  # DISCONNECT_REQUEST
            sock.sendto(hdr(0x020A, 8) + bytes([channel, 0x00]), addr)
            print("[gw] DISCONNECT", flush=True)
            client = None
        elif svc == 0x0420:  # TUNNELLING_REQUEST
            seq = data[8]
            sock.sendto(hdr(0x0421, 10) + bytes([4, channel, seq, 0]), addr)
            if seq != seq_in_expected:
                continue
            seq_in_expected = (seq_in_expected + 1) & 0xFF
            c = data[10:]
            if c[0] != 0x11:
                continue
            dst = struct.unpack(">H", c[6:8])[0]
            apci = ((c[9] & 3) << 2) | (c[10] >> 6)
            val = c[10] & 0x3F
            kind = {0: "READ", 1: "RESPONSE", 2: "WRITE"}.get(apci, "?")
            print(f"[gw] RX {kind} {ga_str(dst)} = {val if apci else '-'}", flush=True)
            if apci == 2 and dst == sw:
                lamp = val & 1
                print(f"[gw] LAMPE -> {'AN' if lamp else 'AUS'}", flush=True)
                send_ind(cemi_write(st, lamp))
            elif apci == 0 and dst == st:
                send_ind(cemi_response(st, lamp))


if __name__ == "__main__":
    sys.exit(main())
