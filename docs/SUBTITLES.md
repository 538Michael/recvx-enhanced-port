# Subtitles

The port shows subtitles you add for the movies (the pre-rendered FMVs) and the dialogue of the
real-time cutscenes. How to install them and the settings:
[USER_GUIDE.md](USER_GUIDE.md#subtitles). This page is about the files (`pc/src/movie/subtitles.c`)
and the fonts (`pc/src/movie/game_font.c`, `pc/src/host/overlay.cpp`).

## Files

All files are in the folder `subtitles` inside the `mods` folder (next to `save/`). On Linux the
names are case-sensitive.

- **Movies**: one file per movie, named after it on the disc: `mods/subtitles/MV_016.srt` for
  `MOVIE/MV_016.PSS`. The disc has `MV_000` to `MV_016` and `MV_021`; `MV_016` is the start-up
  intro, `MV_021` the ending and `MV_015` the credits. Times count from the start of the movie.
- **Cutscene dialogue**: every spoken line of the real-time cutscenes is a clip of the disc's
  `VOICE1.AFS` (525 clips) or `VOICE2.AFS` (515 clips). Their subtitles are one file per archive,
  `mods/subtitles/VOICE1.srt` and `VOICE2.srt`, where each subtitle's number is the clip's number
  and its times count from the start of that clip. A long clip can have several subtitles with the
  same number. The lines show while the clip plays, also when a line is spoken outside a cutscene.

When a movie with a file starts, or the first line of an archive is spoken, the log says
`subtitles: mods/subtitles/MV_016.srt: N subtitles`; a block it cannot read is named with its line
number and skipped.

## SRT

The files are standard SRT, saved as UTF-8 (with or without a byte order mark). A movie's file:

```
1
00:00:01,500 --> 00:00:04,000
First line
second line

2
00:00:05,000 --> 00:00:07,250
Next subtitle
```

A voice archive's file (clip 12, a line in two parts, then clip 13):

```
12
00:00:00,000 --> 00:00:02,400
First part of clip 12

12
00:00:02,600 --> 00:00:04,000
Second part of clip 12

13
00:00:00,000 --> 00:00:01,800
Clip 13
```

In a movie's file the numbers may be left out. `.` may stand for `,`. Tags such as `<i>` or
`{\an8}` are removed; the text is shown plain.

## On screen

The lines are centred at the bottom, in the black bar under a movie or a cutscene's picture, and
wrapped to the picture's width (with Movie zoom and in play, over the bottom of the picture). A
movie's lines follow it frame by frame, and with it its sound, and stay while it is paused. A
clip's lines follow the clip and may stay up to 0.6 s after it, to be read; the next line replaces
them. They are drawn over the finished picture, so they do not dim when the game fades the picture
in or out.

Long lines are broken into lines of about equal length; Japanese and Chinese, which have no spaces,
break between characters, never before closing punctuation and never inside a katakana word.

## Speakers

A subtitle may name its speaker with a tag at the start of its text, `<v Claire>A line Claire says`.
With **Speaker names** on, the line shows as "Claire: A line Claire says"; with **Speaker name
colors** too, each character's name has a colour of its own (Claire red, Steve blue, Chris green,
Alfred violet, Alexia pink, Wesker gold, Rodrigo orange). Both are off by default; a line without a
tag shows without a name.

## Languages

The SRT files are the English subtitles and hold the timings. Each other language is one file next
to them, `mods/subtitles/<code>.json`, with only text: a JSON object whose keys name subtitles and
whose values are their text in that language.

```json
{
  "_name": "Portuguese - Brazil",
  "name/Claire": "Claire",
  "name/Computer": "Computador",
  "MV_000/1": "Primeira legenda do filme.",
  "VOICE1/18/2": "Segunda legenda do clipe 18."
}
```

- `MV_000/1` is the movie's first subtitle (in time order); `VOICE1/18/2` is the second subtitle of
  clip 18 of `VOICE1.srt`.
- `_name` is the language's name in the menu (plain ASCII letters, as the game's font has no others).
- `name/<speaker>` is a speaker's name in that language.
- A subtitle or name the file lacks shows in English.

**Subtitle language** (Display) lists English and every `.json` file found at start. The choice is
kept in `recvx.ini` as `subtitle_language = pt-BR` and applies from the next line spoken. Fixing a
timing in an SRT fixes it in every language.

## Fonts

No fonts are included. **Subtitle font** "Game" uses the game's own lettering: at start the port
reads the font the game writes its messages with from your disc and makes a font of it, kept in
memory and never saved (the log says `subtitles: the game's lettering made from the disc's font`).
Letters the game has no glyph for are composed from its own (accented letters, “ ” „ « » – — …).
A font file you add, `mods/subtitles/fonts/cvx.otf` (OpenType or TrueType), is used instead.

Letters the game's lettering lacks come from the "Modern" font: a bold system font (Segoe UI Bold or
Arial on Windows; DejaVu Sans Bold, Liberation Sans Bold or Noto Sans Bold on Linux), or your own
`mods/subtitles/font.ttf` (or `font.otf`). Japanese, Korean and Chinese also need a font with their
letters:

- a font you add for the language, `mods/subtitles/fonts/<code>.otf` (for example `ja.otf`), is used
  first;
- otherwise the system's (Windows: Yu Gothic, Malgun Gothic, Microsoft YaHei, Microsoft JhengHei;
  Linux: Noto Sans CJK);
- without either, those letters show as boxes, and the log names the missing font.

Languages the game's lettering has no letters for (Japanese, Korean, Chinese, Russian and other
non-Latin scripts) use the "Modern" font throughout, even with "Game", so a line never mixes the
two. The log names every font in use.

## Timing a file

Start the game with `RECVX_MOVIE=n` (for example `RECVX_MOVIE=21`) and Skip intro off: the start-up
intro plays `MV_n` instead.
