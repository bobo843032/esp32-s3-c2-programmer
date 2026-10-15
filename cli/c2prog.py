#!/usr/bin/env python3
"""c2prog - CLI för ESP32-C2-Programmer (Silicon Labs C2, C8051F32x).

Kommandon: ports | info | status | read | write | verify | erase
Läsning är standard. write/erase kräver --yes och gör alltid backup först (om flashen är läsbar).
Verktyget försöker aldrig kringgå lässkydd.
"""
import argparse
import datetime
import glob
import hashlib
import os
import sys
import time

CHUNK = 128
DEFAULT_BAUD = 460800


class C2Error(Exception):
    def __init__(self, code, msg=""):
        super().__init__(f"{code}: {msg}" if msg else code)
        self.code, self.msg = code, msg


def err(*a):
    print(*a, file=sys.stderr)


# ---------------------------------------------------------------- bildfiler
def parse_ihex(text):
    mem, ext = {}, 0
    for n, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise ValueError(f"HEX rad {n}: saknar ':'")
        try:
            raw = bytes.fromhex(line[1:])
        except ValueError:
            raise ValueError(f"HEX rad {n}: ogiltiga tecken")
        if len(raw) < 5 or len(raw) != raw[0] + 5:
            raise ValueError(f"HEX rad {n}: fel längd")
        if sum(raw) & 0xFF:
            raise ValueError(f"HEX rad {n}: felaktig checksumma")
        cnt, addr, typ, data = raw[0], (raw[1] << 8) | raw[2], raw[3], raw[4:4 + raw[0]]
        if typ == 0:
            for i, b in enumerate(data):
                mem[ext + ((addr + i) & 0xFFFF)] = b
        elif typ == 1:
            break
        elif typ == 2:
            ext = ((data[0] << 8) | data[1]) << 4
        elif typ == 4:
            ext = ((data[0] << 8) | data[1]) << 16
        elif typ in (3, 5):
            pass
        else:
            raise ValueError(f"HEX rad {n}: okänd posttyp {typ:02X}")
    return mem


def load_image(path, base=0):
    """Returnerar dict adress->byte."""
    with open(path, "rb") as f:
        data = f.read()
    is_hex = path.lower().endswith((".hex", ".ihx", ".ihex"))
    if not is_hex and data[:1] == b":":
        try:
            parse_ihex(data.decode("ascii"))
            is_hex = True
        except Exception:
            pass
    if is_hex:
        mem = parse_ihex(data.decode("ascii"))
    else:
        mem = {base + i: b for i, b in enumerate(data)}
    if not mem:
        raise ValueError("bildfilen är tom")
    return mem


# ---------------------------------------------------------------- seriekommunikation
def parse_kv(s):
    return dict(x.split("=", 1) for x in s.split() if "=" in x)


def find_ports():
    out = []
    for pat in ("/dev/cu.usbserial*", "/dev/cu.usbmodem*", "/dev/cu.SLAB_USBtoUART*", "/dev/cu.wchusbserial*"):
        out += sorted(glob.glob(pat))
    return out


