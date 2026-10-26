/* Game language (docs/TRANSLATING.md): the game's own text in another language, from
   <mods>/lang/<code>/game.json, and the letters it needs drawn into the font page's free cells.
   Pictures come from <mods>/lang/<code>/textures (hdpack.c). With English (no language chosen)
   nothing changes. The decomp asks through pc_lang_table / pc_lang_str (pc_prelude.h). */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* the languages there are (<mods>/lang/<code>/game.json): codes and menu names ("_name", ASCII), sorted by name */
int  lang_languages(char (*codes)[16], char (*names)[64], int max);
/* main thread at start, before the game thread: the language ("" or NULL: English) */
void lang_init(const char* code);
const char* lang_code(void);    /* the language in use, "" English */
int  lang_active(void);         /* a language other than English is in use */
int  lang_pictures(void);       /* ... with pictures of its own (<mods>/lang/<code>/textures): the port's entries cut
                                   from the game's menu pictures are written in the font instead */
/* tests: the translations from JSON text (an object of strings), language `code`; their count, -1 not JSON */
int  lang_load_text(const char* code, const char* json);
/* the map's area labels (map.json, read by lang_init): "<entry>/<object>": "x z ...;a b c ..."; their count, -1 not JSON */
int  lang_load_map(const char* json);

/* markup (docs/TRANSLATING.md "Markup") to game codes, 0xFFFF at the end: their count with the
   end; -1 (*err: why) if it has a character the font lacks, bad markup or more than max codes */
int  lang_encode(const char* text, uint16_t* out, int max, const char** err);
/* game codes (up to 0xFFFF) back to markup, as lang_encode reads it (tools, tests); its length */
int  lang_decode(const uint16_t* s, char* out, int max);
/* the UTF-8 character at *s (advanced past it): its glyph code, -1 if the font lacks it */
int  lang_utf8_code(const char** s);
/* a glyph code as UTF-8 (item names shown by the port: co-op, teleport); bytes written, 0 none */
int  lang_code_utf8(int code, char* out);
/* FNV-1a 64 of n codes (little-endian bytes): a translation's key is t/<16 hex digits> of it */
uint64_t lang_hash(const uint16_t* s, int n);
/* a glyph's advance (FontSz for the game's, the base letter's for the drawn ones), 0 none */
float lang_font_width(int code);
/* render thread: the extra letters drawn into font page 0's free cells. rgba: w x h pixels
   (512 x 512 as decoded, or an HD pack's page of the same layout at any scale) */
void lang_font_compose(uint32_t* rgba, int w, int h);

#ifdef __cplusplus
}
#endif
