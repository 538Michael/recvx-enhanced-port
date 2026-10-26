/* Game language (pc/src/host/lang.c): markup to the game's codes and back, translated message
   tables and built-in strings (by the English string's hash, or by place), and the letters drawn
   into a font page's free cells. */
#include "../src/host/lang.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

const void* pc_lang_table(const void* table, const char* where);
const unsigned short* pc_lang_str(const unsigned short* s, const char* where);
float pc_lang_font_width(unsigned short code);
void* pc_lang_map_model(int entry, int object, void* model);

/* message.c's advances (the US build) */
const unsigned char FontSz[126] = { 0x1c, 0x0e, 0x0e, 0x1c, 0x1c, 0x1c, 0x1c, 0x0e, 0x0e, 0x0e, 0x1c, 0x16, 0x0e, 0x14, 0x0e, 0x1c, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x0e, 0x0e, 0x1c, 0x1c, 0x1c, 0x14, 0x1c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0e,
    0x12, 0x1a, 0x14, 0x1c, 0x1a, 0x18, 0x16, 0x18, 0x18, 0x14, 0x14, 0x18, 0x16, 0x1c, 0x16, 0x16, 0x14, 0x0e, 0x1c, 0x0e, 0x1c,
    0x14, 0x1c, 0x10, 0x10, 0x0c, 0x10, 0x0e, 0x0e, 0x12, 0x12, 0x0a, 0x0c, 0x12, 0x0a, 0x18, 0x12, 0x10, 0x10, 0x10, 0x0e, 0x0e,
    0x0c, 0x12, 0x10, 0x14, 0x10, 0x10, 0x0e, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
    0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c };
void host_log(const char* fmt, ...) { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); }

static int ulen(const uint16_t* s) { int n = 0; while (s[n] != 0xFFFF) n++; return n; }

/* a table of n strings (u32 count, offsets, strings) in buf */
static void build(uint8_t* buf, const uint16_t* const* s, int n)
{
    uint32_t at = 4 + 4 * (uint32_t)n;
    memcpy(buf, &n, 4);
    for (int i = 0; i < n; i++) {
        int k = ulen(s[i]) + 1;
        memcpy(buf + 4 + 4 * i, &at, 4);
        memcpy(buf + at, s[i], (size_t)k * 2);
        at += (uint32_t)k * 2;
    }
}
static const uint16_t* entry(const void* t, int i) { const uint32_t* o = t; return (const uint16_t*)((const uint8_t*)t + o[i + 1]); }

/* a page of letter-like cells: each a box of fill with a dark outline, capitals from y 2, lower case from y 9, to the baseline at 22 */
static void page_cell(uint32_t* px, int code, int top)
{
    int cx = code % 18 * 28, cy = code / 18 * 28;
    for (int y = top; y < 22; y++)
        for (int x = 6; x < 20; x++) {
            int edge = y == top || y == 21 || x == 6 || x == 19;
            px[(cy + y) * 512 + cx + x] = edge ? 0x80484040u : 0x80B8C8C8u;   /* (alpha 0x80: opaque, as the GS has it) */
        }
}

