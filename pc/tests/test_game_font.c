/* game_font.c: a font page drawn here (shapes, not the game's) to a TrueType font, read back with
   ImGui's stb_truetype as the overlay reads it: the letters, the composed ones, holes, spacing */
#include "../src/movie/game_font.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

void host_log(const char* fmt, ...) { (void)fmt; }
int cdvd_read_sectors(uint32_t lsn, uint32_t count, void* dst) { (void)lsn; (void)count; (void)dst; return 0; }
int cdvd_find_file(const char* path, uint32_t* lsn, uint32_t* size) { (void)path; (void)lsn; (void)size; return 0; }

static unsigned char page[512 * 512];

/* in the cell of character c: a filled rectangle (with the game's dark outline around it), a ring */
static void rect(int c, int x0, int y0, int x1, int y1)
{
    int cx = (c - 32) % 18 * 28, cy = (c - 32) / 18 * 28;
    for (int y = y0 - 1; y <= y1 + 1; y++)
        for (int x = x0 - 1; x <= x1 + 1; x++) {
            unsigned char* p = &page[(cy + y) * 512 + cx + x];
            if (x >= x0 && x <= x1 && y >= y0 && y <= y1) *p = 200;
            else if (!*p) *p = 64;
        }
}
static void ring(int c, float mx, float my, float ro, float ri)
{
    int cx = (c - 32) % 18 * 28, cy = (c - 32) / 18 * 28;
    for (int y = 0; y < 28; y++)
        for (int x = 0; x < 28; x++) {
            float d2 = (x - mx) * (x - mx) + (y - my) * (y - my);
            if (d2 <= ro * ro && d2 >= ri * ri) page[(cy + y) * 512 + cx + x] = 200;
        }
}

int main(void)
{
    stbtt_fontinfo f;
    unsigned char* ttf;
    int size, glyphs, adv, lsb, asc, desc, gap, w, h, xo, yo;
    stbtt_vertex* v;

    CHECK(game_font_from_page(page, &size, &glyphs) == NULL && size == 0 && glyphs == 0);   /* no letters */

    for (const char* c = "HIEFLTl"; *c; c++) rect(*c, 11, 3, 16, 22);   /* capitals: rows 3..22 */
    for (const char* c = "xzvwe"; *c; c++) rect(*c, 9, 10, 18, 22);     /* x-height from row 10 */
    rect('i', 12, 11, 15, 22); rect('i', 12, 5, 15, 8);                 /* an i with its dot */
    rect('-', 8, 14, 19, 16); rect('.', 12, 19, 15, 22); rect('!', 12, 3, 15, 22);
    rect('"', 8, 3, 11, 9); rect('"', 15, 3, 18, 9);                    /* cell 2: the closing double quote */
    rect('\'', 12, 3, 15, 9);
    ring('O', 13.5f, 12.5f, 9.5f, 5.0f); ring('o', 13.5f, 16.5f, 6.0f, 2.5f);

    ttf = game_font_from_page(page, &size, &glyphs);
    CHECK(ttf && size > 1000 && glyphs > 40);
    CHECK(stbtt_InitFont(&f, ttf, stbtt_GetFontOffsetForIndex(ttf, 0)));
    CHECK(f.numGlyphs == glyphs);

    /* the page's letters, its quotes as the curly ones (and " ' too), composed letters and punctuation */
    for (const char* c = "HIOoil-.!"; *c; c++) CHECK(stbtt_FindGlyphIndex(&f, *c) != 0);
    CHECK(stbtt_FindGlyphIndex(&f, 0x201D) && stbtt_FindGlyphIndex(&f, 0x2019) && stbtt_FindGlyphIndex(&f, '"') && stbtt_FindGlyphIndex(&f, '\''));
    CHECK(stbtt_FindGlyphIndex(&f, 0x201C) && stbtt_FindGlyphIndex(&f, 0x201E));   /* “ „ */
    CHECK(stbtt_FindGlyphIndex(&f, 0xD3) && stbtt_FindGlyphIndex(&f, 0xF5) && stbtt_FindGlyphIndex(&f, 0xED));   /* Ó õ í */
    CHECK(stbtt_FindGlyphIndex(&f, 0x142) && stbtt_FindGlyphIndex(&f, 0x153) && stbtt_FindGlyphIndex(&f, 0xA1));   /* ł œ ¡ */
    CHECK(stbtt_FindGlyphIndex(&f, 0x2013) && stbtt_FindGlyphIndex(&f, 0x2026) && stbtt_FindGlyphIndex(&f, 0xAB));   /* – … « */
    CHECK(stbtt_FindGlyphIndex(&f, 'A') == 0 && stbtt_FindGlyphIndex(&f, 0xC1) == 0);   /* not on the page: neither A nor Á */

    /* outlines: the O a ring (outer and hole), Ó the O and its accent, í the dotless i and its accent */
    {
        int n = stbtt_GetCodepointShape(&f, 'O', &v), moves = 0;
        for (int i = 0; i < n; i++) moves += v[i].type == STBTT_vmove;
        stbtt_FreeShape(&f, v);
        CHECK(moves == 2);
        n = stbtt_GetCodepointShape(&f, 0xD3, &v); moves = 0;
        for (int i = 0; i < n; i++) moves += v[i].type == STBTT_vmove;
        stbtt_FreeShape(&f, v);
        CHECK(moves == 3);
        n = stbtt_GetCodepointShape(&f, 0xED, &v); moves = 0;
        for (int i = 0; i < n; i++) moves += v[i].type == STBTT_vmove;
        stbtt_FreeShape(&f, v);
        CHECK(moves == 2);   /* the i's dot dropped */
    }

    /* drawn: the O's hole empty, its ring full (10 pixels per cell pixel) */
    {
        float s = stbtt_ScaleForPixelHeight(&f, 280);
        unsigned char* bm = stbtt_GetCodepointBitmap(&f, s, s, 'O', &w, &h, &xo, &yo);
        CHECK(bm && w > 150 && w < 220 && h > 150 && h < 220);
        CHECK(bm[(h / 2) * w + w / 2] == 0);
        CHECK(bm[20 * w + w / 2] > 200 && bm[(h / 2) * w + 20] > 200);
        stbtt_FreeBitmap(bm, NULL);
    }

    /* metrics: 1000 units = the 28-pixel cell, the baseline under the capitals; spaced by the ink */
    stbtt_GetFontVMetrics(&f, &asc, &desc, &gap);
    CHECK(asc - desc == 1000 && asc > 780 && asc < 860 && gap == 0);
    stbtt_GetCodepointHMetrics(&f, ' ', &adv, &lsb);
    CHECK(adv == 232);   /* 6.5 cell pixels */
    stbtt_GetCodepointHMetrics(&f, 'I', &adv, &lsb);
    CHECK(adv > 240 && adv < 320 && lsb > 20 && lsb < 45);   /* about 6 pixels of ink and 0.9 each side */
    stbtt_GetCodepointHMetrics(&f, 0x2014, &adv, &lsb);
    CHECK(adv > 600 && adv < 700);   /* — 16 pixels long */

    free(ttf);
    printf("game_font: ok (%d glyphs, %d bytes)\n", glyphs, size);
    return 0;
}
