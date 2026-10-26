"""The map's area labels (lang.py maplabels, docs/TRANSLATING.md "Map labels").

A few map floors name the areas they lead to with labels that are models, not pictures: the letters as
triangles in a floor file of MRY.AFS (map.c), drawn in palette 6 over a box drawn in palette 5. A label
in another language is set from a font's outlines, triangulated, at the English letters' height (smaller
where a line would leave its box) and centred where the English was; the port draws it instead
(pc/src/host/lang.c pc_lang_map_model). Its key is map/<MRY.AFS entry>/<object>.
"""
import math, struct

FIRST_FLOOR, LAST_FLOOR = 5, 70   # map.c: floor file = 5 + area * 6 + floor (0-4: the legend files)
OPS = {0: 0, 1: 0, 2: 0, 3: 1, 4: 1, 5: 4, 6: 3, 7: 3, 8: 1, 9: 1, 10: 1, 11: 1, 12: 2, 13: 6, 14: 2,
       15: 4, 16: 4, 17: 5, 18: 3}   # map.c MapCodeProcess: an op's argument count (shorts); 2 ends
BOX, TEXT = 5, 6                     # the palettes of a label's box and letters
PITCH = 1.23                         # the English labels' line pitch over their letters' height
MARGIN = 0.3                         # room left between a line and its box, in letter heights

# ---------------------------------------------------------------- the floor files
def _vertices(d, o):
    """chunk vertex list: {index: (x, y, z)}"""
    out = {}
    while d[o] != 255:
        typ, size = d[o], struct.unpack_from('<H', d, o + 2)[0]
        first, n = struct.unpack_from('<HH', d, o + 4)
        stride = {32: 16, 33: 32, 34: 12, 35: 16, 41: 24, 42: 28}.get(typ)
        if stride:
            for i in range(n):
                out[first + i] = struct.unpack_from('<3f', d, o + 8 + i * stride)
        o += (size + 1) * 4
    return out

def _triangles(d, o, vs):
    """chunk polygon list: its strips' triangles [((x, y, z) x 3) ...]"""
    out = []
    while d[o] != 255:
        typ = d[o]
        if typ < 8:
            o += 2
        elif typ < 16:
            o += 4
        elif typ < 64 or typ > 75:
            o += 4 + struct.unpack_from('<H', d, o + 2)[0] * 2
        else:
            size, head = struct.unpack_from('<HH', d, o + 2)
            extra = {64: 0, 65: 2, 66: 2, 67: 3, 68: 5, 69: 5, 70: 2, 71: 4, 72: 4, 73: 0, 74: 4, 75: 4}[typ]
            p = o + 6
            for _ in range(head & 0x3FFF):
                n = abs(struct.unpack_from('<h', d, p)[0])
                p += 2
                idx = []
                for k in range(n):
                    idx.append(struct.unpack_from('<H', d, p)[0])
                    p += 2 + extra * 2 + ((head >> 14) * 2 if k >= 2 else 0)
                out += [(vs[idx[k]], vs[idx[k + 1]], vs[idx[k + 2]]) for k in range(n - 2)
                        if idx[k] in vs and idx[k + 1] in vs and idx[k + 2] in vs]
            o += 4 + size * 2
    return out

def floor(d):
    """a floor file: (its code's offset, [each object's triangles]) or None"""
    if d[:4] != b'MPAC':
        return None
    p = 4 + struct.unpack_from('<i', d, 4)[0]
    nxt = struct.unpack_from('<i', d, p)[0]
    code = p + 4
    if nxt == -1:
        return code, []
    mlb = code + nxt + 4   # (the model: ninja chunk objects, bhMlbBinRealize)
    head, nobj = struct.unpack_from('<HH', d, mlb + 4)
    objs_at = struct.unpack_from('<i', d, mlb + 12)[0]
    base = mlb + head
    objs = []
    for i in range(nobj):
        model = struct.unpack_from('<i', d, base + objs_at + i * 52 + 4)[0]
        tris = []
        if model != -1 and d[mlb + 3] & 0x80:
            vl, pl = struct.unpack_from('<ii', d, base + model)
            if vl != -1 and pl != -1:
                tris = _triangles(d, base + pl, _vertices(d, base + vl))
        objs.append(tris)
    return code, objs

def draws(d, code):
    """the floor's drawing, in order: [(object, palette)] (op 3 takes an object, op 4 draws it)"""
    out, cur, n = [], None, 0
    while n < 100000:
        op = struct.unpack_from('<h', d, code)[0] & 0xFFF
        code += 2
        if op == 2 or op not in OPS:
            break
        args = struct.unpack_from('<%dh' % OPS[op], d, code)
        code += 2 * OPS[op]
        if op == 3:
            cur = args[0]
        elif op == 4:
            out.append((cur, args[0]))
        n += 1
    return out

