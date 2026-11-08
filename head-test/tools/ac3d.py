"""Minimal AC3D (.ac) / Speed Dreams (.acc) reader and glTF writer.

Used offline by the asset scripts to convert FlightGear and Speed Dreams models into
glTF for the head-tracking test window. Not used at runtime.
"""
import io, json, math, os, re, struct
from PIL import Image


class Mesh:
    """Triangles in a common frame, grouped by material key."""
    def __init__(self):
        self.groups = {}  # key -> dict(pos=[], nrm=[], uv=[], tex=path|None, color=(r,g,b,a), unlit=bool)

    def group(self, tex, color, unlit=False):
        key = (tex, tuple(round(c, 3) for c in color), unlit)
        g = self.groups.get(key)
        if g is None:
            g = self.groups[key] = dict(pos=[], nrm=[], uv=[], tex=tex, color=color, unlit=unlit)
        return g

    def merge(self, other):
        for (tex, col, unlit), g in other.groups.items():
            d = self.group(tex, col, unlit)
            for k in ("pos", "nrm", "uv"):
                d[k].extend(g[k])


def _mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def _apply(m, t, v):
    return (m[0][0]*v[0] + m[0][1]*v[1] + m[0][2]*v[2] + t[0],
            m[1][0]*v[0] + m[1][1]*v[1] + m[1][2]*v[2] + t[1],
            m[2][0]*v[0] + m[2][1]*v[1] + m[2][2]*v[2] + t[2])


def _norm(v):
    l = math.sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]) or 1.0
    return (v[0]/l, v[1]/l, v[2]/l)


def _cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def _sub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])


IDENT = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]


def triangulate(pts, normal):
    """Ear-clipping triangulation of a planar (possibly concave) polygon. Returns index triples."""
    n = len(pts)
    if n == 3:
        return [(0, 1, 2)]
    # project onto the plane's dominant axes
    ax = max(range(3), key=lambda i: abs(normal[i]))
    u, v = [(1, 2), (2, 0), (0, 1)][ax]
    p2 = [(p[u], p[v]) for p in pts]
    area = sum(p2[i][0] * p2[(i + 1) % n][1] - p2[(i + 1) % n][0] * p2[i][1] for i in range(n))
    sign = 1.0 if area >= 0 else -1.0

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    def inside(pt, a, b, c):
        d1, d2, d3 = cross(a, b, pt) * sign, cross(b, c, pt) * sign, cross(c, a, pt) * sign
        return d1 > 1e-12 and d2 > 1e-12 and d3 > 1e-12

    idx = list(range(n))
    tris = []
    guard = 0
    while len(idx) > 3 and guard < n * n:
        guard += 1
        m = len(idx)
        for i in range(m):
            a, b, c = idx[(i - 1) % m], idx[i], idx[(i + 1) % m]
            if cross(p2[a], p2[b], p2[c]) * sign <= 1e-14:
                continue  # reflex or degenerate
            if any(inside(p2[j], p2[a], p2[b], p2[c]) for j in idx if j not in (a, b, c)):
                continue
            tris.append((a, b, c))
            del idx[i]
            break
        else:
            break  # no ear found (self-intersecting); fall back to a fan for the rest
    if len(idx) >= 3:
        tris += [(idx[0], idx[i], idx[i + 1]) for i in range(1, len(idx) - 1)]
    return tris


