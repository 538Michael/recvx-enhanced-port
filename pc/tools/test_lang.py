#!/usr/bin/env python3
"""lang.py without a disc: markup to codes and back (as pc/src/host/lang.c), keys, lines and widths,
message tables, the check against made-up English strings."""
import json, os, struct, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lang  # noqa: E402

def check(cond, what):
    if not cond:
        sys.exit('FAIL: ' + what)

# markup: commands, symbols, quotes, letters the game lacks
text = 'Use it?\n{choice}Sim  {choice}Não{p}{item}{item:12}{w:30}{c:2}{fast}{np}{default}{biohazard}"x\''
codes = lang.encode(text)
check(codes[:9] == [ord('U') - 32, ord('s') - 32, ord('e') - 32, 0xFF01, ord('i') - 32, ord('t') - 32, ord('?') - 32, 0xFF00, 0xFF04], 'commands')
atilde = lang.char_code('ã')
check(atilde is not None and atilde >= lang.FIRST_EXTRA, 'a tilde drawn')
check(codes[14:] == [0xFF04, ord('N') - 32, atilde, ord('o') - 32, 0xFF02, 0, 0xFF03, 0xFFFF, 0xFF03, 12, 0xFF02, 30, 0xFE03,
                     0xFE00, 0xFE08, 0xFE09, 3, 2, ord('x') - 32, 7, 0xFFFF], 'the rest')
check(lang.decode(codes) == text, 'round trip')
for bad in ('a@b', '€', '{nope}', '{p'):
    try:
        lang.encode(bad)
        check(False, 'accepted %r' % bad)
    except ValueError:
        pass
check(lang.width(lang.char_code('É')) == lang.FONTSZ[ord('E') - 32], 'E acute as wide as E')
check(lang.width(lang.char_code('“')) == lang.FONTSZ[2], 'opening quote as the closing one')
check(lang.key_hash([]) == 't/cbf29ce484222325', 'FNV-1a of nothing')
check(lang.key_hash([ord('H') - 32, ord('i') - 32]) != lang.key_hash([ord('H') - 32]), 'keys differ')

# lines and pages: spaces 14, a page break waits, the item name counts nothing
pages = lang.layout(lang.encode('ab cd\nx{p}y{item}'))
check(pages == [[lang.width(65) * 2 + 14 + lang.width(67) + lang.width(68), lang.width(88)], [lang.width(89)]], 'layout %r' % pages)

# a message table
strs = [lang.encode(s)[:-1] for s in ('Hi', 'Door', '')]
blob = bytearray(struct.pack('<I', 3) + b'\0' * 12)
for i, s in enumerate(strs):
    struct.pack_into('<I', blob, 4 + 4 * i, len(blob))
    blob += struct.pack('<%dH' % (len(s) + 1), *(s + [0xFFFF]))
check(lang.table(bytes(blob), 0) == strs, 'table')

# the check, with made-up English strings in place of the disc's
hi, door = strs[0], strs[1]
fake = {}
for place, kd, c in (('room/RM_1000/0', 'message', hi), ('room/RM_1000/1', 'message', door), ('item/3', 'item name', door)):
    e = fake.setdefault(lang.key_hash(c), {'en': lang.decode(c), 'codes': c, 'places': [], 'kind': kd})
    e['places'].append(place)
lang.strings = lambda iso: fake
with tempfile.TemporaryDirectory() as d:
    def run(tr):
        with open(os.path.join(d, 'game.json'), 'w', encoding='utf-8') as f:
            json.dump(tr, f)
        class A: iso = __file__; langdir = d
        return lang.cmd_check(A)
    check(run({'_name': 'x', lang.key_hash(hi): 'Olá', 'room/RM_1000/1': 'Porta'}) == 0, 'a good translation')
    check(run({'t/0123456789abcdef': 'x'}) == 1, 'an unknown key')
    check(run({lang.key_hash(hi): 'a@'}) == 1, 'a character the font lacks')
    check(run({lang.key_hash(hi): 'x' * 40}) == 1, 'a line too wide')
    check(run({lang.key_hash(hi): 'a\nb\nc\nd'}) == 1, 'too many lines')

# the map's area labels: outlines with holes to triangles (maplabels.py), the box's room on a line
import maplabels  # noqa: E402
ring = maplabels.fill([[(0, 0), (10, 0), (10, 10), (0, 10)], [(3, 3), (3, 7), (7, 7), (7, 3)]])
area = sum(maplabels._cross(*t) / 2 for t in ring)
check(abs(area - 84) < 1e-6 and all(maplabels._cross(*t) > 0 for t in ring), 'a square with a hole: %r' % area)
check(not any(3 < sum(p[0] for p in t) / 3 < 7 and 3 < sum(p[1] for p in t) / 3 < 7 for t in ring), 'nothing in the hole')
hexagon = maplabels.hull([(-10, 0), (-5, -5), (5, -5), (10, 0), (5, 5), (-5, 5), (0, 0)])
check(len(hexagon) == 6 and maplabels.span(hexagon, 0) == (-10, 10) and maplabels.span(hexagon, -5) == (-5, 5), 'box span')
check(maplabels.span(hexagon, 9) is None, 'outside the box')
check(maplabels.encode([(0, 0), (1.5, -2)], [(0, 1, 0)]) == '0 0 1.5 -2;0 1 0', 'map.json value')
print('test_lang.py: ok')