class Programmer:
    def __init__(self, ser):
        self.ser = ser
        self.banner = {}

    @classmethod
    def open(cls, port=None, baud=DEFAULT_BAUD):
        try:
            import serial
        except ImportError:
            raise C2Error("NOPYSERIAL", "installera pyserial: pip3 install pyserial")
        port = port or os.environ.get("C2PROG_PORT")
        if not port:
            ports = find_ports()
            if len(ports) != 1:
                raise C2Error("NOPORT", "ange --port (hittade: %s)" % (", ".join(ports) or "inga"))
            port = ports[0]
        ser = serial.Serial()
        ser.port, ser.baudrate, ser.timeout = port, baud, 0.5
        ser.dtr = False
        ser.rts = False
        ser.open()
        p = cls(ser)
        try:
            p.handshake()
        except Exception:
            ser.close()
            raise
        return p

    def handshake(self):
        end = time.time() + 8          # ESP32 kan starta om när porten öppnas
        while time.time() < end:
            self.ser.reset_input_buffer()
            self.ser.write(b"PING\n")
            for _ in range(3):
                line = self.ser.readline().decode(errors="replace").strip()
                if line.startswith("OK c2prog 1 "):
                    self.banner = parse_kv(line)
                    try:
                        if not all(0 <= int(self.banner[k]) <= 48 for k in ("ck", "d")):
                            raise ValueError("ogiltiga GPIO-nummer")
                    except (KeyError, ValueError) as ex:
                        raise C2Error("BADREPLY", f"ogiltig PING-banner {line!r}: {ex}") from ex
                    return
                if line == "OK" or line.startswith("OK "):
                    raise C2Error("BADREPLY", f"oväntat PING-svar: {line!r}")
        raise C2Error("NODEVICE", "ingen respons från ESP32-C2-Programmer (rätt port/firmware/baudrate?)")

    def cmd(self, line, timeout=5.0):
        self.ser.reset_input_buffer()
        self.ser.write((line + "\n").encode())
        end = time.time() + timeout
        stale_banner = None
        while time.time() < end:
            raw = self.ser.readline()
            if not raw:
                continue
            s = raw.decode(errors="replace").strip()
            if not s:
                continue
            if s.startswith("OK c2prog 1 "):
                kv = parse_kv(s)
                try:
                    if not all(0 <= int(kv[k]) <= 48 for k in ("ck", "d")):
                        raise ValueError("ogiltiga GPIO-nummer")
                except (KeyError, ValueError) as ex:
                    raise C2Error("BADREPLY", f"ogiltig försenad PING-banner {s!r}: {ex}") from ex
                stale_banner = s
                continue                    # PING-svar får inte bli INFO-data
            if s == "OK" or s.startswith("OK "):
                return s[2:].strip()
            if s == "ERR" or s.startswith("ERR "):
                parts = s.split(None, 2)
                if len(parts) < 2:
                    raise C2Error("BADREPLY", f"ofullständigt felsvar: {s!r}")
                raise C2Error(parts[1], parts[2] if len(parts) > 2 else "")
            raise C2Error("BADREPLY", f"oväntat svar på {line.split()[0]}: {s!r}")
        if stale_banner is not None:
            raise C2Error("BADREPLY", f"bara försenat PING-svar, inget svar på {line.split()[0]}: {stale_banner!r}")
        raise C2Error("TIMEOUT", "inget svar från ESP32")

    def info(self):
        reply = self.cmd("INFO", 5)
        kv = parse_kv(reply)
        try:
            result = dict(dev=int(kv["dev"], 16), rev=int(kv["rev"], 16), name=kv["name"],
                          flash=int(kv["flash"]), page=int(kv["page"]), lock=int(kv["lock"], 16),
                          known=kv["known"] == "1")
            if (kv["known"] not in ("0", "1") or not result["name"]
                    or not 0 <= result["dev"] <= 255 or not 0 <= result["rev"] <= 255
                    or not 0 <= result["lock"] <= 65535
                    or result["flash"] < 0 or result["page"] < 0):
                raise ValueError("ogiltigt fältvärde")
            return result
        except (KeyError, ValueError) as ex:
            raise C2Error("BADREPLY", f"ogiltigt INFO-svar {reply!r}: {ex}") from ex

    def status(self):
        reply = self.cmd("STATUS", 30)
        kv = parse_kv(reply)
        try:
            return dict(pages=int(kv["pages"]), readable=int(kv["readable"]), mask=int(kv["mask"], 16),
                        lockbyte=None if kv["lock"] == "NA" else int(kv["lock"], 16))
        except (KeyError, ValueError) as ex:
            raise C2Error("BADREPLY", f"ogiltigt STATUS-svar {reply!r}: {ex}") from ex

    def read_block(self, addr, n):
        h = self.cmd(f"READ 0x{addr:04X} {n}", 5)
        b = bytes.fromhex(h)
        if len(b) != n:
            raise C2Error("BADREPLY", f"väntade {n} byte, fick {len(b)}")
        return b

    def read_range(self, start, length, label="Läser"):
        out = bytearray()
        while len(out) < length:
            n = min(CHUNK, length - len(out))
            out += self.read_block(start + len(out), n)
            progress(label, len(out), length)
        progress_done()
        return bytes(out)

    def arm(self):
        self.cmd("ARM")

    def disarm(self):
        self.cmd("DISARM")

    def erase_page(self, n):
        self.cmd(f"ERASE PAGE {n}", 5)

    def erase_all(self):
        self.cmd("ERASE ALL", 15)

    def write_block(self, addr, data):
        self.cmd(f"WRITE 0x{addr:04X} {data.hex().upper()}", 5)

    def reset(self):
        try:
            self.cmd("DISARM", 2)
            self.cmd("RESET", 3)
        except Exception:
            pass