int main(void)
{
    uint16_t out[256];
    char text[512];
    const char* err = NULL, *p;
    static uint8_t tbuf[4096];
    char json[1024];
    int n;

    /* markup: the commands, symbols, quotes, letters the game lacks */
    n = lang_encode("Use it?\n{choice}Sim  {choice}N\xC3\xA3o{p}{item}{item:12}{w:30}{c:2}{fast}{np}{default}{biohazard}\"x'", out, 256, &err);
    CHECK(n > 0);
    CHECK(out[0] == 'U' - 32 && out[3] == 0xFF01 && out[6] == '?' - 32 && out[7] == 0xFF00 && out[8] == 0xFF04);
    {
        uint16_t want[] = { 0xFF04, 'N' - 32, 0, 'o' - 32, 0xFF02, 0, 0xFF03, 0xFFFF, 0xFF03, 12, 0xFF02, 30, 0xFE03, 0xFE00, 0xFE08, 0xFE09, 3, 2, 'x' - 32, 7, 0xFFFF };
        p = "\xC3\xA3"; want[2] = (uint16_t)lang_utf8_code(&p);
        CHECK(want[2] >= 96 && !*p);
        CHECK(n == 14 + (int)(sizeof(want) / sizeof(want[0])) && !memcmp(out + 14, want, sizeof(want)));
    }
    CHECK(lang_decode(out, text, sizeof(text)) > 0);
    CHECK(!strcmp(text, "Use it?\n{choice}Sim  {choice}N\xC3\xA3o{p}{item}{item:12}{w:30}{c:2}{fast}{np}{default}{biohazard}\"x'"));
    CHECK(lang_encode("a@b", out, 256, &err) < 0 && err);          /* '@' is the circle's cell, not '@' */
    CHECK(lang_encode("\xE2\x82\xAC", out, 256, &err) < 0);         /* no euro sign */
    CHECK(lang_encode("{nope}", out, 256, &err) < 0 && lang_encode("{p", out, 256, &err) < 0);
    CHECK(lang_encode("abcdef", out, 4, &err) < 0);                 /* too long */
    p = "\xE2\x80\x9C"; n = lang_utf8_code(&p);                     /* opening quote: a drawn one */
    CHECK(n >= 96 && lang_font_width(n) == FontSz[2]);
    p = "\xC3\x89"; n = lang_utf8_code(&p);                         /* capital E acute: the width of E */
    CHECK(lang_font_width(n) == FontSz['E' - 32] && pc_lang_font_width((unsigned short)n) == FontSz['E' - 32]);
    CHECK(lang_code_utf8(n, text) == 2 && !memcmp(text, "\xC3\x89", 2));
    CHECK(lang_font_width(0xFF01) == 0 && lang_font_width(300) == 0);
    CHECK(lang_hash(out, 0) == 0xCBF29CE484222325ull);

    /* tables: English while no language is chosen */
    {
        static const uint16_t s0[] = { 'H' - 32, 'i' - 32, 0xFFFF }, s1[] = { 'D' - 32, 'o' - 32, 'o' - 32, 'r' - 32, 0xFFFF }, s2[] = { 0xFFFF },
                              s3[] = { 'H' - 32, 'i' - 32, 0xFFFF };
        static const uint16_t* const ss[] = { s0, s1, s2, s3 };
        const void* t;
        uint64_t h0 = lang_hash(s0, 2);
        build(tbuf, ss, 4);
        CHECK(pc_lang_table(tbuf, "room/RM_1000") == tbuf);
        CHECK(!lang_active() && lang_load_text("pt-BR", "not json") < 0);
        snprintf(json, sizeof(json), "{\"_name\": \"Portugues\", \"t/%016llx\": \"Ol\\u00e1\", \"room/RM_1000/1\": \"Porta\", \"t/cbf29ce484222325\": \"vazio\"}",
                 (unsigned long long)h0);
        CHECK(lang_load_text("pt-BR", json) == 4 && lang_active() && !strcmp(lang_code(), "pt-BR"));
        t = pc_lang_table(tbuf, "room/RM_1000");
        CHECK(t != tbuf && *(const uint32_t*)t == 4);
        CHECK(lang_decode(entry(t, 0), text, sizeof(text)) && !strcmp(text, "Ol\xC3\xA1"));   /* by its hash */
        CHECK(lang_decode(entry(t, 1), text, sizeof(text)) && !strcmp(text, "Porta"));        /* by its place */
        CHECK(entry(t, 2)[0] == 0xFFFF);                                                      /* empty stays empty */
        CHECK(lang_decode(entry(t, 3), text, sizeof(text)) && !strcmp(text, "Ol\xC3\xA1"));   /* the same English, the same translation */
        CHECK(entry(t, 3)[ulen(entry(t, 3)) + 1] == 0xFFFF);   /* the 1 KB after it */
        CHECK(pc_lang_table(tbuf, "room/RM_1000") == t);        /* once */
        CHECK(pc_lang_table(tbuf, "room/RM_2000") != t);        /* another place: no override, translated anew */
        CHECK(lang_decode(entry(pc_lang_table(tbuf, "room/RM_2000"), 1), text, sizeof(text)) && !strcmp(text, "Door"));
        memset(tbuf, 0, 64);
        CHECK(pc_lang_table(tbuf, "room/RM_3000") == tbuf);     /* not a table: as it is */
        CHECK(pc_lang_str(s0, "exe/pause") != s0 && lang_decode(pc_lang_str(s0, "exe/pause"), text, sizeof(text)) && !strcmp(text, "Ol\xC3\xA1"));
        CHECK(pc_lang_str(s1, "exe/retry") == s1);              /* nothing for it */
        CHECK(lang_load_text("pt-BR", "{\"t/0\": \"Ol{x\"}") == 1); /* bad markup: kept in English */
        build(tbuf, ss, 4);
        CHECK(pc_lang_table(tbuf, "sys") == tbuf);
    }

    /* the drawn letters: an accented letter is its base letter with a mark above it */
    {
        uint32_t* pg = calloc(512 * 512, 4);
        int a, aa, ink_above = 0, ink_body = 0, same = 1;
        CHECK(pg);
        for (const char* c = "HIEFTLO"; *c; c++) page_cell(pg, *c - 32, 2);
        for (const char* c = "xzvwaei"; *c; c++) page_cell(pg, *c - 32, 9);
        lang_load_text("pt-BR", "{}");
        lang_font_compose(pg, 512, 512);
        p = "a"; a = lang_utf8_code(&p);
        p = "\xC3\xA1"; aa = lang_utf8_code(&p);
        for (int y = 0; y < 28; y++)
            for (int x = 0; x < 28; x++) {
                uint32_t v = pg[(aa / 18 * 28 + y) * 512 + aa % 18 * 28 + x], b = pg[(a / 18 * 28 + y) * 512 + a % 18 * 28 + x];
                if (y < 8 && v >> 24) ink_above++;
                if (y >= 9) { ink_body += (v >> 24) != 0; same &= v == b; }
            }
        CHECK(ink_above > 4 && ink_body > 100 && same);
        free(pg);
    }

    /* the map's area labels: map.json's triangles as a chunk model like the original (map.c) */
    {
        static int32_t vl[2 + 3 * 6 + 1];
        static const int16_t pl[] = { 19 | 7 << 8, 2, 0x1234, 0x5678, 64 | 2 << 8, 5, 1, 3, 0, 1, 2, 255 };   /* material, one strip, end */
        struct { int32_t* vlist; const int16_t* plist; float center[3], r; } orig = { vl, pl, { 1, 2, 3 }, -4 }, *m;
        float nrm[3] = { 0, 1, 0 };
        vl[0] = 41 | 18 << 16; vl[1] = 3 << 16; vl[20] = 255;
        for (int i = 0; i < 3; i++) memcpy(&vl[2 + i * 6 + 3], nrm, sizeof(nrm));
        CHECK(pc_lang_map_model(65, 2, &orig) == &orig);                          /* no labels */
        CHECK(lang_load_map("{\"65/2\": \"0 0 10 0 10 -5 0 -5;0 1 2 0 2 3\", \"66/3\": \"1 2;0 1 2\"}") == 2);
        lang_load_text("", "{}");
        CHECK(pc_lang_map_model(65, 2, &orig) == &orig);                          /* English */
        lang_load_text("pt-BR", "{}");
        CHECK(pc_lang_map_model(65, 3, &orig) == &orig);                          /* not a label */
        CHECK(pc_lang_map_model(66, 3, &orig) == &orig);                          /* bad: the label stays */
        m = pc_lang_map_model(65, 2, &orig);
        CHECK(m != (void*)&orig && pc_lang_map_model(65, 2, &orig) == m);         /* once */
        CHECK(m->vlist[0] == (41 | 25 << 16) && m->vlist[1] == 4 << 16 && m->vlist[2 + 4 * 6] == 255);
        CHECK(((float*)m->vlist)[2 + 2 * 6] == 10 && ((float*)m->vlist)[2 + 2 * 6 + 2] == -5 && ((float*)m->vlist)[2 + 2 * 6 + 4] == 1);
        CHECK(!memcmp(m->plist, pl, 4 * 2));                                      /* its material */
        CHECK(m->plist[4] == (64 | 2 << 8) && m->plist[5] == 9 && m->plist[6] == 2);   /* a strip chunk: its flags, two strips */
        CHECK(m->plist[7] == 3 && m->plist[8] == 0 && m->plist[9] == 1 && m->plist[10] == 2 && m->plist[11] == 3 && m->plist[14] == 3);
        CHECK(m->plist[15] == 255 && m->center[2] == 3 && m->r == -4);
        CHECK(lang_load_map(NULL) < 0 && pc_lang_map_model(65, 2, &orig) == &orig);
    }
    printf("lang: ok\n");
    return 0;
}
