#!/usr/bin/env python3
"""Game language tool (docs/TRANSLATING.md): the game's English text from your disc, and a check of a
translation against it. Nothing of the game's text is in this file or written anywhere but where you ask.

  lang.py extract [--iso ISO] OUT.json     every English string once: its key, its text (markup), the
                                           places it appears, its kind and the room it has on screen
  lang.py check [--iso ISO] LANGDIR        LANGDIR/game.json against the disc: unknown keys, markup the
                                           game cannot show, lines wider or pages longer than the game's
                                           own of that kind, what is left in English
  lang.py maplabels [--iso ISO] --font F LANGDIR
                                           the map's area labels (game.json's map/... texts) set in the
                                           font F (TrueType/OpenType; needs fontTools) to LANGDIR/map.json

The disc image: --iso, else $RECVX_ISO. The markup, keys and glyphs are pc/src/host/lang.c's (read
from it here, so the two agree).
"""
import argparse, json, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import rdx  # noqa: E402
import maplabels  # noqa: E402

MAX_CODES = 4096
TERM_COLUMNS = 29  # a computer terminal's line (sp_evt.c: 29 cells of its own ASCII font)
COPY_CODES = 511   # EasyDispMessage and its kind copy 1 KB of a string (adv.c, system.c)

# ---------------------------------------------------------------- the glyphs (lang.c, message.c)
def _src(path):
    with open(os.path.join(REPO, path), encoding='utf-8') as f:
        return f.read()

def _glyphs():
    lang = _src('pc/src/host/lang.c')
    sym = re.search(r'symbols\[\] = \{(.*?)\};', lang, re.S).group(1)
    symbols = [(int(c), int(cp, 16), None if nm == 'NULL' else nm.strip('"'))
               for c, cp, nm in re.findall(r'\{ (\d+), (0x[0-9A-Fa-f]+|0), (NULL|"\w+") \}', sym)]
    ex = re.search(r'extra\[\] = \{(.*?)\};', lang, re.S).group(1)
    extra = [(int(cp, 16), base.encode().decode('unicode_escape'), mark)
             for cp, base, mark in re.findall(r"\{ 0x([0-9A-Fa-f]{4}), '(\\?.)', (\w+) \}", ex)]
    first = int(re.search(r'#define FIRST_EXTRA (\d+)', lang).group(1))
    not_ascii = re.search(r'not_ascii\[\] = "(.*?)";', lang).group(1).encode().decode('unicode_escape')
    m = re.search(r'FontSz\[126\] = \{([^}]*)\}', _src('decomp/src/ps2/veronica/prog/message.c'))
    fontsz = [int(v, 16) for v in m.group(1).split(',')]
    return symbols, extra, first, not_ascii, fontsz

SYMBOLS, EXTRA, FIRST_EXTRA, NOT_ASCII, FONTSZ = _glyphs()
COMMANDS = {'p': [0xFF02, 0], 'choice': [0xFF04], 'fast': [0xFE00], 'np': [0xFE08], 'default': [0xFE09], 'sp': [0xFF01]}

def char_code(ch):
    """a character's glyph code, None if the game's font lacks it (lang.c lang_utf8_code)"""
    cp = ord(ch)
    if ch == '"':
        return 2
    if ch == "'":
        return 7
    if 0x20 <= cp < 0x7F:
        return None if ch in NOT_ASCII else cp - 32
    for c, scp, _ in SYMBOLS:
        if scp and scp == cp:
            return c
    for i, (ecp, _, _) in enumerate(EXTRA):
        if ecp == cp:
            return FIRST_EXTRA + i
    return None

def code_char(c):
    if FIRST_EXTRA <= c < FIRST_EXTRA + len(EXTRA):
        return chr(EXTRA[c - FIRST_EXTRA][0])
    for sc, scp, _ in SYMBOLS:
        if sc == c:
            return chr(scp) if scp else None
    if 0 < c < 95 and chr(c + 32) not in NOT_ASCII:
        return chr(c + 32)
    return None

def width(c):
    """a glyph's advance (lang.c lang_font_width)"""
    if 0 <= c < FIRST_EXTRA:
        return FONTSZ[c]
    if FIRST_EXTRA <= c < FIRST_EXTRA + len(EXTRA):
        b = EXTRA[c - FIRST_EXTRA][1]
        return FONTSZ[2] if b == '"' else FONTSZ[7] if b == "'" else FONTSZ[ord(b) - 32]
    return 0

