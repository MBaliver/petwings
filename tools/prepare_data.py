#!/usr/bin/env python3
"""Extract PetWings game data from the original Windows release.

Usage: prepare_data.py <dir containing PetWings.exe and Music0N.mid> <output data dir>

Produces:
  gfx/<id>.bmp      8-bit bitmaps (shared palette)
  sfx/<id>.wav      sound effects
  script/<id>.txt   movement scripts / maps
  music/musicNN.ogg pre-rendered MIDI tracks (needs fluidsynth + ffmpeg + a GM soundfont)

Only the Python standard library is used.
"""
import os
import shutil
import struct
import subprocess
import sys

RT_BITMAP = 2
RT_RCDATA = 10


def read_pe_resources(path):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("not a PE file: " + path)
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    # PE32: data directories start at opt+96; resource dir is index 2
    rsrc_rva, _ = struct.unpack_from("<II", data, opt + 96 + 2 * 8)
    sections = []
    for i in range(nsec):
        off = opt + optsz + i * 40
        vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        sections.append((va, max(vsize, rawsize), rawptr))

    def rva2off(rva):
        for va, size, raw in sections:
            if va <= rva < va + size:
                return rva - va + raw
        raise ValueError(hex(rva))

    base = rva2off(rsrc_rva)

    def entries(off):
        named, ids = struct.unpack_from("<HH", data, off + 12)
        for i in range(named + ids):
            name, target = struct.unpack_from("<II", data, off + 16 + i * 8)
            if name & 0x80000000:
                p = base + (name & 0x7FFFFFFF)
                ln = struct.unpack_from("<H", data, p)[0]
                key = data[p + 2:p + 2 + ln * 2].decode("utf-16le")
            else:
                key = name
            yield key, target

    out = {}
    for tkey, ttarget in entries(base):
        for nkey, ntarget in entries(base + (ttarget & 0x7FFFFFFF)):
            for _, ltarget in entries(base + (ntarget & 0x7FFFFFFF)):
                rva, size = struct.unpack_from("<II", data, base + ltarget)
                o = rva2off(rva)
                out[(tkey, nkey)] = data[o:o + size]
    return out


def bmp_from_dib(dib):
    hs = struct.unpack_from("<I", dib, 0)[0]
    bpp = struct.unpack_from("<H", dib, 14)[0]
    clr = struct.unpack_from("<I", dib, 32)[0]
    pal = (clr or (1 << bpp if bpp <= 8 else 0)) * 4
    return b"BM" + struct.pack("<IHHI", 14 + len(dib), 0, 0, 14 + hs + pal) + dib


def read_bmp8(path):
    d = open(path, "rb").read()
    off = struct.unpack_from("<I", d, 10)[0]
    w, h = struct.unpack_from("<ii", d, 18)
    pal = [(d[54 + i * 4 + 2], d[54 + i * 4 + 1], d[54 + i * 4]) for i in range(256)]
    row = (w + 3) & ~3
    px = [[d[off + (h - 1 - y) * row + x] for x in range(w)] for y in range(h)]
    return w, h, pal, px


