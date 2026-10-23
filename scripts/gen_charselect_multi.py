#!/usr/bin/env python3
"""Add the CharacterSelect<N>_<col>_Multi2 and _Multi4 controls UI/SameCharacterSelect.cpp loads to a MenuMulti.szs:
each column's control on its Mii twin's layout, with the per-player OK markers in a 2 x 1 or 2 x 2 grid.
ReplacedAssets.szs is mounted ahead of MenuMulti.szs, so its controls are the sources. Requires wuj5.

Usage:
  gen_charselect_multi.py --wuj5 <wuj5 dir> <in MenuMulti.szs> <ReplacedAssets.szs> <out MenuMulti.szs>
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Each driver button size has a Mii twin with per-player OK markers.
MULTI_LAYOUTS = {
    "common_w006_chara": "common_w117_mii_suit",
    "common_w025_chara_middle": "common_w118_mii_suit_middle",
    "common_w026_chara_large": "common_w119_mii_suit_large",
}
CTRL_RE = re.compile(r"^CharacterSelect(\d+)_(\d+)(_Mii)?\.brctr$")
LAYOUT_SUFFIX = "_multi"
# The marker grids by the players they hold, (columns, rows).
MARKER_GRIDS = {2: (2, 1), 4: (2, 2)}


def find_dir(node: dict, path: list[str]) -> dict:
    for part in path:
        for child in node["children"]:
            if child["is_dir"] and child["name"] == part:
                node = child
                break
        else:
            raise SystemExit(f"directory {'/'.join(path)} not found in archive")
    return node


def to_multi(ctrl: dict) -> dict:
    layout = ctrl["main brlyt"]
    if layout in MULTI_LAYOUTS.values():
        return ctrl  # The Mii column already uses a multi layout.
    if layout not in MULTI_LAYOUTS:
        raise SystemExit(f"unexpected character button layout {layout!r}")
    multi = MULTI_LAYOUTS[layout]
    ctrl = dict(ctrl)
    ctrl["main brlyt"] = multi
    ctrl["animations"] = [
        {**anim, "brlan": anim["brlan"].replace(layout + "_", multi + "_", 1)}
        for anim in ctrl["animations"]
    ]
    return ctrl


def find_pane(pane: dict, name: str) -> dict | None:
    if pane.get("name") == name:
        return pane
    for child in pane.get("children", []):
        hit = find_pane(child, name)
        if hit is not None:
            return hit
    return None


def sized_markers(content: bytes, name: str, cells: int, unpack_brlyt, pack_brlyt) -> bytes:
    """The layout with its OK markers in a grid of `cells` over black_base, each at half the vanilla marker's
    width and 0.55 of its height, then scaled evenly as far as its cell allows."""
    cols, rows = MARKER_GRIDS[cells]
    lyt = unpack_brlyt(content)
    root = next(s for s in lyt["sections"] if s["magic"] == "pan1")
    panes = {n: find_pane(root, n) for n in ["black_base"] + [f"ok_null_{i}p" for i in range(1, 5)]}
    missing = [n for n, pane in panes.items() if pane is None]
    plate = find_pane(panes["ok_null_1p"], "ok_waku_1p") if panes["ok_null_1p"] else None
    if plate is None:
        missing.append("ok_waku_1p")
    if missing:
        raise SystemExit(f"{name}: panes {', '.join(missing)} missing")
    w, h = panes["black_base"]["size x"], panes["black_base"]["size y"]
    fit = min(4 / cols, (h / rows) / (plate["size y"] * panes["ok_null_1p"]["scale y"] * 0.55))
    for k in range(cells):
        pane = panes[f"ok_null_{k + 1}p"]
        col, row = k % cols, k // cols
        pane["translation x"] = w * ((col + 0.5) / cols - 0.5)
        pane["translation y"] = 0.0 if rows == 1 else (h * 0.23 if row == 0 else -h * 0.23)
        pane["scale x"] = pane["scale x"] * 0.5 * fit
        pane["scale y"] = pane["scale y"] * 0.55 * fit
    return pack_brlyt(lyt)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--wuj5", type=Path, required=True, help="wuj5 checkout")
    ap.add_argument("input", type=Path)
    ap.add_argument("replaced_assets", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()

    sys.path.insert(0, str(args.wuj5.resolve()))
    from brctr import pack_brctr, unpack_brctr
    from brlyt import pack_brlyt, unpack_brlyt
    from u8 import pack_u8, unpack_u8
    from yaz import pack_yaz, unpack_yaz

    def load_szs(path: Path) -> tuple[dict, dict, bool]:
        data = path.read_bytes()
        compressed = data[:4] == b"Yaz0"
        root = unpack_u8(unpack_yaz(data) if compressed else data)
        top = root["children"][0] if root["children"] and root["children"][0]["name"] == "." else root
        return root, top, compressed

    root, top, compressed = load_szs(args.input)
    ctrl_dir = find_dir(top, ["button", "ctrl"])
    blyt_dir = find_dir(top, ["button", "blyt"])
    layouts = {c["name"] for c in blyt_dir["children"]}
    for multi in MULTI_LAYOUTS.values():
        if multi + ".brlyt" not in layouts:
            raise SystemExit(f"{multi}.brlyt missing from {args.input}")

    effective = {c["name"]: c["content"] for c in ctrl_dir["children"]}
    _, overlay_top, _ = load_szs(args.replaced_assets)
    effective.update({c["name"]: c["content"] for c in find_dir(overlay_top, ["ctrl"])["children"]})
    overlay_layouts = {c["name"] for d in overlay_top["children"] if d["is_dir"] and d["name"] == "blyt"
                       for c in d["children"]}

    sources: dict[tuple[str, str], str] = {}
    for name in effective:
        m = CTRL_RE.match(name)
        # Prefer _Mii: it is what multiplayer already loads for the Mii column.
        if m and ((m.group(1), m.group(2)) not in sources or m.group(3)):
            sources[(m.group(1), m.group(2))] = name

    existing = {c["name"] for c in ctrl_dir["children"]}
    added = []
    multi_layouts = set()
    for (count, col), src in sorted(sources.items()):
        ctrl = to_multi(unpack_brctr(effective[src]))
        multi_layouts.add(ctrl["main brlyt"])
        for cells in MARKER_GRIDS:
            name = f"CharacterSelect{count}_{col}_Multi{cells}.brctr"
            if name in existing:
                raise SystemExit(f"{name} already in {args.input}")
            sized = dict(ctrl, **{"main brlyt": f"{ctrl['main brlyt']}{LAYOUT_SUFFIX}{cells}"})
            ctrl_dir["children"].append({"is_dir": False, "name": name, "content": pack_brctr(sized)})
            added.append(name)
    shadowed = sorted(f"{layout}.brlyt" for layout in multi_layouts if f"{layout}.brlyt" in overlay_layouts)
    if shadowed:
        raise SystemExit(f"{args.replaced_assets.name} shadows {', '.join(shadowed)}; copy the overlay's layout instead")
    for layout in sorted(multi_layouts):
        src = next(c for c in blyt_dir["children"] if c["name"] == layout + ".brlyt")
        for cells in MARKER_GRIDS:
            name = f"{layout}{LAYOUT_SUFFIX}{cells}.brlyt"
            if name in layouts:
                raise SystemExit(f"{name} already in {args.input}")
            blyt_dir["children"].append(
                {"is_dir": False, "name": name, "content": sized_markers(src["content"], name, cells, unpack_brlyt, pack_brlyt)})
    ctrl_dir["children"].sort(key=lambda c: c["name"].lower())

    out = pack_u8(root)
    args.output.write_bytes(pack_yaz(out) if compressed else out)
    print(f"wrote {args.output} with {len(added)} controls: {', '.join(added)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