def encode(text):
    """markup -> codes ending in 0xFFFF (lang.c lang_encode); raises ValueError"""
    out, i = [], 0
    while i < len(text):
        ch = text[i]
        if ch == '\n':
            out.append(0xFF00); i += 1; continue
        if ch == ' ':
            out.append(0xFF01); i += 1; continue
        if ch == '{':
            e = text.find('}', i)
            if e < 0 or e == i + 1 or e - i - 1 >= 24:
                raise ValueError('a { without its }')
            name = text[i + 1:e]
            i = e + 1
            if name in COMMANDS:
                out += COMMANDS[name]; continue
            sym = [c for c, _, nm in SYMBOLS if nm == name]
            if sym:
                out.append(sym[0]); continue
            if name == 'item':
                out += [0xFF03, 0xFFFF]; continue
            m = re.fullmatch(r'item:(\d+)', name)
            if m and int(m.group(1)) < 0xFFFF:
                out += [0xFF03, int(m.group(1))]; continue
            m = re.fullmatch(r'w:(\d+)', name)
            if m and 0 < int(m.group(1)) < 0x10000:
                out += [0xFF02, int(m.group(1))]; continue
            m = re.fullmatch(r'c:([0-4])', name)
            if m:
                out.append(0xFE01 + int(m.group(1))); continue
            m = re.fullmatch(r'x([0-9A-Fa-f]{4})', name)
            if m and int(m.group(1), 16) != 0xFFFF:
                out.append(int(m.group(1), 16)); continue
            raise ValueError('unknown {%s}' % name)
        c = char_code(ch)
        if c is None:
            raise ValueError('%r: a character the game\'s font does not have' % ch)
        out.append(c)
        i += 1
    if len(out) >= MAX_CODES:
        raise ValueError('too long')
    return out + [0xFFFF]

def decode(codes):
    """codes (to 0xFFFF) -> markup (lang.c lang_decode)"""
    s, i = [], 0
    while i < len(codes) and codes[i] != 0xFFFF:
        c = codes[i]
        if c == 0xFF00: s.append('\n')
        elif c == 0xFF01: s.append(' ')
        elif c == 0xFF02:
            i += 1; s.append('{w:%d}' % codes[i] if codes[i] else '{p}')
        elif c == 0xFF03:
            i += 1; s.append('{item}' if codes[i] == 0xFFFF else '{item:%d}' % codes[i])
        elif c == 0xFF04: s.append('{choice}')
        elif c == 0xFE00: s.append('{fast}')
        elif 0xFE01 <= c <= 0xFE05: s.append('{c:%d}' % (c - 0xFE01))
        elif c == 0xFE08: s.append('{np}')
        elif c == 0xFE09: s.append('{default}')
        elif c == 2: s.append('"')
        elif c == 7: s.append("'")
        else:
            nm = [n for sc, _, n in SYMBOLS if sc == c and n]
            ch = None if (c == 0 or c >= 0xFE00) else code_char(c)
            s.append('{%s}' % nm[0] if nm else ch if ch else '{x%04X}' % c)
        i += 1
    return ''.join(s)

def key_hash(codes):
    """t/<FNV-1a 64 of the codes before the string's end, little-endian> (lang.c lang_hash)"""
    h = 0xCBF29CE484222325
    for c in codes:
        for b in (c & 0xFF, c >> 8):
            h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return 't/%016x' % h

def strip_end(codes):
    """the codes before the string's 0xFFFF (the numbers after 0xFF02 / 0xFF03 kept)"""
    i = 0
    while i < len(codes):
        if codes[i] == 0xFFFF:
            return codes[:i]
        i += 2 if codes[i] in (0xFF02, 0xFF03) else 1
    return codes

def layout(codes, items=None, current=0):
    """pages -> lines -> widths; an inserted item name ({item:N}) as wide as items[N] (none: 0), the
    current item's ({item}) as `current`"""
    pages, lines, w, i = [], [], 0, 0
    while i < len(codes) and codes[i] != 0xFFFF:
        c = codes[i]
        if c == 0xFF00:
            lines.append(w); w = 0
        elif c == 0xFF01:
            w += 14
        elif c == 0xFF02:
            i += 1
            if codes[i] == 0:
                lines.append(w); pages.append(lines); lines, w = [], 0
        elif c == 0xFE08:
            lines.append(w); pages.append(lines); lines, w = [], 0
        elif c == 0xFF03:
            i += 1
            w += current if codes[i] == 0xFFFF else (items or {}).get(codes[i], 0)
        elif c == 0xFF04:
            w += 28
        elif c < 0xFE00:
            w += width(c)
        i += 1
    lines.append(w)
    pages.append(lines)
    return pages