def find(mry):
    """the labels: {'map/<entry>/<object>': {'lines': English lines, 'cap': letter height, 'centre': (x, z),
    'box': its box's outline, centred on the label}}; mry: entry -> bytes"""
    out = {}
    for e in range(FIRST_FLOOR, LAST_FLOOR + 1):
        f = floor(mry(e))
        if not f or not f[1]:
            continue
        objs = f[1]
        seq = draws(mry(e), f[0])
        for k, (o, pal) in enumerate(seq):
            if pal != TEXT or not k or seq[k - 1][1] != BOX or len(objs[o]) < 20:
                continue
            if any(p != TEXT for q, p in seq if q == o):
                continue
            xs = [v[0] for t in objs[o] for v in t]
            zs = [v[2] for t in objs[o] for v in t]
            mx, mz = (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2
            h = max(zs) - min(zs)
            lines = max(1, round(h / 29.7 + 0.2))   # (the English: letters 24.2 high, lines 29.7 apart)
            bt = objs[seq[k - 1][0]]
            bx = [v[0] for t in bt for v in t]
            bz = [v[2] for t in bt for v in t]
            dx, dz = mx - (min(bx) + max(bx)) / 2, mz - (min(bz) + max(bz)) / 2   # (the game shows each centred)
            out['map/%d/%d' % (e, o)] = {'lines': lines, 'cap': h / (1 + (lines - 1) * PITCH) / 1.02, 'centre': (mx, mz),
                                         'box': hull([(v[0] + dx, v[2] + dz) for t in bt for v in t])}
    return out

# ---------------------------------------------------------------- outlines to triangles
def _cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

def _area(c):
    return sum(c[i][0] * c[i - 1][1] - c[i - 1][0] * c[i][1] for i in range(len(c))) / -2

def _inside(pt, c):
    x, y, r = pt[0], pt[1], False
    for i in range(len(c)):
        a, b = c[i], c[i - 1]
        if (a[1] > y) != (b[1] > y) and x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0]:
            r = not r
    return r

def _in_tri(p, a, b, c):
    return _cross(a, b, p) > 0 and _cross(b, c, p) > 0 and _cross(c, a, p) > 0

def _bridge(outer, hole):
    """an outline (counter-clockwise) and a hole in it (clockwise) as one outline: joined from the hole's
    rightmost point to an outline point it sees"""
    mi = max(range(len(hole)), key=lambda i: hole[i][0])
    m = hole[mi]
    best, bx = None, math.inf
    for i in range(len(outer)):
        a, b = outer[i], outer[(i + 1) % len(outer)]
        if (a[1] - m[1]) * (b[1] - m[1]) > 0 or a[1] == b[1]:
            continue
        x = a[0] + (m[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1])
        if m[0] <= x < bx:
            bx, best = x, i if a[0] > b[0] else (i + 1) % len(outer)
    hit = (bx, m[1])
    seen = [(abs(math.atan2(q[1] - m[1], q[0] - m[0])), (q[0] - m[0]) ** 2 + (q[1] - m[1]) ** 2, i)
            for i, q in enumerate(outer) if i != best and q[0] >= m[0] and
            (_in_tri(q, m, hit, outer[best]) or _in_tri(q, m, outer[best], hit))]
    p = min(seen)[2] if seen else best   # (an outline point in the way: the one nearest in angle)
    return outer[:p + 1] + hole[mi:] + hole[:mi + 1] + outer[p:]

def _earclip(c):
    """a simple counter-clockwise outline's triangles (index triples)"""
    idx, out = list(range(len(c))), []
    while len(idx) > 3:
        n = len(idx)
        for k in range(n):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % n]
            a, b, d = c[i0], c[i1], c[i2]
            if _cross(a, b, d) > 1e-9 and not any(j not in (i0, i1, i2) and c[j] not in (a, b, d) and _in_tri(c[j], a, b, d) for j in idx):
                out.append((i0, i1, i2))
                idx.pop(k)
                break
        else:   # (no ear: a degenerate spot) the flattest point goes
            idx.pop(min(range(n), key=lambda k: abs(_cross(c[idx[k - 1]], c[idx[k]], c[idx[(k + 1) % n]]))))
    if len(idx) == 3 and _cross(c[idx[0]], c[idx[1]], c[idx[2]]) > 1e-9:
        out.append(tuple(idx))
    return out

def fill(contours):
    """closed outlines (a glyph's, even-odd) as triangles [(p, p, p) ...]"""
    depth = [sum(_inside(c[0], o) for o in contours if o is not c) for c in contours]
    outers = [c if _area(c) > 0 else c[::-1] for c, n in zip(contours, depth) if n % 2 == 0]
    holes = [c if _area(c) < 0 else c[::-1] for c, n in zip(contours, depth) if n % 2]
    out = []
    for o in outers:
        poly = o
        for h in sorted((h for h in holes if _inside(h[0], o)), key=lambda h: -max(p[0] for p in h)):
            poly = _bridge(poly, h)
        out += [tuple(poly[i] for i in t) for t in _earclip(poly)]
    return out

