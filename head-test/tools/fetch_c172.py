#!/usr/bin/env python3
"""Builds the cockpit scene model from FlightGear's Cessna 172P (GPL-2.0-or-later).

Source: https://github.com/c172p-team/c172p (master). Fetches the needed .ac/.xml/texture
files, follows the model XML includes (offsets only; animations are ignored, so needles and
switches stay at their modelled rest positions), skips optional/damaged/effect variants, and
writes one glTF to head-test/assets/models/c172p/ in the test window's frame:
metres, +X right, +Y up, -Z forward, origin at the pilot's eye.

Usage: python fetch_c172.py [cache_dir]
"""
import json, math, os, re, sys, urllib.request, time, shutil
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ac3d  # noqa: E402

REPO = "https://raw.githubusercontent.com/c172p-team/c172p/master/"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "assets", "models", "c172p")
CACHE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, ".cache-c172p")

# Pilot eye point in FlightGear model coordinates (x aft, y right, z up), metres.
EYE_FG = (0.30, -0.255, 0.38)

# Object / include names that are optional equipment, damage states, effects or people.
SKIP = re.compile(r"damage|broken|pontoon|float|ski|bush|fg1000|g1000|human|pilot|copilot|crew|walker|"
                  r"shadow|light|lamp|glow|halo|cone|beacon|strobe|procedural|reflect|fuelcap|dipstick|"
                  r"tiedown|chock|towbar|cowlplug|rbf|heater|external|control-lock|suitcase|bag|"
                  r"immat|registration|ice|frost|rain|prop-disc|propdisc|blur|fast|immat|tank|sump|"
                  r"rope|harness|mooring|ladder|parachute|step|visor", re.I)
KEEP_ANYWAY = re.compile(r"^(pedal|yoke|stick|seat)", re.I)

UA = {"User-Agent": "trackhat-asset-fetch"}


def fetch(rel):
    rel = rel.replace("\\", "/").lstrip("/")
    dst = os.path.join(CACHE, rel)
    if os.path.exists(dst):
        return dst
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    for i in range(5):
        try:
            with urllib.request.urlopen(urllib.request.Request(REPO + urllib.request.quote(rel), headers=UA), timeout=120) as r:
                data = r.read()
            with open(dst, "wb") as f:
                f.write(data)
            return dst
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            time.sleep(3 * (i + 1))
        except Exception:
            time.sleep(3 * (i + 1))
    return None


def repo_rel(path, xml_dir):
    path = path.strip()
    if path.startswith("Aircraft/c172p/"):
        return path[len("Aircraft/c172p/"):]
    if path.startswith("Aircraft/"):
        return None  # outside this aircraft (shared FlightGear data)
    return os.path.normpath(os.path.join(xml_dir, path)).replace("\\", "/")


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return [[1, 0, 0], [0, c, -s], [0, s, c]]


def rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return [[c, 0, s], [0, 1, 0], [-s, 0, c]]


def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return [[c, -s, 0], [s, c, 0], [0, 0, 1]]


def compose(a, b):
    """(m, t) transforms; returns a∘b (apply b first)."""
    ma, ta = a
    mb, tb = b
    m = ac3d._mat_mul(ma, mb)
    t = ac3d._apply(ma, ta, tb)
    return (m, t)


# AC3D files are y-up; FlightGear rotates them to z-up: FG(x, y, z) = (ac.x, -ac.z, ac.y)
AC_TO_FG = ([[1, 0, 0], [0, 0, -1], [0, 1, 0]], (0, 0, 0))


def offsets_xform(node):
    off = node.find("offsets")
    if off is None:
        return (ac3d.IDENT, (0, 0, 0))
    g = lambda k: float(off.findtext(k, "0").strip() or 0)
    t = (g("x-m"), g("y-m"), g("z-m"))
    m = ac3d._mat_mul(rot_z(math.radians(g("heading-deg"))),
                      ac3d._mat_mul(rot_y(math.radians(g("pitch-deg"))), rot_x(math.radians(g("roll-deg")))))
    return (m, t)


def select_hidden(root):
    """Objects that are toggled by 'select' animations; hide the ones that look optional."""
    hidden = set()
    for anim in root.iter("animation"):
        if (anim.findtext("type") or "").strip() != "select":
            continue
        for on in anim.iter("object-name"):
            n = (on.text or "").strip()
            if SKIP.search(n) and not KEEP_ANYWAY.search(n):
                hidden.add(n)
    return hidden