# ---------------------------------------------------------------- the disc's text
def table(d, o):
    """a message table at d[o:] (u32 count, offsets from o, u16 strings) -> [codes without the end]"""
    n = struct.unpack_from('<I', d, o)[0]
    out = []
    for i in range(n):
        p = o + struct.unpack_from('<I', d, o + 4 + 4 * i)[0]
        codes = []
        while len(codes) < MAX_CODES:
            c = struct.unpack_from('<H', d, p)[0]; p += 2
            if c == 0xFFFF:
                break
            codes.append(c)
            if c in (0xFF02, 0xFF03):
                codes.append(struct.unpack_from('<H', d, p)[0]); p += 2
        out.append(codes)
    return out

def c_array(src, name):
    """a C array's numbers from decomp source"""
    m = re.search(r'\b%s\[\d*\]\s*=\s*\{(.*?)\};' % re.escape(name), src, re.S)
    return [int(v, 0) for v in re.findall(r'0x[0-9A-Fa-f]+|\d+', m.group(1))]

def kind(where, i):
    if where == 'item':
        return 'item name'
    if where == 'sys':
        return 'description' if i < 160 else 'document' if 280 <= i < 451 else 'save screen' if i >= 451 else 'message'
    if where.startswith('room/'):
        return 'message'
    return 'menu'

