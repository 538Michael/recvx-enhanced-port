# Translating the game

A language pack shows the game's own text and the words in its pictures in another language
(user guide, "Game language"). It holds no English: each translation is found by the hash of the
English string it replaces, read from the player's disc. `pc/tools/lang.py` lists that English from
your disc and checks a translation against it. Code: `pc/src/host/lang.c`.

## The pack

```
mods/lang/<code>/game.json     the text
mods/lang/<code>/textures/     the pictures (optional)
mods/lang/<code>/map.json      the map's area labels, made by lang.py maplabels (optional)
```

- `<code>`: letters, digits, `-` and `_`, at most 15 (`pt-BR`, `es-419`).
- `game.json`: one JSON object of strings. `"_name"` is the name the menu shows (ASCII: the menu's
  font has nothing else); the other members are translations.
- Game language lists each folder with a `game.json` at start; the language chosen is read at the
  next start.

## Keys

| Key | Translates |
|---|---|
| `t/<16 hex digits>` | an English string, wherever it appears (`lang.py extract` gives each one's key) |
| `item/N` | item name N (0..158 the items, 159..182 the files' titles) |
| `sys/N` | system message N: item descriptions, prompts, file pages (from 280), save screens (from 451) |
| `room/RM_xxxx/N` | message N of a room (`RM_1000` = stage 1, room 00, case 0) |
| `adv/N` | the title's and option screen's messages (memory card, disc) |
| `save/N` | the PS2 system data's save and load messages |
| `exe/pause`, `exe/retry`, `exe/demo`, `exe/press_start` | PAUSE, the game over's Retry?, DEMO PLAY, PRESS START |
| `exe/load_game`, `exe/save_game` | the port's header over the save list with native saves (Load Game, Save Game) |
| `term/T/N` | line N of computer terminal T (the only key these lines have) |
| `map/E/O` | an area label on the map (see "Map labels"); plain text, lines broken by hand |

The key `t/...` is the FNV-1a 64 hash of the English string's 16-bit codes (little-endian) before
its end, so a sentence the rooms repeat is translated once. A place key wins over it: use one where
the same English needs other words in one place (a door "trancada", a cabinet "trancado"). A string
with neither, or whose English a mod changed, stays English; so does every empty string.

## Markup

| Write | The game's code | Meaning |
|---|---|---|
| a new line | `FF00` | lines are broken by hand, as in the English |
| a space | `FF01` | 14 pixels |
| `{p}` | `FF02 0` | waits for a button, then a new page |
| `{w:N}` | `FF02 N` | waits N frames |
| `{item}`, `{item:N}` | `FF03` | the current item's name, or item N's |
| `{choice}` | `FF04` | a choice (Yes / No): the cursor stops there |
| `{default}` | `FE09` | the choice after it is preselected |
| `{fast}` | `FE00` | the rest at once |
| `{np}` | `FE08` | a new page without waiting |
| `{c:N}` | `FE01`..`FE05` | colour 0..4: white, cyan, red, green, grey |
| `{xNNNN}` | NNNN | any other code |

Keep the commands of the English (`lang.py check` warns when their counts differ): a `{choice}`
less leaves the game waiting for a choice that is not there.

## Letters

- ASCII, except `# $ & * < = > @ [ \ ] ^ ` { | }`: their cells hold the game's symbols.
- `"` and `'` show as ” and ’, as in the English; “ and ‘ are drawn.
- Latin-1 letters made of a letter and a mark: À Á Â Ã Ä Å Ç È É Ê Ë Ì Í Î Ï Ñ Ò Ó Ô Õ Ö Ù Ú Û Ü Ý
  and the lower case ones, ÿ. Not Æ Ð Ø Þ ß.
- The game's symbols: ” ’ × · α Σ ® ○ ▶ ▼ 「 」, `{biohazard}`, `{crown}`, `{heart}`, `{spade}`,
  `{umbrella}`.

The computer terminals type one line per key in a font of their own: plain ASCII, at most 29
characters, no markup. Their letters with marks lose them ("ação" shows as "acao"); the pass codes
the player types are not listed and stay as they are.

A drawn letter is as wide as its base letter. They are drawn at start into the font page's free
cells (codes 96 and up), on the page as decoded and on an HD pack's page.

## Room on screen

What `lang.py check` allows, in pixels of the 640 x 480 screen:

| Kind | Line | Lines a page |
|---|---|---|
| message (rooms, prompts) | 520 | 3 |
| item description | 338 | 3 |
| file page | 530 | 12 |
| item name | 300 | 1 |
| title, memory card | the English string's own | the English string's own |

A string longer than the English of its kind keeps the English's size as its limit. The title's and
memory card's messages are copied 1 KB at a time: at most 511 codes. The save screen's room names
sit in a fixed layout: keep them about as long as the English.

## Pictures

`textures/` holds pictures under PCSX2's names (`<texture>-<palette>-<bits>.png` or `.dds`), as an
HD texture pack does (HD_PACK.md; `RECVX_HD_DUMP` writes the originals under these names). A
picture keeps the original's layout, at any resolution; one drawn in several palettes needs a file
for each.

- They win over every pack in `mods/textures` and are used even with HD textures off.
- They are not compared with the original (a translated plate would fail that check).
- The port spells MULTIPLAYER, ENHANCED SETTINGS, RETURN TO TITLE, YES and NO from the letters of
  the title and option pictures; with a pack's pictures it writes them in the game's font instead.

## Map labels

A few maps name the areas they lead to in boxes at their edges. These labels are not pictures: the
letters are triangles in the map's floor models (MRY.AFS). The key is `map/<entry>/<object>`, the
floor file and the label's model in it:

| Key | Map | The label of |
|---|---|---|
| `map/65/2` | Iron Bridge | the prison |
| `map/66/3`, `map/66/5` | Passage | the training facility, the palace |
| `map/67/2`, `map/67/4` | Bridge | the palace, the residence |

Write them in `game.json` like the rest (`"map/65/2": "PRISÃO"`, lines broken by hand), then let
`lang.py maplabels` set them in a font into `map.json`, which the game reads. Any TrueType or
OpenType font with the letters will do; the English labels' look is a geometric sans-serif (such as
Century Gothic). Each label gets the English letters' height, smaller where a line would leave its
box, centred where the English was.

## The tool

```
python3 pc/tools/lang.py --iso <image> extract en.json    # every English string once: key, text, places, kind, room
python3 pc/tools/lang.py --iso <image> check mods/lang/pt-BR
python3 pc/tools/lang.py --iso <image> maplabels --font <font.ttf> mods/lang/pt-BR    # needs fontTools
```

- `extract` writes the English (keep it to yourself: it is the game's text).
- `check` reports keys that match nothing, characters the font lacks, bad markup, lines or pages
  over the room above, and how many strings are still English. It warns about map labels not yet
  in `map.json`.
- `maplabels` writes `map.json` from the `map/...` texts (`pip install fonttools`).
- The disc image can also come from `RECVX_ISO`.