def progress(label, done, total, unit="byte"):
    sys.stderr.write(f"\r{label}: {done}/{total} {unit} ({100 * done // total}%)")
    sys.stderr.flush()


def progress_done():
    sys.stderr.write("\n")


# ---------------------------------------------------------------- gemensamt
def sha(b):
    return hashlib.sha256(b).hexdigest()


def need_known(info):
    if not info["known"]:
        raise C2Error("UNKNOWN_TARGET", f"Device ID 0x{info['dev']:02X} finns inte i måltabellen. Avbryter.")


def readable_all(st):
    return st["readable"] == st["pages"]


def protection_text(info, st):
    if readable_all(st):
        lb = "okänd" if st["lockbyte"] is None else f"0x{st['lockbyte']:02X}"
        return f"Flash läsbar ({st['readable']}/{st['pages']} sidor). Låsbyte @0x{info['lock']:04X} = {lb} (tolka mot databladet)."
    return (f"Flash EJ fullt läsbar ({st['readable']}/{st['pages']} sidor). Troligen lässkyddad.\n"
            "  Verktyget försöker inte kringgå skyddet. För att återprogrammera krävs fullständig radering\n"
            "  (erase --all), vilket permanent raderar allt innehåll.")


def do_backup(p, info, outdir="backups"):
    data = p.read_range(0, info["flash"], "Backup")
    os.makedirs(outdir, exist_ok=True)
    ts = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    path = os.path.join(outdir, f"c2_{info['dev']:02X}_{info['rev']:02X}_{ts}.bin")
    with open(path, "wb") as f:
        f.write(data)
    print(f"Backup sparad: {path}  sha256={sha(data)[:16]}…")
    return data


# ---------------------------------------------------------------- kommandon
def cmd_ports(args, p=None):
    ports = find_ports()
    print("\n".join(ports) if ports else "Inga seriella portar hittades.")
    return 0


def cmd_info(args, p):
    i = p.info()
    if "ck" in p.banner:
        print(f"ESP32 pinnar: C2CK=GPIO{p.banner['ck']}  C2D=GPIO{p.banner['d']}")
    print(f"Device ID  : 0x{i['dev']:02X}")
    print(f"Revision ID: 0x{i['rev']:02X}")
    print(f"Krets      : {i['name']}" + ("" if i["known"] else "  (ej i måltabellen)"))
    if i["known"]:
        print(f"Flash      : {i['flash']} byte, sida {i['page']} byte, låsbyte @0x{i['lock']:04X}")
    return 0


def cmd_status(args, p):
    i = p.info()
    cmd_info(args, p)
    need_known(i)
    st = p.status()
    print(protection_text(i, st))
    return 0 if readable_all(st) else 3


