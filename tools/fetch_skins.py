#!/usr/bin/env python3
"""Builds assets/skins.json from the local Counter-Strike 2 install.

The econ database (items_game.txt) and the english localisation
(csgo_english.txt) are pulled out of pak01_dir.vpk with ValveResourceFormat,
then the paint kits, their rarities and the weapon / knife / glove lists are
written out as a small json the menu can read directly.

    ./tools/fetch_skins.py

Result: assets/skins.json
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kv1 import parse  # noqa: E402

VRF_VERSION = "20.1"
VRF_URL = ("https://github.com/ValveResourceFormat/ValveResourceFormat/releases/"
           "download/{v}/cli-linux-x64.zip")

GAME_PATHS = [
    "~/.steam/steam/steamapps/common/Counter-Strike Global Offensive",
    "~/.local/share/Steam/steamapps/common/Counter-Strike Global Offensive",
    "/mnt/c/Program Files (x86)/Steam/steamapps/common/Counter-Strike Global Offensive",
]

WEAPON_CLASSES = {"rifle", "pistol", "smg", "shotgun", "heavy", "sniper", "lmg", "machinegun"}


def find_game():
    for path in GAME_PATHS:
        path = os.path.expanduser(path)
        if os.path.isfile(os.path.join(path, "game/csgo/pak01_dir.vpk")):
            return path
    return None


def ensure_vrf(work_dir):
    cli = os.path.join(work_dir, "Source2Viewer-CLI")
    if os.path.isfile(cli):
        return cli

    local = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".vrf", "Source2Viewer-CLI")
    if os.path.isfile(local):
        return local

    print("fetching ValveResourceFormat", VRF_VERSION)
    archive = os.path.join(work_dir, "vrf.zip")
    urllib_download(VRF_URL.format(v=VRF_VERSION), archive)
    with zipfile.ZipFile(archive) as zf:
        zf.extractall(work_dir)
    os.chmod(cli, 0o755)
    return cli


def urllib_download(url, target):
    import urllib.request
    with urllib.request.urlopen(url) as response, open(target, "wb") as out:
        shutil.copyfileobj(response, out)


def extract(vrf, vpk, vpk_path, target):
    subprocess.run([vrf, "-i", vpk, "--vpk_filepath", vpk_path, "-o", target],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if os.path.isdir(target):
        for root, _, files in os.walk(target):
            for name in files:
                return os.path.join(root, name)
    return target


def localization(path):
    """{token: text} out of csgo_english.txt"""
    root = parse(open(path, encoding="utf-8", errors="replace").read())
    tokens = root.children.get("Tokens")
    out = {}
    if tokens is None:
        return out
    for key, value in tokens.children.items():
        if isinstance(value, str):
            out[key] = value
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--game")
    parser.add_argument("--out", default=None)
    args = parser.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_path = args.out or os.path.join(root, "assets", "skins.json")

    game = args.game or find_game()
    if not game:
        sys.exit("could not find a Counter-Strike 2 install")
    vpk = os.path.join(game, "game/csgo/pak01_dir.vpk")
    print("game:", game)

    with tempfile.TemporaryDirectory() as tmp:
        vrf = ensure_vrf(tmp)
        print("extracting items_game.txt")
        items_path = extract(vrf, vpk, "scripts/items/items_game.txt", os.path.join(tmp, "items"))
        print("extracting csgo_english.txt")
        lang_path = extract(vrf, vpk, "resource/csgo_english.txt", os.path.join(tmp, "lang"))
        print("listing the vpk")
        listing = subprocess.run([vrf, "-i", vpk, "--vpk_list"], check=True,
                                 stdout=subprocess.PIPE).stdout.decode("utf-8", "replace")

        print("parsing")
        tree = parse(open(items_path, encoding="utf-8", errors="replace").read())
        tokens = localization(lang_path)

        ig = tree["items_game"]

        rarities = {}
        for name, node in ig["rarities"].children.items():
            value = node.children.get("value")
            if value is not None:
                rarities[name] = int(value)

        # paint kits ------------------------------------------------------------
        paints = {}
        for pid, node in ig["paint_kits"].children.items():
            if not pid.isdigit():
                continue
            fields = node.children
            internal = fields.get("name", "")
            tag = fields.get("description_tag", "")
            display = lookup(tokens, tag)
            if not display:
                display = lookup(tokens, fields.get("description_string", ""))
            if not display:
                display = internal.replace("_", " ").title()
            wear = fields.get("wear_default")
            paints[pid] = {
                "name": internal,
                "display": display,
                "wear": round(float(wear), 4) if wear else 0.0,
            }

        # paint rarity ----------------------------------------------------------
        by_internal = {paint["name"]: pid for pid, paint in paints.items()}
        for name, node in ig["paint_kits_rarity"].children.items():
            pid = by_internal.get(name)
            if pid is None:
                continue
            rarity_name = node.children if hasattr(node, "children") else node
            paints[pid]["rarity"] = rarity_name
            paints[pid]["rarity_value"] = rarities.get(rarity_name, 0)

        # items -----------------------------------------------------------------
        prefabs = ig["prefabs"].children
        items = {}
        for def_index, node in ig["items"].children.items():
            if not def_index.isdigit():
                continue
            fields = node.children
            internal = fields.get("name", "")
            prefab = prefabs.get(fields.get("prefab", ""), None)
            display = ""
            slot = ""
            if prefab is not None:
                display = lookup(tokens, prefab.children.get("item_name", ""))
                slot = prefab.children.get("prefab", "")
            if not display:
                display = internal.replace("weapon_", "").replace("_", " ").title()
            items[def_index] = {
                "name": internal,
                "display": display,
                "kind": classify(internal, slot),
            }

        # weapon -> paint kits, recovered from the econ preview image names -----
        mapping = weapon_paint_mapping(ig, paints, listing.splitlines())

        for def_index, item in items.items():
            item["paints"] = mapping.get(item["name"], [])

        database = {
            "rarities": rarities,
            "paints": paints,
            "items": items,
        }

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as out:
        json.dump(database, out, indent=1, sort_keys=True)

    print("wrote", out_path)
    print("  paints:", len(paints), " items:", len(items))


def lookup(tokens, token):
    if not token:
        return ""
    if token.startswith("#"):
        token = token[1:]
    return tokens.get(token, "")


def classify(internal, prefab_class):
    if internal.startswith("weapon_knife"):
        return "knife"
    if internal.endswith("_gloves") or internal.endswith("_gloves_b"):
        return "glove"
    if internal.startswith("customplayer_") or internal.startswith("character_"):
        return "agent"
    if internal.startswith("weapon_"):
        return "weapon"
    return "other"


def weapon_paint_mapping(ig, paints, vpk_listing):
    """weapon_<weapon>_<paint>_<variant> image names carry the pair."""
    by_paint_name = {}
    for pid, paint in paints.items():
        by_paint_name.setdefault(paint["name"], []).append(pid)

    item_names = []
    for pid, node in ig["items"].children.items():
        if not pid.isdigit():
            continue
        internal = node.children.get("name", "")
        if internal:
            item_names.append(internal)
    item_names.sort(key=len, reverse=True)

    mapping = {}
    for line in vpk_listing:
        if "/econ/default_generated/" not in line:
            continue
        base = line.split()[0].split("/")[-1]
        if base.endswith("_png.vtex_c"):
            base = base[:-len("_png.vtex_c")]
        for variant in ("_light", "_medium", "_heavy"):
            if base.endswith(variant):
                base = base[:-len(variant)]
                break

        for internal in item_names:
            if not base.startswith(internal + "_"):
                continue
            paint_name = base[len(internal) + 1:]
            for pid in by_paint_name.get(paint_name, []):
                bucket = mapping.setdefault(internal, [])
                paint_id = int(pid)
                if paint_id not in bucket:
                    bucket.append(paint_id)
            break

    return mapping


if __name__ == "__main__":
    main()