def read_ac(path, xform=(IDENT, (0, 0, 0)), skip=None, texdirs=(), report=None, glass=None):
    """Parse an AC3D file. xform = (3x3 matrix, translation) applied to AC coordinates.
    skip(name) -> True drops an object (and its children). Returns Mesh."""
    with open(path, "r", encoding="latin-1") as f:
        toks = f.read().splitlines()
    base = os.path.dirname(path)
    mats = []
    mesh = Mesh()
    pos = [0]

    def line():
        while pos[0] < len(toks):
            s = toks[pos[0]].strip()
            pos[0] += 1
            if s:
                return s
        return None

    hdr = line()
    if not hdr.startswith("AC3D"):
        raise ValueError(f"{path}: not AC3D")

    def find_tex(name):
        for d in (base,) + tuple(texdirs):
            p = os.path.normpath(os.path.join(d, name))
            if os.path.exists(p):
                return p
        if report is not None:
            report.append(f"missing texture {name} for {os.path.basename(path)}")
        return None

    def parse_object(parent_m, parent_t, skipping):
        # at "OBJECT type"
        name, tex, texrep, texoff = "", None, (1.0, 1.0), (0.0, 0.0)
        rot, loc = IDENT, (0.0, 0.0, 0.0)
        crease = 61.0
        verts, surfs = [], []
        kids = 0
        while True:
            s = line()
            if s is None:
                break
            kw = s.split(None, 1)[0]
            rest = s[len(kw):].strip()
            if kw == "name":
                name = rest.strip('"')
            elif kw == "data":
                n = int(rest)
                # data may span lines; skip n chars
                consumed = 0
                while consumed < n:
                    consumed += len(toks[pos[0]]) + 1
                    pos[0] += 1
            elif kw == "texture":
                m = re.match(r'"([^"]*)"\s*(\w*)', rest) or re.match(r'(\S+)\s*(\w*)', rest)
                fname, layer = m.group(1), m.group(2)
                if (layer in ("", "base")) and tex is None and fname and "empty_texture" not in fname:
                    tex = find_tex(fname)
            elif kw == "texrep":
                a = rest.split(); texrep = (float(a[0]), float(a[1]))
            elif kw == "texoff":
                a = rest.split(); texoff = (float(a[0]), float(a[1]))
            elif kw == "rot":
                a = [float(x) for x in rest.split()]
                rot = [[a[0], a[3], a[6]], [a[1], a[4], a[7]], [a[2], a[5], a[8]]]
            elif kw == "loc":
                loc = tuple(float(x) for x in rest.split()[:3])
            elif kw == "crease":
                crease = float(rest)
            elif kw == "numvert":
                n = int(rest)
                for _ in range(n):
                    a = line().split()
                    verts.append((float(a[0]), float(a[1]), float(a[2])))
            elif kw == "numsurf":
                n = int(rest)
                for _ in range(n):
                    flags, mat, refs = 0, 0, []
                    while True:
                        t = line()
                        k2 = t.split()[0]
                        if k2 == "SURF":
                            flags = int(t.split()[1], 0)
                        elif k2 == "mat":
                            mat = int(t.split()[1])
                        elif k2 == "refs":
                            nr = int(t.split()[1])
                            for _ in range(nr):
                                a = line().split()
                                u = float(a[1]) if len(a) > 2 else 0.0
                                v = float(a[2]) if len(a) > 2 else 0.0
                                refs.append((int(a[0]), u, v))
                            break
                    surfs.append((flags, mat, refs))
            elif kw == "kids":
                kids = int(rest)
                break
        # AC3D: child coordinates are relative to the parent (loc then rot)
        m = _mat_mul(parent_m, rot)
        t = _apply(parent_m, parent_t, loc)
        drop = skipping or (skip is not None and skip(name))
        if not drop and verts and surfs:
            wv = [_apply(m, t, v) for v in verts]
            emit(name, wv, surfs, tex, texrep, texoff, crease, bool(glass and glass(name)))
        for _ in range(kids):
            s = line()
            if s and s.startswith("OBJECT"):
                parse_object(m, t, drop)

    def emit(name, wv, surfs, tex, texrep, texoff, crease, is_glass=False):
        cosc = math.cos(math.radians(max(1.0, crease)))
        faces = []
        polys = []
        for flags, mat, refs in surfs:
            kind = flags & 0x0F
            if len(refs) < 3:
                continue
            if kind == 0:
                polys.append((flags, mat, refs))
            elif kind == 4:  # triangle strip (Speed Dreams .acc)
                for i in range(len(refs) - 2):
                    tri = [refs[i], refs[i + 1], refs[i + 2]] if i % 2 == 0 else [refs[i + 1], refs[i], refs[i + 2]]
                    if len({r[0] for r in tri}) == 3:
                        polys.append((flags & ~0x0F, mat, tri))
            # 1/2: lines, ignored
        for flags, mat, refs in polys:
            p = [wv[r[0]] for r in refs]
            n = (0.0, 0.0, 0.0)
            for i in range(1, len(p) - 1):
                c = _cross(_sub(p[i], p[0]), _sub(p[i + 1], p[0]))
                n = (n[0] + c[0], n[1] + c[1], n[2] + c[2])
            faces.append((flags, mat, refs, _norm(n)))
        # smooth normals per vertex index
        at = {}
        for fi, (flags, mat, refs, n) in enumerate(faces):
            for r in refs:
                at.setdefault(r[0], []).append(fi)
        for flags, mat, refs, n in faces:
            mm = mats[mat] if mat < len(mats) else dict(rgb=(.8, .8, .8), trans=0.0, emis=(0, 0, 0))
            col = (*mm["rgb"], 1.0 - mm["trans"])
            if col[3] < 0.05:
                continue  # invisible (click hotspots etc.)
            unlit = sum(mm["emis"]) > 1.5
            if is_glass or col[3] < 0.99:
                # neutral, mostly clear glass (FlightGear tints some glass green for its own shaders)
                col = (0.75, 0.8, 0.82, min(col[3], 0.15))
                is_glass = True
            g = mesh.group(None if is_glass else tex, col, unlit)
            smooth = bool(flags & 0x10)
            vn = []
            for r in refs:
                if smooth:
                    acc = (0.0, 0.0, 0.0)
                    for fj in at[r[0]]:
                        n2 = faces[fj][3]
                        if n2[0]*n[0] + n2[1]*n[1] + n2[2]*n[2] >= cosc:
                            acc = (acc[0] + n2[0], acc[1] + n2[1], acc[2] + n2[2])
                    vn.append(_norm(acc))
                else:
                    vn.append(n)
            # polygons may be concave (e.g. a door panel around a window): ear clipping
            for tri in triangulate([wv[r[0]] for r in refs], n):
                for k in tri:
                    r = refs[k]
                    g["pos"].append(wv[r[0]])
                    g["nrm"].append(vn[k])
                    g["uv"].append((r[1] * texrep[0] + texoff[0], 1.0 - (r[2] * texrep[1] + texoff[1])))

    while True:
        s = line()
        if s is None:
            break
        if s.startswith("MATERIAL"):
            m = re.search(r'rgb\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+)', s)
            e = re.search(r'emis\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+)', s)
            t = re.search(r'trans\s+([\d.e+-]+)', s)
            mats.append(dict(rgb=tuple(float(x) for x in m.groups()) if m else (.8, .8, .8),
                             emis=tuple(float(x) for x in e.groups()) if e else (0, 0, 0),
                             trans=float(t.group(1)) if t else 0.0))
        elif s.startswith("OBJECT"):
            parse_object(xform[0], xform[1], False)
    return mesh