def cmd_read(args, p):
    i = p.info()
    need_known(i)
    st = p.status()
    if not readable_all(st):
        print(protection_text(i, st))
        raise C2Error("PROTECTED", "kan inte läsa ut flash")
    start = args.start
    length = args.length if args.length else i["flash"] - start
    if start < 0 or start + length > i["flash"]:
        raise C2Error("RANGE", f"utanför flashen (0..0x{i['flash']:04X})")
    data = p.read_range(start, length)
    with open(args.output, "wb") as f:
        f.write(data)
    print(f"Sparade {len(data)} byte till {args.output}  sha256={sha(data)}")
    return 0


def cmd_verify(args, p):
    img = load_image(args.file, args.base)
    i = p.info()
    need_known(i)
    lo, hi = min(img), max(img)
    if hi >= i["flash"]:
        raise C2Error("RANGE", f"bilden når 0x{hi:04X}, flashen slutar vid 0x{i['flash']:04X}")
    dev = p.read_range(lo, hi - lo + 1, "Verifierar")
    bad = [a for a, b in sorted(img.items()) if dev[a - lo] != b]
    if bad:
        print(f"AVVIKELSE: {len(bad)} av {len(img)} byte skiljer sig. Första: " +
              ", ".join(f"0x{a:04X}" for a in bad[:10]))
        return 1
    print(f"OK: {len(img)} byte matchar bildfilen.")
    return 0


