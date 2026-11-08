#!/usr/bin/env python3
"""Download the head-tracking test scene assets from Poly Haven (all CC0).

Writes into head-test/assets/:
  sky/<id>.jpg         equirectangular sky panoramas (Poly Haven tonemapped JPG, resized)
  ground/<id>.jpg      tiling ground textures (diffuse only)
  models/<id>/...      glTF models, stripped to base-colour textures only
  CREDITS.txt          source list

Usage:  python fetch_assets.py            (needs Pillow)
The assets are committed to the repo, so this only needs re-running to change the asset set.
"""
import json, os, sys, shutil, urllib.request, io, time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "assets")
API = "https://api.polyhaven.com"
UA = {"User-Agent": "trackhat-opentrack-asset-fetch"}

SKIES = {  # id -> output width (height = width/2)
    "kloofendal_48d_partly_cloudy_puresky": 4096,  # cockpit
    "zwartkops_start_afternoon": 4096,             # race car (Zwartkops Raceway start line)
    "mud_road": 4096,                              # battlefield
}
GROUND = {  # id -> output size
    "aerial_grass_rock": 1024,   # terrain far below the aircraft
    "brown_mud_leaves_01": 1024, # battlefield ground
}
MODELS = [
    "bolt_action_rifle_7_62",
    "wooden_military_crate",
    "ammo_box",
    "metal_jerrycan_green",
    "concrete_road_barrier",
    "dead_tree_trunk",
    "old_tyre",
    "stick_grenade",
]
MODEL_TEX_MAX = 1024


def get(url, tries=5):
    for i in range(tries):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=120) as r:
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


def fetch_sky(asset_id, width):
    files = api(f"files/{asset_id}")
    img = Image.open(io.BytesIO(get(files["tonemapped"]["url"])))
    img = img.resize((width, width // 2), Image.LANCZOS)
    save_jpg(img, os.path.join(OUT, "sky", f"{asset_id}.jpg"), 85)


def fetch_ground(asset_id, size):
    files = api(f"files/{asset_id}")
    diff = files.get("Diffuse") or files.get("diffuse")
    entry = diff.get("1k") or diff[sorted(diff)[0]]
    img = Image.open(io.BytesIO(get(entry["jpg"]["url"])))
    img = img.resize((size, size), Image.LANCZOS)
    save_jpg(img, os.path.join(OUT, "ground", f"{asset_id}.jpg"))


def fetch_model(asset_id):
    files = api(f"files/{asset_id}")
    g = files["gltf"]["1k"]["gltf"]
    doc = json.loads(get(g["url"]))
    # keep only base colour textures
    keep_tex = set()
    for m in doc.get("materials", []):
        for k in ("normalTexture", "occlusionTexture", "emissiveTexture"):
            m.pop(k, None)
        pbr = m.get("pbrMetallicRoughness", {})
        pbr.pop("metallicRoughnessTexture", None)
        m.pop("extensions", None)
        if "baseColorTexture" in pbr:
            keep_tex.add(pbr["baseColorTexture"]["index"])
    tex_map, new_tex, img_map, new_img = {}, [], {}, []
    for old in sorted(keep_tex):
        t = dict(doc["textures"][old])
        src = t["source"]
        if src not in img_map:
            img_map[src] = len(new_img)
            new_img.append(doc["images"][src])
        t["source"] = img_map[src]
        t.pop("extensions", None)
        tex_map[old] = len(new_tex)
        new_tex.append(t)
    for m in doc.get("materials", []):
        pbr = m.get("pbrMetallicRoughness", {})
        if "baseColorTexture" in pbr:
            pbr["baseColorTexture"]["index"] = tex_map[pbr["baseColorTexture"]["index"]]
    doc["textures"], doc["images"] = new_tex, new_img
    if not new_tex:
        doc.pop("textures"); doc.pop("images")
        doc.pop("samplers", None)
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
        if rel.lower().endswith((".jpg", ".png")):
            img = Image.open(io.BytesIO(data))
            if max(img.size) > MODEL_TEX_MAX:
                img.thumbnail((MODEL_TEX_MAX, MODEL_TEX_MAX), Image.LANCZOS)
            save_jpg(img, dst) if rel.lower().endswith(".jpg") else img.save(dst)
        else:
            with open(dst, "wb") as f:
                f.write(data)
    with open(os.path.join(out, f"{asset_id}.gltf"), "w", encoding="utf-8") as f:
        json.dump(doc, f, separators=(",", ":"))


def main():
    credits = ["Head-tracking test scene assets. All from Poly Haven (https://polyhaven.com), licensed CC0 1.0.", ""]
    for kind, items, fn in (("Sky", SKIES, fetch_sky), ("Ground texture", GROUND, fetch_ground)):
        for a, size in items.items():
            print(kind, a); fn(a, size)
            credits.append(f"{kind}: {a}  https://polyhaven.com/a/{a}")
    for a in MODELS:
        print("Model", a); fetch_model(a)
        credits.append(f"Model: {a}  https://polyhaven.com/a/{a}")
    with open(os.path.join(OUT, "CREDITS.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(credits) + "\n")


if __name__ == "__main__":
    sys.exit(main())