def write_gltf(mesh, out_dir, name, max_tex=2048, jpeg_quality=88, report=None):
    """Writes <out_dir>/<name>.gltf + .bin + textures/. Positions/normals are already final."""
    os.makedirs(os.path.join(out_dir, "textures"), exist_ok=True)
    buf = bytearray()
    doc = dict(asset=dict(version="2.0", generator="trackhat ac3d.py"), scene=0,
               scenes=[dict(nodes=[0])], nodes=[dict(mesh=0)], meshes=[dict(primitives=[])],
               materials=[], accessors=[], bufferViews=[], buffers=[], textures=[], images=[],
               samplers=[dict(wrapS=10497, wrapT=10497)])
    tex_index = {}

    def view(data, target):
        while len(buf) % 4:
            buf.append(0)
        off = len(buf)
        buf.extend(data)
        doc["bufferViews"].append(dict(buffer=0, byteOffset=off, byteLength=len(data), target=target))
        return len(doc["bufferViews"]) - 1

    def texture(path):
        if path in tex_index:
            return tex_index[path]
        img = Image.open(path)
        has_alpha = img.mode in ("RGBA", "LA", "PA") or (img.mode == "P" and "transparency" in img.info)
        img = img.convert("RGBA" if has_alpha else "RGB")
        if has_alpha and img.getchannel("A").getextrema()[0] == 255:
            img = img.convert("RGB"); has_alpha = False
        if max(img.size) > max_tex:
            img.thumbnail((max_tex, max_tex), Image.LANCZOS)
        stem = re.sub(r"[^A-Za-z0-9_.-]", "_", os.path.splitext(os.path.basename(path))[0])
        fn = f"textures/{stem}.{'png' if has_alpha else 'jpg'}"
        i = 1
        while any(im["uri"] == fn for im in doc["images"]):
            fn = f"textures/{stem}_{i}.{'png' if has_alpha else 'jpg'}"; i += 1
        dst = os.path.join(out_dir, fn)
        if has_alpha:
            img.save(dst, optimize=True)
        else:
            img.save(dst, "JPEG", quality=jpeg_quality, optimize=True, progressive=True)
        doc["images"].append(dict(uri=fn))
        doc["textures"].append(dict(source=len(doc["images"]) - 1, sampler=0))
        tex_index[path] = (len(doc["textures"]) - 1, has_alpha)
        return tex_index[path]

    for (tex, col, unlit), g in mesh.groups.items():
        n = len(g["pos"])
        if n == 0:
            continue
        mat = dict(pbrMetallicRoughness=dict(baseColorFactor=[min(1, max(0, c)) ** 2.2 for c in col[:3]] + [col[3]],
                                             metallicFactor=0.0, roughnessFactor=0.8), doubleSided=True)
        if tex:
            ti, alpha = texture(tex)
            mat["pbrMetallicRoughness"]["baseColorTexture"] = dict(index=ti)
            if alpha:
                mat["alphaMode"] = "MASK"; mat["alphaCutoff"] = 0.5
        if col[3] < 0.99:
            mat["alphaMode"] = "BLEND"
        if unlit:
            mat["extensions"] = {"KHR_materials_unlit": {}}
            doc.setdefault("extensionsUsed", [])
            if "KHR_materials_unlit" not in doc["extensionsUsed"]:
                doc["extensionsUsed"].append("KHR_materials_unlit")
        doc["materials"].append(mat)
        pos = struct.pack(f"<{3*n}f", *[c for p in g["pos"] for c in p])
        nrm = struct.pack(f"<{3*n}f", *[c for p in g["nrm"] for c in p])
        uv = struct.pack(f"<{2*n}f", *[c for p in g["uv"] for c in p])
        mn = [min(p[i] for p in g["pos"]) for i in range(3)]
        mx = [max(p[i] for p in g["pos"]) for i in range(3)]
        acc = []
        for data, typ in ((pos, "VEC3"), (nrm, "VEC3"), (uv, "VEC2")):
            bv = view(data, 34962)
            a = dict(bufferView=bv, componentType=5126, count=n, type=typ)
            if typ == "VEC3" and data is pos:
                a["min"], a["max"] = mn, mx
            doc["accessors"].append(a)
            acc.append(len(doc["accessors"]) - 1)
        doc["meshes"][0]["primitives"].append(dict(attributes=dict(POSITION=acc[0], NORMAL=acc[1], TEXCOORD_0=acc[2]),
                                                   material=len(doc["materials"]) - 1))
    with open(os.path.join(out_dir, f"{name}.bin"), "wb") as f:
        f.write(buf)
    doc["buffers"].append(dict(uri=f"{name}.bin", byteLength=len(buf)))
    for k in ("textures", "images"):
        if not doc[k]:
            doc.pop(k)
    if "textures" not in doc:
        doc.pop("samplers")
    with open(os.path.join(out_dir, f"{name}.gltf"), "w", encoding="utf-8") as f:
        json.dump(doc, f, separators=(",", ":"))
    tris = sum(len(g["pos"]) for g in mesh.groups.values()) // 3
    return tris