def cmd_write(args, p):
    img = load_image(args.file, args.base)
    i = p.info()
    need_known(i)
    psz, lock, flash = i["page"], i["lock"], i["flash"]
    if max(img) >= lock:
        raise C2Error("RANGE", f"bilden innehåller data på/över 0x{lock:04X} (låsbyte/reserverat). "
                               "Verktyget skriver aldrig dit.")
    pages = sorted({a // psz for a in img})
    print(f"Mål      : {i['name']} (Device ID 0x{i['dev']:02X}, rev 0x{i['rev']:02X})")
    print(f"Bild     : {len(img)} byte, 0x{min(img):04X}..0x{max(img):04X}")
    print(f"Sidor    : {len(pages)} st raderas och skrivs om ({', '.join(map(str, pages))})")
    st = p.status()
    if not readable_all(st):
        print(protection_text(i, st))
        raise C2Error("PROTECTED", "ingen backup möjlig och skrivning är inte tillåten på skyddad krets. "
                                   "Kör 'erase --all --yes' uttryckligen om du accepterar att allt raderas.")
    if st["lockbyte"] not in (None, 0xFF) and (lock // psz) in pages:
        raise C2Error("LOCKBYTE", f"låsbyten är 0x{st['lockbyte']:02X} och bilden rör låssidan. Avbryter.")
    if not args.yes:
        print("\nTorrkörning: inget har skrivits. Lägg till --yes för att backa upp och skriva.")
        return 0

    backup = do_backup(p, i)
    merged = {}
    for pg in pages:
        base = pg * psz
        buf = bytearray(backup[base:base + psz])
        for k in range(len(buf)):
            if base + k in img:
                buf[k] = img[base + k]
        if base <= lock < base + psz:
            buf[lock - base] = 0xFF            # låsbyten skrivs aldrig
        merged[pg] = bytes(buf)

    p.arm()
    total = len(pages)
    for n, pg in enumerate(pages, 1):
        progress("Raderar", n, total, "sidor")
        p.erase_page(pg)
    progress_done()
    written = 0
    for n, pg in enumerate(pages, 1):
        base = pg * psz
        for off in range(0, psz, CHUNK):
            end = min(off + CHUNK, psz)
            if base + end > lock:
                end = max(off, lock - base)
            chunk = merged[pg][off:end]
            if chunk and any(b != 0xFF for b in chunk):
                p.write_block(base + off, chunk)
                written += len(chunk)
        progress("Skriver", n, total, "sidor")
    progress_done()
    p.disarm()

    bad = 0
    for pg in pages:
        back = p.read_range(pg * psz, psz, f"Verifierar sida {pg}")
        if back != merged[pg]:
            bad += sum(1 for x, y in zip(back, merged[pg]) if x != y)
    if bad:
        raise C2Error("VERIFY", f"{bad} byte avviker vid återläsning. Backup finns kvar i backups/.")
    print(f"Klart: {written} byte skrivna och verifierade. Målet startas om.")
    return 0


def cmd_erase(args, p):
    if args.all == (args.page is not None):
        raise C2Error("USAGE", "ange antingen --page N eller --all")
    i = p.info()
    need_known(i)
    pages = i["flash"] // i["page"]
    if args.page is not None and not (0 <= args.page < pages):
        raise C2Error("RANGE", f"sida 0..{pages - 1}")
    what = "HELA flashen (inkl. lås)" if args.all else f"sida {args.page}"
    if not args.yes:
        print(f"Torrkörning: skulle radera {what}. Lägg till --yes för att utföra.")
        return 0
    st = p.status()
    if readable_all(st):
        do_backup(p, i)
    else:
        print(protection_text(i, st))
        print("INGEN backup är möjlig. Innehållet går förlorat för alltid.")
    if args.all:
        print("Fullständig radering tar bort allt, inklusive originalfirmware. Du behöver en egen bildfil för att få kortet att fungera igen.")
        if input("Skriv ERASE för att fortsätta: ").strip() != "ERASE":
            print("Avbrutet.")
            return 1
    p.arm()
    if args.all:
        p.erase_all()
        p.reset()
        i = p.info()
        data = p.read_range(0, i["flash"], "Kontrollerar")
        if any(b != 0xFF for b in data):
            raise C2Error("VERIFY", "flashen är inte helt tom efter radering")
        print("Fullständig radering klar och verifierad (allt 0xFF).")
    else:
        p.erase_page(args.page)
        base = args.page * i["page"]
        data = p.read_range(base, i["page"], "Kontrollerar")
        if any(b != 0xFF for b in data):
            raise C2Error("VERIFY", "sidan är inte tom efter radering")
        print(f"Sida {args.page} raderad och verifierad.")
    return 0


# ---------------------------------------------------------------- main
def main(argv=None):
    ap = argparse.ArgumentParser(prog="c2prog", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-p", "--port", help="seriell port (annars C2PROG_PORT eller automatiskt)")
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("ports", help="lista seriella portar")
    sub.add_parser("info", help="Device ID, revision, krets")
    sub.add_parser("status", help="kontrollera läsbarhet/flashskydd")
    r = sub.add_parser("read", help="läs flash till .bin")
    r.add_argument("output")
    r.add_argument("--start", type=lambda x: int(x, 0), default=0)
    r.add_argument("--length", type=lambda x: int(x, 0), default=0)
    v = sub.add_parser("verify", help="jämför flash mot .bin/.hex")
    v.add_argument("file")
    v.add_argument("--base", type=lambda x: int(x, 0), default=0)
    w = sub.add_parser("write", help="skriv .bin/.hex (kräver --yes)")
    w.add_argument("file")
    w.add_argument("--base", type=lambda x: int(x, 0), default=0, help="startadress för .bin")
    w.add_argument("--yes", action="store_true")
    e = sub.add_parser("erase", help="radera sida eller allt (kräver --yes)")
    e.add_argument("--page", type=int)
    e.add_argument("--all", action="store_true")
    e.add_argument("--yes", action="store_true")
    args = ap.parse_args(argv)

    if args.cmd == "ports":
        return cmd_ports(args)
    handlers = dict(info=cmd_info, status=cmd_status, read=cmd_read, verify=cmd_verify, write=cmd_write, erase=cmd_erase)
    p = None
    try:
        p = Programmer.open(args.port, args.baud)
        return handlers[args.cmd](args, p)
    except C2Error as ex:
        err(f"FEL: {ex}")
        return 1
    except (ValueError, OSError) as ex:
        err(f"FEL: {ex}")
        return 1
    except KeyboardInterrupt:
        err("Avbrutet.")
        return 1
    finally:
        if p:
            p.reset()


if __name__ == "__main__":
    sys.exit(main())
