/* The game's lettering for the subtitles (docs/USER_GUIDE.md "Subtitles", Subtitle font "Game"):
   a TrueType font made at start from the disc's own font page (SYSTEM.AFS), kept in memory and
   never written anywhere. A font file the player adds (<mods>/subtitles/fonts/cvx.otf) replaces it. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* main thread, the disc image open and the game thread not started: build the font; 1 built */
int game_font_build(void);
/* the font built (TrueType), *size its bytes; NULL if none (another disc, or not built) */
const unsigned char* game_font_data(int* size);
/* the font from a font page: 512 x 512 grey levels, one byte per texel (0 background, the fill from
   about 128 up), 28 x 28 cells in ASCII order from ' '; a malloc'd TrueType font and its size and
   glyph count, NULL if the page has no letters (tests) */
unsigned char* game_font_from_page(const unsigned char* page, int* size, int* glyphs);

#ifdef __cplusplus
}
#endif