def disc_strings(iso_path):
    """[(place, kind, codes)] in the game's order: sysmes.ald, the rooms, the title's, the built-in ones"""
    iso = rdx.Iso(iso_path)
    res = []
    lsn, size = iso.find('SYSMES.ALD')
    d = iso.read(lsn, (size + rdx.SECTOR - 1) // rdx.SECTOR)[:size]
    t1 = struct.unpack_from('<I', d, 0)[0]
    for where, o in (('item', 4), ('sys', 4 + t1 + 4)):
        for i, codes in enumerate(table(d, o)):
            res.append(('%s/%d' % (where, i), kind(where, i), codes))
    afs = rdx.Afs(iso, 'RDX_LNK.AFS')
    for i, name in enumerate(rdx.room_names()):
        room = rdx.Room(rdx.expand(afs.get(i)), name)
        if room.ptr['mesp']:
            where = 'room/' + name.split('.')[0]
            for k, codes in enumerate(table(room.d, room.ptr['mesp'])):
                res.append(('%s/%d' % (where, k), 'message', codes))
    adv = rdx.Afs(iso, 'ADV.AFS')
    for e in (0, 4):   # adv.c: MsgPtr, resource 0 of these entries (the title's, the option screen's)
        d = adv.get(e)
        for k, codes in enumerate(table(d, struct.unpack_from('<I', d, 0)[0])):
            res.append(('adv/%d' % k, 'menu', codes))
    src = _src('decomp/src/ps2/veronica/prog/adv.c')
    d = bytes(c_array(src, 'SaveLoadMessage'))
    for k, codes in enumerate(table(d, 0)):
        res.append(('save/%d' % k, 'menu', codes))
    for place, path, name in (('exe/pause', 'system.c', 'pause_mes'), ('exe/retry', 'gameover.c', 'cnt_mes'),
                              ('exe/demo', 'pad.c', 'pdm_mes'), ('exe/press_start', 'pad.c', 'pdm_palmes')):
        codes = c_array(_src('decomp/src/ps2/veronica/prog/' + path), name)
        res.append((place, 'menu', strip_end(codes)))
    m = re.search(r'listing_save \? "([^"]+)" : "([^"]+)"', _src('pc/glue/save_glue.c'))   # (the port's own: native saves' header)
    for place, text in (('exe/save_game', m.group(1)), ('exe/load_game', m.group(2))):
        res.append((place, 'menu', [0xFF01 if ch == ' ' else ord(ch) - 32 for ch in text]))
    res += terminal_lines()
    return res

def terminal_lines():
    """the computer terminals' lines (sp_evt.c comevt_message_tab: terminal n's array; the test one and the
    pass codes the player types left out), as codes (ASCII - 32) at term/<terminal>/<line>"""
    src = _src('decomp/src/ps2/veronica/prog/sp_evt.c')
    tab = re.search(r'comevt_message_tab\[\d+\]\s*=\s*\{(.*?)\};', src, re.S).group(1).split(',')
    tab = [t.strip() for t in tab if t.strip()]
    res = []
    for com, name in enumerate(tab):
        if com == 0 or name in tab[:com]:
            continue
        body = re.search(r'\b%s\[\d+\]\s*=\s*\{(.*?)\};' % re.escape(name), src, re.S).group(1)
        for n, line in enumerate(re.findall(r'"((?:[^"\\]|\\.)*)"', body)):
            if re.fullmatch(r'[A-Z0-9>]*', line):   # a pass code (compared with what is typed) or the prompt
                continue
            res.append(('term/%d/%d' % (com, n), 'terminal', [0xFF01 if c == ' ' else ord(c) - 32 for c in line]))
    return res

def map_labels(iso_path):
    """the map's area labels (maplabels.py): {'map/<entry>/<object>': ...}"""
    mry = rdx.Afs(rdx.Iso(iso_path), 'MRY.AFS')
    files = {}
    return maplabels.find(lambda e: files.setdefault(e, mry.get(e)))

def strings(iso_path):
    """the English strings once each: {key: {'en', 'codes', 'places', 'kind'}} in the game's order"""
    out = {}
    for place, kd, codes in disc_strings(iso_path):
        if not codes:   # empty: nothing to translate (lang.c keeps them)
            continue
        k = place if kd == 'terminal' else key_hash(codes)   # (terminal lines: by place only, lang.c pc_lang_cstr)
        e = out.setdefault(k, {'en': decode(codes), 'codes': codes, 'places': [], 'kind': kd})
        e['places'].append(place)
    return out

# what each kind has room for on screen (pixels at 640 x 480, lines on a page), from the game's own
# text: the message box 2 or 3 lines of about 500 (a few rooms' lines run longer), the inventory's
# description, the file viewer's page, the inventory's item name. The title's and the memory card's
# messages are mostly centred (some scaled down): 600, or 8% over the English's widest line, and
# the English's lines.
ROOM = {'message': (520, 3), 'description': (338, 3), 'document': (530, 12), 'item name': (300, 1), 'menu': (600, 0), 'save screen': (600, 0),
        'terminal': (TERM_COLUMNS * 16, 1)}

def item_widths(strs, tr=None):
    """{item id: the width of its name}, in the translation tr where it has one"""
    out = {}
    for k, e in strs.items():
        for p in e['places']:
            if p.startswith('item/'):
                t = (tr or {}).get(p) or (tr or {}).get(k)
                try:
                    codes = encode(t) if t else e['codes'] + [0xFFFF]
                except ValueError:
                    codes = e['codes'] + [0xFFFF]
                out[int(p[5:])] = max(layout(codes)[0])
    return out

def limit(e):
    """(width, lines a page) a translation of English string e may take"""
    pages = layout(e['codes'])
    w, n = ROOM[e['kind']]
    own = max(max(p) for p in pages)
    if e['kind'] in ('menu', 'save screen'):
        own = int(own * 1.08)
    return max(w, own), max(n, max(len(p) for p in pages))

# ---------------------------------------------------------------- commands
def iso_arg(args):
    iso = args.iso or os.environ.get('RECVX_ISO')
    if not iso or not os.path.exists(iso):
        sys.exit('disc image not found: pass --iso or set RECVX_ISO')
    return iso

def cmd_extract(args):
    strs = strings(iso_arg(args))
    rows = []
    for k, e in strs.items():
        pages = layout(e['codes'])
        w, n = limit(e)
        rows.append({'key': k, 'en': e['en'], 'places': e['places'], 'kind': e['kind'],
                     'width': max(max(p) for p in pages), 'max_width': w, 'max_lines': n})
    for k, lb in map_labels(iso_arg(args)).items():   # (models of letters: no text to show; TRANSLATING.md says which is which)
        rows.append({'key': k, 'places': [k], 'kind': 'map label', 'lines': lb['lines']})
    with open(args.out, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(rows, f, ensure_ascii=False, indent=1)
    print('%d strings (%d places) to %s' % (len(rows), sum(len(r['places']) for r in rows), args.out))

def cmd_check(args):
    strs = strings(iso_arg(args))
    places = {p: k for k, e in strs.items() for p in e['places']}
    path = os.path.join(args.langdir, 'game.json')
    with open(path, encoding='utf-8-sig') as f:
        tr = json.load(f)
    errors, warnings = [], []
    done = set()
    items = item_widths(strs, tr)
    widest = max(items.values() or [0])
    labels = None
    try:
        with open(os.path.join(args.langdir, 'map.json'), encoding='utf-8-sig') as f:
            made = json.load(f)
    except (OSError, ValueError):
        made = {}
    for key, text in tr.items():
        if key == '_name' or key.startswith('_'):
            continue
        if not isinstance(text, str):
            errors.append('%s: not a string' % key); continue
        if key.startswith('map/'):   # an area label: set into map.json by lang.py maplabels
            labels = labels if labels is not None else map_labels(iso_arg(args))
            if key not in labels:
                errors.append('%s: no such map label on the disc' % key)
            elif key[4:] not in made:
                warnings.append('%s: not in map.json yet (run lang.py maplabels)' % key)
            continue
        if key in strs:
            src = strs[key]; done.add(key)
        elif key in places:
            src = strs[places[key]]
        else:
            errors.append('%s: no such string on the disc (the English changed, or a typo)' % key); continue
        try:
            codes = encode(text)
        except ValueError as e:
            errors.append('%s: %s' % (key, e)); continue
        if src['kind'] == 'terminal':   # one line of ASCII; marks dropped (lang.c pc_lang_cstr)
            if len(text) > TERM_COLUMNS or '\n' in text or '{' in text:
                errors.append('%s: a terminal line: one line, no markup, at most %d characters' % (key, TERM_COLUMNS))
            continue
        if len(codes) > COPY_CODES and src['kind'] in ('menu', 'save screen'):
            errors.append('%s: %d codes, more than the %d the game copies' % (key, len(codes), COPY_CODES))
        w_lim, n_lim = limit(src)
        for pi, page in enumerate(layout(codes, items)):   # (the current item's name: a warning below)
            if max(layout(codes, items, widest)[pi]) > w_lim and max(page) <= w_lim:
                warnings.append('%s: page %d: a line too wide with the widest item name in {item}' % (key, pi + 1))
            if max(page) > w_lim:
                errors.append('%s: page %d: a line %d wide, a %s has room for %d' % (key, pi + 1, max(page), src['kind'], w_lim))
            if len(page) > n_lim:
                errors.append('%s: page %d: %d lines, a %s has room for %d' % (key, pi + 1, len(page), src['kind'], n_lim))
        for cmd in ('{choice}', '{item', '{p}', '{np}', '{default}'):
            if src['en'].count(cmd) != text.count(cmd):
                warnings.append('%s: %d %s in English, %d here' % (key, src['en'].count(cmd), cmd, text.count(cmd)))
    left = {}
    for k, e in strs.items():
        if k not in done and not all(p in tr for p in e['places']):
            left[e['kind']] = left.get(e['kind'], 0) + 1
    for m in errors:
        print('error: ' + m)
    for m in warnings:
        print('warning: ' + m)
    print('%s: %d of %d strings translated; still English: %s' % (path, len(done), len(strs),
          ', '.join('%d %s' % (n, k) for k, n in sorted(left.items())) or 'none'))
    return 1 if errors else 0

def cmd_maplabels(args):
    try:
        import fontTools  # noqa: F401
    except ImportError:
        sys.exit('maplabels needs fontTools (pip install fonttools)')
    with open(os.path.join(args.langdir, 'game.json'), encoding='utf-8-sig') as f:
        tr = json.load(f)
    labels = map_labels(iso_arg(args))
    out, bad = {}, 0
    for key, text in tr.items():
        if not key.startswith('map/'):
            continue
        if key not in labels:
            print('error: %s: no such map label on the disc' % key); bad = 1; continue
        try:
            verts, tris, scale = maplabels.setting(labels[key], text, args.font)
        except ValueError as e:
            print('error: %s: %s' % (key, e)); bad = 1; continue
        out[key[4:]] = maplabels.encode(verts, tris)
        print('%s: %d triangles%s' % (key, len(tris), ', letters at %d%% to fit its box' % round(scale * 100) if scale < 1 else ''))
    path = os.path.join(args.langdir, 'map.json')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(out, f, indent=0)
    print('%d of %d map labels to %s' % (len(out), len(labels), path))
    return bad

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--iso')
    sub = ap.add_subparsers(dest='cmd', required=True)
    e = sub.add_parser('extract'); e.add_argument('out')
    c = sub.add_parser('check'); c.add_argument('langdir')
    m = sub.add_parser('maplabels'); m.add_argument('--font', required=True); m.add_argument('langdir')
    args = ap.parse_args()
    sys.exit({'extract': cmd_extract, 'check': cmd_check, 'maplabels': cmd_maplabels}[args.cmd](args))

if __name__ == '__main__':
    main()
