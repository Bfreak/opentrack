#!/usr/bin/env python3
"""Download the head-tracking test scene assets from Poly Haven (all CC0).

Writes into head-test/assets/:
  sky/<id>.jpg                 equirectangular panoramas (Poly Haven tonemapped JPG, resized)
  ground/<id>_{diff,nor,arm}.jpg  tiling PBR textures (colour, OpenGL normal, AO/rough/metal)
  models/<id>/...              glTF models with colour, normal and AO/rough/metal maps
  CREDITS.txt                  source list (also lists the non-Poly Haven models)

Usage:  python fetch_assets.py            (needs Pillow)
The assets are committed to the repo, so this only needs re-running to change the asset set.
The C172 cockpit (fetch_c172.py) and the CarConcept car (models/carconcept) come from elsewhere.
"""
import json, os, sys, shutil, urllib.request, io, time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "assets")
API = "https://api.polyhaven.com"
UA = {"User-Agent": "trackhat-opentrack-asset-fetch"}

SKY_WIDTH = 8192
SKIES = [
    "derelict_airfield_01",       # cockpit: lined up on a runway
    "zwartkops_start_afternoon",  # race car: Zwartkops Raceway start line
    "mud_road",                   # soldier: overcast farmland
]
GROUND = {  # id -> resolution
    "brown_mud_leaves_01": "2k",  # battlefield ground
    "hessian_230": "2k",          # sandbags
}
MODELS = {  # id -> resolution (2k for things within arm's reach)
    "bolt_action_rifle_7_62": "2k",
    "wooden_military_crate": "2k",
    "ammo_box": "2k",
    "vintage_radio_transceiver": "2k",
    "stick_grenade": "1k",
    "metal_jerrycan_green": "1k",
    "old_military_crate": "1k",
    "signal_flashlight": "1k",
    "concrete_road_barrier": "1k",
    "concrete_road_barrier_02": "1k",
    "dead_tree_trunk": "1k",
    "dead_tree_trunk_02": "1k",
    "tree_stump_01": "1k",
    "old_tyre": "1k",
    "rusted_wheel_rim_01": "1k",
    "barrel_03": "1k",
    "wooden_barrels_01": "1k",
    "boulder_01": "1k",
}


def get(url, tries=5):
    for i in range(tries):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=180) as r:
                return r.read()
        except Exception as e:  # noqa: BLE001
            print(f"  retry {i + 1}: {e}")
            time.sleep(5 * (i + 1))
    raise SystemExit(f"failed: {url}")


def api(path):
    return json.loads(get(f"{API}/{path}"))


def save_jpg(img, path, quality=88):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.convert("RGB").save(path, "JPEG", quality=quality, optimize=True, progressive=True)


def fetch_sky(asset_id):
    files = api(f"files/{asset_id}")
    img = Image.open(io.BytesIO(get(files["tonemapped"]["url"])))
    if img.width > SKY_WIDTH:
        img = img.resize((SKY_WIDTH, SKY_WIDTH // 2), Image.LANCZOS)
    save_jpg(img, os.path.join(OUT, "sky", f"{asset_id}.jpg"), 92)


def fetch_ground(asset_id, res):
    files = api(f"files/{asset_id}")
    for key, suffix in (("Diffuse", "diff"), ("nor_gl", "nor"), ("arm", "arm")):
        entry = files.get(key) or files.get(key.lower())
        if not entry:
            print(f"  {asset_id}: no {key}")
            continue
        e = entry.get(res) or entry[sorted(entry)[0]]
        src = e.get("jpg") or e.get("png")
        img = Image.open(io.BytesIO(get(src["url"])))
        save_jpg(img, os.path.join(OUT, "ground", f"{asset_id}_{suffix}.jpg"), 90 if suffix != "diff" else 88)


def fetch_model(asset_id, res):
    files = api(f"files/{asset_id}")
    g = files["gltf"][res]["gltf"]
    doc = json.loads(get(g["url"]))
    for m in doc.get("materials", []):
        m.pop("extensions", None)
    for k in ("extensionsUsed", "extensionsRequired"):
        doc.pop(k, None)
    out = os.path.join(OUT, "models", asset_id)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    needed = {b["uri"] for b in doc.get("buffers", [])} | {i["uri"] for i in doc.get("images", [])}
    for rel, inc in g["include"].items():
        if rel not in needed:
            continue
        data = get(inc["url"])
        dst = os.path.join(out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if rel.lower().endswith(".jpg"):
            save_jpg(Image.open(io.BytesIO(data)), dst, 90 if "_nor" in rel or "_arm" in rel else 88)
        else:
            with open(dst, "wb") as f:
                f.write(data)
    with open(os.path.join(out, f"{asset_id}.gltf"), "w", encoding="utf-8") as f:
        json.dump(doc, f, separators=(",", ":"))


def main():
    only = set(sys.argv[1:])
    credits = [
        "Head-tracking test scene assets.",
        "",
        "Poly Haven (https://polyhaven.com), CC0 1.0 public domain:",
    ]
    for a in SKIES:
        if not only or a in only:
            print("Sky", a); fetch_sky(a)
        credits.append(f"  Sky: {a}  https://polyhaven.com/a/{a}")
    for a, res in GROUND.items():
        if not only or a in only:
            print("Texture", a); fetch_ground(a, res)
        credits.append(f"  Texture: {a}  https://polyhaven.com/a/{a}")
    for a, res in MODELS.items():
        if not only or a in only:
            print("Model", a); fetch_model(a, res)
        credits.append(f"  Model: {a}  https://polyhaven.com/a/{a}")
    credits += [
        "",
        "models/c172p: Cessna 172P from FlightGear, https://github.com/c172p-team/c172p",
        "  GPL-2.0-or-later (some parts CC-BY/CC0); see models/c172p/COPYING and AUTHORS.",
        "  Converted to glTF by head-test/tools/fetch_c172.py.",
        "",
        "models/carconcept: \"Car Concept\" from the Khronos glTF Sample Assets,",
        "  https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/CarConcept",
        "  CC-BY 4.0 (models/carconcept/LICENSE.md). Based on a public-domain model by Unity Fan",
        "  (https://sketchfab.com/3d-models/free-concept-car-004-public-domain-cc0-4cba124633eb494eadc3bb0c4660ad7e).",
    ]
    with open(os.path.join(OUT, "CREDITS.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(credits) + "\n")


if __name__ == "__main__":
    sys.exit(main())