def hull(pts):
    pts = sorted(set(pts))
    lo, hi = [], []
    for q in pts:
        while len(lo) >= 2 and _cross(lo[-2], lo[-1], q) <= 0:
            lo.pop()
        lo.append(q)
    for q in reversed(pts):
        while len(hi) >= 2 and _cross(hi[-2], hi[-1], q) <= 0:
            hi.pop()
        hi.append(q)
    return lo[:-1] + hi[:-1]

def span(outline, z):
    """a convex outline's x range on the line z, None outside it"""
    xs = [a[0] + (z - a[1]) * (b[0] - a[0]) / (b[1] - a[1]) for a, b in zip(outline, outline[1:] + outline[:1])
          if (a[1] - z) * (b[1] - z) <= 0 and a[1] != b[1]]
    return (min(xs), max(xs)) if xs else None

# ---------------------------------------------------------------- setting a label
def font_lines(text, font_path, steps=6):
    """the text's lines in the font: ([triangles of each line, centred, baseline 0, y up], the H's height)"""
    from fontTools.ttLib import TTFont
    from fontTools.pens.basePen import BasePen

    class Flat(BasePen):
        def __init__(self, gs):
            super().__init__(gs)
            self.contours, self.cur = [], []
        def _moveTo(self, p):
            self.cur = [p]
        def _lineTo(self, p):
            self.cur.append(p)
        def _qCurveToOne(self, p1, p2):
            p0 = self.cur[-1]
            self.cur += [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
                          (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in (k / steps for k in range(1, steps + 1))]
        def _curveToOne(self, p1, p2, p3):
            p0 = self.cur[-1]
            self.cur += [tuple((1 - t) ** 3 * a + 3 * (1 - t) ** 2 * t * b + 3 * (1 - t) * t * t * c + t ** 3 * d
                               for a, b, c, d in zip(p0, p1, p2, p3)) for t in (k / steps for k in range(1, steps + 1))]
        def _closePath(self):
            pts = self.cur[:-1] if len(self.cur) > 1 and self.cur[0] == self.cur[-1] else self.cur
            out = [p for i, p in enumerate(pts) if not i or abs(p[0] - pts[i - 1][0]) + abs(p[1] - pts[i - 1][1]) > 1e-6]
            if len(out) >= 3:
                self.contours.append(out)
            self.cur = []
        _endPath = _closePath

    font = TTFont(font_path)
    gs, cmap, hmtx = font.getGlyphSet(), font.getBestCmap(), font['hmtx']

    def glyph(ch):
        if ord(ch) not in cmap:
            raise ValueError('the font has no %r' % ch)
        pen = Flat(gs)
        gs[cmap[ord(ch)]].draw(pen)
        return pen.contours, hmtx[cmap[ord(ch)]][0]

    cap = max(p[1] for c in glyph('H')[0] for p in c)   # (older fonts' OS/2 has no cap height)
    lines = []
    for line in text.split('\n'):
        x, row = 0, []
        for ch in line:
            contours, adv = glyph(ch)
            row += [tuple((p[0] + x, p[1]) for p in t) for t in fill(contours)]
            x += adv
        lines.append([tuple((p[0] - x / 2, p[1]) for p in t) for t in row])
    return lines, cap

def setting(label, text, font_path):
    """the label's text as (vertices [(x, z)], triangles [(a, b, c)], scale): the English letters' height
    and centre, smaller until every line keeps MARGIN inside the box"""
    lines, cap = font_lines(text, font_path)
    s0, cap_en = label['cap'] / cap, label['cap']
    mx, mz = label['centre']
    pitch, n, margin = PITCH * cap_en, len(lines), MARGIN * cap_en

    def place(u):
        block = ((n - 1) * pitch + cap_en) * u
        return [[tuple((mx + p[0] * s0 * u, mz + block / 2 - (n - 1 - i) * pitch * u - p[1] * s0 * u) for p in t) for t in row]
                for i, row in enumerate(lines)]

    def fits(rows):
        for row in rows:
            if not row:
                continue
            lx, rx = min(p[0] for t in row for p in t) - margin, max(p[0] for t in row for p in t) + margin
            for z in (min(p[1] for t in row for p in t), max(p[1] for t in row for p in t)):
                sp = span(label['box'], z)
                if not sp or lx < sp[0] or rx > sp[1]:
                    return False
        return True

    u = 1.0
    while u > 0.5 and not fits(place(u)):
        u -= 0.01
    verts, index, tris = [], {}, []
    for row in place(u):
        for t in row:
            ids = []
            for p in t:
                k = (round(p[0], 3), round(p[1], 3))
                if k not in index:
                    index[k] = len(verts)
                    verts.append(k)
                ids.append(index[k])
            a, b, c = (verts[i] for i in ids)
            if _cross(a, b, c) > 0:   # (facing as the English: clockwise in x, z)
                ids = [ids[0], ids[2], ids[1]]
            tris.append(tuple(ids))
    return verts, tris, u

def encode(verts, tris):
    """map.json's value: "x z x z ...;a b c a b c ..." (lang.c label_model)"""
    return ' '.join('%g %g' % v for v in verts) + ';' + ' '.join('%d %d %d' % t for t in tris)