def load_xml(rel, xform, mesh, report, depth=0, name=""):
    p = fetch(rel)
    if not p:
        report.append(f"missing {rel}")
        return
    if rel.endswith(".ac"):
        load_ac(rel, xform, mesh, report, set())
        return
    try:
        root = ET.parse(p).getroot()
    except ET.ParseError as e:
        report.append(f"xml {rel}: {e}")
        return
    xml_dir = os.path.dirname(rel)
    hidden = select_hidden(root)
    top = root.findtext("path")
    if top:
        r = repo_rel(top, xml_dir)
        if r:
            load_ac(r, xform, mesh, report, hidden)
    for m in root.findall("model"):
        nm = (m.findtext("name") or "").strip()
        path = (m.findtext("path") or "").strip()
        if not path or m.find("condition") is not None:
            continue
        if SKIP.search(nm + " " + path) and not KEEP_ANYWAY.search(nm):
            continue
        r = repo_rel(path, xml_dir)
        if not r:
            continue
        load_xml(r, compose(xform, offsets_xform(m)), mesh, report, depth + 1, nm)


def load_ac(rel, xform, mesh, report, hidden):
    p = fetch(rel)
    if not p:
        report.append(f"missing {rel}")
        return
    # pre-fetch textures referenced by the file
    with open(p, encoding="latin-1") as f:
        for tex in set(re.findall(r'^texture\s+"([^"]+)"', f.read(), re.M)):
            if not fetch(os.path.normpath(os.path.join(os.path.dirname(rel), tex)).replace("\\", "/")):
                fetch("Models/" + os.path.basename(tex))  # default livery textures
    extra = os.environ.get("C172_SKIP_EXTRA")  # debugging aid: extra object-name regex to drop
    skip = lambda n: (n in hidden) or (bool(SKIP.search(n)) and not KEEP_ANYWAY.search(n)) or         bool(extra and re.search(extra, n, re.I))
    glass = lambda n: bool(re.search(r"glas|window|windshield|landinglightcover", n, re.I)) and not re.search(r"frame|lock|handle|latch", n, re.I)
    m = ac3d.read_ac(p, compose(xform, AC_TO_FG), skip=skip, report=report,
                     texdirs=(os.path.join(CACHE, "Models"),), glass=glass)
    mesh.merge(m)


def main():
    report = []
    mesh = ac3d.Mesh()
    load_xml("Models/c172p.xml", (ac3d.IDENT, (0, 0, 0)), mesh, report)
    load_xml("Models/Interior/Panel/c172p-panel/c172p.xml", (ac3d.IDENT, (0, 0, 0)), mesh, report)
    # FG model frame -> test window frame, eye at origin: X = y, Y = z, Z = x
    ex, ey, ez = EYE_FG
    for g in mesh.groups.values():
        g["pos"] = [(p[1] - ey, p[2] - ez, p[0] - ex) for p in g["pos"]]
        g["nrm"] = [(n[1], n[2], n[0]) for n in g["nrm"]]
    shutil.rmtree(OUT, ignore_errors=True)
    tris = ac3d.write_gltf(mesh, OUT, "c172p", max_tex=2048, report=report)
    # licence + credits travel with the converted model (GPL-2.0-or-later)
    shutil.copy(fetch("LICENSE"), os.path.join(OUT, "COPYING"))
    sha = "master"
    try:
        with urllib.request.urlopen(urllib.request.Request(
                "https://api.github.com/repos/c172p-team/c172p/commits/master", headers=UA), timeout=60) as r:
            sha = json.loads(r.read())["sha"]
    except Exception:
        pass
    authors = ET.parse(fetch("c172p-set.xml")).getroot().findtext(".//author") or ""
    text = [
        "Cessna 172P for FlightGear, https://github.com/c172p-team/c172p (commit " + sha + ")",
        "Licence: GPL-2.0-or-later (COPYING); some files CC-BY/CC0.",
        "Authors: " + " ".join(authors.split()),
        "3D model and textures: Fernando Barbosa (thevirtualfer), Jonathan Schellhase (dg-505) and contributors.",
        "",
        "Converted to glTF for the TrackHat head-tracking test by head-test/tools/fetch_c172.py:",
        "assembled from the model XML (no animations), optional/damage/effect parts and sun visors removed,",
        "glass made neutral, moved so the pilot's eye is at the origin.",
    ]
    with open(os.path.join(OUT, "AUTHORS"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(text) + "\n")
    print("triangles", tris, "groups", len(mesh.groups))
    for r in sorted(set(report)):
        print("  ", r)


if __name__ == "__main__":
    main()