def png_rgba(w, h, rgba):
    import zlib
    raw = b"".join(b"\0" + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    def chunk(t, data):
        c = t + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def make_icon(path, size):
    """Square RGBA icon: sprite cropped to its opaque area, centred, area-averaged down-scale."""
    w, h, pal, px = read_bmp8(path)
    t = px[0][0]
    xs = [x for y in range(h) for x in range(w) if px[y][x] != t]
    ys = [y for y in range(h) for x in range(w) if px[y][x] != t]
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    side = max(x1 - x0, y1 - y0)
    margin = side // 16
    side += 2 * margin
    ox = x0 - (side - (x1 - x0)) // 2
    oy = y0 - (side - (y1 - y0)) // 2
    out = bytearray(size * size * 4)
    for Y in range(size):
        sy0, sy1 = Y * side / size, (Y + 1) * side / size
        for X in range(size):
            sx0, sx1 = X * side / size, (X + 1) * side / size
            r = g = b = a = n = 0.0
            yy = int(sy0)
            while yy < sy1:
                xx = int(sx0)
                while xx < sx1:
                    n += 1
                    px_x, px_y = ox + xx, oy + yy
                    if 0 <= px_x < w and 0 <= px_y < h and px[px_y][px_x] != t:
                        cr, cg, cb = pal[px[px_y][px_x]]
                        r += cr; g += cg; b += cb; a += 1
                    xx += 1
                yy += 1
            i = (Y * size + X) * 4
            if a:
                out[i:i + 4] = bytes((int(r / a), int(g / a), int(b / a), int(255 * a / n)))
    return png_rgba(size, size, out)


def make_og_image(gfx):
    """1200x630 link-preview image (Open Graph) in the style of the title screen."""
    W, H = 1200, 630
    out = bytearray(W * H * 4)
    tw, th, tpal, tpx = read_bmp8(os.path.join(gfx, "191.bmp"))
    for y in range(H):
        for x in range(W):
            r, g, b = tpal[tpx[(y + 40) % th][(x + 40) % tw]]
            i = (y * W + x) * 4
            out[i:i + 4] = bytes((r, g, b, 255))

    def blit(name, cx, cy):
        w, h, pal, px = read_bmp8(os.path.join(gfx, name + ".bmp"))
        t = px[0][0]
        x0, y0 = cx - w // 2, cy - h // 2
        for y in range(h):
            for x in range(w):
                c = px[y][x]
                if c != t and 0 <= x0 + x < W and 0 <= y0 + y < H:
                    i = ((y0 + y) * W + x0 + x) * 4
                    out[i:i + 3] = bytes(pal[c])

    # same layout as the title (wings around the logo), pet below
    blit("197", 600 - 200, 170)
    blit("196", 600 + 200, 170)
    blit("195", 600, 190)
    blit("187", 600, 460)
    return png_rgba(W, H, out)


def find_soundfont():
    env = os.environ.get("PETWINGS_SOUNDFONT")
    cands = [env] if env else []
    cands += [
        "/usr/share/soundfonts/FluidR3_GM.sf2",
        "/usr/share/sounds/sf2/FluidR3_GM.sf2",
        "/usr/share/soundfonts/default.sf2",
        "/usr/share/sounds/sf2/default-GM.sf2",
    ]
    for c in cands:
        if c and os.path.isfile(c):
            return c
    return None


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    exe = os.path.join(src, "PetWings.exe")
    res = read_pe_resources(exe)
    for sub in ("gfx", "sfx", "script", "music"):
        os.makedirs(os.path.join(dst, sub), exist_ok=True)
    n = {"gfx": 0, "sfx": 0, "script": 0}
    for (t, name), blob in res.items():
        if t == RT_BITMAP:
            open(os.path.join(dst, "gfx", f"{name}.bmp"), "wb").write(bmp_from_dib(blob))
            n["gfx"] += 1
        elif t == "WAVE":
            open(os.path.join(dst, "sfx", f"{name}.wav"), "wb").write(blob)
            n["sfx"] += 1
        elif t == RT_RCDATA:
            open(os.path.join(dst, "script", f"{name}.txt"), "wb").write(blob)
            n["script"] += 1
    print(f"extracted {n['gfx']} bitmaps, {n['sfx']} sounds, {n['script']} scripts")

    # web/app icons from the big flying pet (bitmap 154)
    os.makedirs(os.path.join(dst, "web"), exist_ok=True)
    pet = os.path.join(dst, "gfx", "154.bmp")
    for size in (512, 192, 180, 32):
        open(os.path.join(dst, "web", f"icon-{size}.png"), "wb").write(make_icon(pet, size))
    open(os.path.join(dst, "web", "og-image.png"), "wb").write(make_og_image(os.path.join(dst, "gfx")))
    print("wrote web icons and link preview image")

    sf = find_soundfont()
    fluid, ffmpeg = shutil.which("fluidsynth"), shutil.which("ffmpeg")
    for i in (1, 2, 3):
        mid = os.path.join(src, f"Music{i:02d}.mid")
        ogg = os.path.join(dst, "music", f"music{i:02d}.ogg")
        if os.path.exists(ogg):
            continue
        if not os.path.exists(mid):
            print("warning: missing", mid)
            continue
        if not (sf and fluid and ffmpeg):
            print("warning: need fluidsynth, ffmpeg and a GM soundfont to render", mid)
            continue
        wav = ogg[:-4] + ".wav"
        subprocess.run([fluid, "-ni", "-q", "-F", wav, "-r", "44100", "-g", "0.6", sf, mid],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # trim trailing silence so the loop restarts promptly, then encode
        subprocess.run([ffmpeg, "-loglevel", "error", "-y", "-i", wav, "-af",
                        "areverse,silenceremove=start_periods=1:start_threshold=-60dB,areverse",
                        "-c:a", "libvorbis", "-q:a", "4", ogg], check=True)
        os.remove(wav)
        print("rendered", ogg)


if __name__ == "__main__":
    main()
