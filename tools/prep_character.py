#!/usr/bin/env python3
"""
Prep a character pack: downscale GIFs to 96px with a CONSISTENT crop
across all states, so the character is the same size in every animation.
Writes to characters/<name>/ ready to drag onto the Hardware Buddy window.

Usage:
  python3 tools/prep_character.py <character-dir-or-zip> [--width N]

--width sets the on-device sprite width, default 96. The firmware centres
whatever it is given -- gifX = (135 - w) / 2, gifY = (140 - h) / 2 -- and peek
mode halves it into a 70px window, so anything up to 135x140 renders correctly
and 96 leaves most of the pet area empty. Wider costs bytes roughly with the
pixel count, against a 1.8MB budget for the whole pack.
"""
import json, sys, shutil, tempfile, zipfile
from pathlib import Path
from PIL import Image, ImageSequence

TARGET_W = 96      # overridden by --width
REF_W    = 1000   # normalize to this before computing the cross-state bbox
PROJECT  = Path(__file__).resolve().parent.parent
OUT_ROOT = PROJECT / "characters"


def _load_normalized(src_path: Path) -> tuple[list[Image.Image], list[int]]:
    """All frames at REF_W width, RGBA, with durations."""
    im = Image.open(src_path)
    frames, durations = [], []
    for f in ImageSequence.Iterator(im):
        durations.append(f.info.get("duration", 100))
        rgba = f.convert("RGBA").copy()
        scale = REF_W / rgba.width
        frames.append(rgba.resize((REF_W, round(rgba.height * scale)), Image.LANCZOS))
    return frames, durations


def _union(a, b):
    if a is None: return b
    if b is None: return a
    return (min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3]))


def _save_state(frames, durations, dst: Path, bbox, bg_rgb, target_w=TARGET_W):
    out = []
    for f in frames:
        cropped = f.crop(bbox)
        w, h = cropped.size
        new_h = max(1, round(h * target_w / w))
        resized = cropped.resize((target_w, new_h), Image.LANCZOS)
        flat = Image.new("RGB", resized.size, bg_rgb)
        flat.paste(resized, mask=resized.split()[-1])
        out.append(flat.convert("P", palette=Image.ADAPTIVE, colors=64))
    out[0].save(
        dst, save_all=True, append_images=out[1:],
        duration=durations, loop=0, optimize=False, disposal=1,
    )
    return dst.stat().st_size


def install(src: Path, target_w: int = TARGET_W) -> None:
    if src.suffix == ".zip":
        tmp = Path(tempfile.mkdtemp())
        with zipfile.ZipFile(src) as z:
            z.extractall(tmp)
        found = list(tmp.rglob("manifest.json"))
        if not found:
            sys.exit("no manifest.json in zip")
        src = found[0].parent

    manifest = json.loads((src / "manifest.json").read_text())
    name = manifest["name"]
    bg_hex = manifest.get("colors", {}).get("bg", "#000000").lstrip("#")
    bg_rgb = tuple(int(bg_hex[i:i+2], 16) for i in (0, 2, 4))

    # Pass 1: load every state (single or list), normalize, compute one bbox across all
    loaded = []   # (out_name, state_key, frames, durations, src_bytes)
    global_bbox = None
    for state, cfg in manifest["states"].items():
        entries = cfg if isinstance(cfg, list) else [cfg]
        for i, entry in enumerate(entries):
            gif_src = src / entry
            if not gif_src.exists():
                print(f"  skip {state}[{i}]: {entry} not found")
                continue
            frames, durations = _load_normalized(gif_src)
            out_name = f"{state}_{i}.gif" if len(entries) > 1 else f"{state}.gif"
            loaded.append((out_name, state, frames, durations, gif_src.stat().st_size))
            for f in frames:
                global_bbox = _union(global_bbox, f.getbbox())

    cw, ch = global_bbox[2] - global_bbox[0], global_bbox[3] - global_bbox[1]
    out_h = round(ch * target_w / cw)
    if out_h > 140 or target_w > 135:
        print(f"  warning: {target_w}x{out_h} exceeds the 135x140 the firmware "
              f"places into; it will be clipped")
    print(f"  global crop: {global_bbox} from {REF_W}-wide reference -> {target_w}x{out_h} on device\n")

    # Pass 2: write
    out = OUT_ROOT / name
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    device_states, total = {}, 0
    for out_name, state, frames, durations, src_bytes in loaded:
        dst = out / out_name
        after = _save_state(frames, durations, dst, global_bbox, bg_rgb, target_w)
        total += after
        device_states.setdefault(state, []).append(out_name)
        print(f"  {out_name:14s} {src_bytes:>10,}b -> {after:>7,}b  ({len(frames)} frames)")
    # Collapse single-entry lists back to strings for the common case
    device_states = {k: (v[0] if len(v) == 1 else v) for k, v in device_states.items()}

    (out / "manifest.json").write_text(json.dumps({
        "name": name,
        "colors": manifest.get("colors", {}),
        "states": device_states,
    }, indent=2))

    cap_kb = 1800
    print(f"\nwrote {name}: {total:,} bytes -> {out}")
    if total > cap_kb * 1024:
        print(f"  warning: over {cap_kb}KB — desktop install will reject it")
        if not shutil.which("gifsicle"):
            hint = {
                "darwin": "brew install gifsicle",
                "win32":  "winget install LCDF.Gifsicle",
            }.get(sys.platform, "apt install gifsicle")
            print(f"  gifsicle not found: {hint}")
        gifs = " ".join(f'"{g}"' for g in out.glob("*.gif"))
        print(f"  shrink: gifsicle --batch --lossy=80 -O3 --colors 64 {gifs}")
    print("next: drag that folder onto the Hardware Buddy window")


if __name__ == "__main__":
    args = sys.argv[1:]
    width = TARGET_W
    if "--width" in args:
        i = args.index("--width")
        try:
            width = int(args[i + 1])
        except (IndexError, ValueError):
            sys.exit("--width needs a number")
        del args[i:i + 2]
    if len(args) != 1:
        sys.exit(__doc__)
    install(Path(args[0]), width)
