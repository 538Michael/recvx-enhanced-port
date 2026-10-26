/*
 * Game language (lang.h, docs/TRANSLATING.md). The game draws every message, item name and
 * document from tables of 16-bit codes (message.c): a glyph is its cell on the font page (ASCII
 * - 32 on the US disc, some ASCII cells holding symbols), 0xFFxx / 0xFExx are commands. The decomp
 * hands each table to pc_lang_table as it loads it (SYSMES.ALD's item names and messages, a room's
 * messages, the title's and the memory card's) and each built-in string to pc_lang_str; they come
 * back in the language chosen: a copy whose strings are translated where game.json has them.
 *
 * game.json holds no English: a translation's key is the hash of the English string it replaces
 * (t/<16 hex digits>, so a sentence the rooms repeat is translated once), or the place of one
 * string (room/RM_1000/3, sys/152 ...) where that one must read differently. A string without a
 * translation, or changed by a mod (its hash differs), stays English.
 *
 * The US font page has the ASCII letters only; letters with accents are drawn into its free cells
 * (codes 96 and up) from the base letter and a drawn mark, on the decoded page (textures.c) and on
 * an HD pack's page (hdpack.c), after the page's HD key is taken, so packs still match.
 */
#include "lang.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../movie/subtitles.h"

void  host_log(const char* fmt, ...);
FILE* pc_fopen(const char* path, const char* mode);
extern const unsigned char FontSz[126];   /* message.c: the game's glyph advances */

#define FIRST_EXTRA 96
#define MAX_CODES 4096       /* a string's codes, at most (the longest documents have about 900) */

static char language[16];
static int pictures;
static SubPair* tr;
static int ntr;
static SubPair* maps;   /* map.json: the map's area labels */
static int nmaps;

/* ---------------------------------------------------------------- the glyphs */

/* cells of the US page that hold something other than their ASCII character */
static const struct { int code; uint32_t cp; const char* name; } symbols[] = {
    { 2, 0x201D, NULL }, { 3, 0x2623, "biohazard" }, { 4, 0x265B, "crown" }, { 6, 0x2665, "heart" },
    { 7, 0x2019, NULL }, { 10, 0x2660, "spade" }, { 28, 0x25B6, NULL }, { 29, 0x25BC, NULL },
    { 32, 0x25CB, NULL }, { 59, 0x300C, NULL }, { 60, 0x00D7, NULL }, { 61, 0x300D, NULL },
    { 62, 0x00B7, NULL }, { 91, 0x03B1, NULL }, { 92, 0x03A3, NULL }, { 93, 0, "umbrella" },
    { 95, 0x00AE, NULL },
};
static const char not_ascii[] = "#$&*<=>@[\\]^`{|}";   /* their cells hold the symbols above (or nothing) */

enum { GRAVE, ACUTE, CIRCUMFLEX, TILDE, DIAERESIS, RING, CEDILLA, TURNED };
/* the drawn letters, codes FIRST_EXTRA + index: a base cell and a mark (TURNED: the base turned, “ ‘) */
static const struct { uint16_t cp; char base; unsigned char mark; } extra[] = {
    { 0x00C0, 'A', GRAVE }, { 0x00C1, 'A', ACUTE }, { 0x00C2, 'A', CIRCUMFLEX }, { 0x00C3, 'A', TILDE }, { 0x00C4, 'A', DIAERESIS }, { 0x00C5, 'A', RING },
    { 0x00C7, 'C', CEDILLA }, { 0x00C8, 'E', GRAVE }, { 0x00C9, 'E', ACUTE }, { 0x00CA, 'E', CIRCUMFLEX }, { 0x00CB, 'E', DIAERESIS }, { 0x00CC, 'I', GRAVE },
    { 0x00CD, 'I', ACUTE }, { 0x00CE, 'I', CIRCUMFLEX }, { 0x00CF, 'I', DIAERESIS }, { 0x00D1, 'N', TILDE }, { 0x00D2, 'O', GRAVE }, { 0x00D3, 'O', ACUTE },
    { 0x00D4, 'O', CIRCUMFLEX }, { 0x00D5, 'O', TILDE }, { 0x00D6, 'O', DIAERESIS }, { 0x00D9, 'U', GRAVE }, { 0x00DA, 'U', ACUTE }, { 0x00DB, 'U', CIRCUMFLEX },
    { 0x00DC, 'U', DIAERESIS }, { 0x00DD, 'Y', ACUTE }, { 0x00E0, 'a', GRAVE }, { 0x00E1, 'a', ACUTE }, { 0x00E2, 'a', CIRCUMFLEX }, { 0x00E3, 'a', TILDE },
    { 0x00E4, 'a', DIAERESIS }, { 0x00E5, 'a', RING }, { 0x00E7, 'c', CEDILLA }, { 0x00E8, 'e', GRAVE }, { 0x00E9, 'e', ACUTE }, { 0x00EA, 'e', CIRCUMFLEX },
    { 0x00EB, 'e', DIAERESIS }, { 0x00EC, 'i', GRAVE }, { 0x00ED, 'i', ACUTE }, { 0x00EE, 'i', CIRCUMFLEX }, { 0x00EF, 'i', DIAERESIS }, { 0x00F1, 'n', TILDE },
    { 0x00F2, 'o', GRAVE }, { 0x00F3, 'o', ACUTE }, { 0x00F4, 'o', CIRCUMFLEX }, { 0x00F5, 'o', TILDE }, { 0x00F6, 'o', DIAERESIS }, { 0x00F9, 'u', GRAVE },
    { 0x00FA, 'u', ACUTE }, { 0x00FB, 'u', CIRCUMFLEX }, { 0x00FC, 'u', DIAERESIS }, { 0x00FD, 'y', ACUTE }, { 0x00FF, 'y', DIAERESIS },
    { 0x201C, '"', TURNED }, { 0x2018, '\'', TURNED },
};
#define NEXTRA ((int)(sizeof(extra) / sizeof(extra[0])))

int lang_utf8_code(const char** s)
{
    const unsigned char* p = (const unsigned char*)*s;
    uint32_t cp;
    int n = *p < 0x80 ? 1 : (*p & 0xE0) == 0xC0 ? 2 : (*p & 0xF0) == 0xE0 ? 3 : (*p & 0xF8) == 0xF0 ? 4 : 1;
    cp = n == 1 ? *p : n == 2 ? *p & 0x1F : n == 3 ? *p & 0x0F : *p & 0x07;
    for (int i = 1; i < n; i++) {
        if ((p[i] & 0xC0) != 0x80) { *s += i; return -1; }
        cp = cp << 6 | (p[i] & 0x3F);
    }
    *s += n;
    if (cp == '"') return 2;   /* (the English text's quotes: its cells hold ” and ’) */
    if (cp == '\'') return 7;
    if (cp >= 0x20 && cp < 0x7F) return strchr(not_ascii, (int)cp) ? -1 : (int)cp - 32;
    for (size_t i = 0; i < SDL_arraysize(symbols); i++) if (symbols[i].cp && symbols[i].cp == cp) return symbols[i].code;
    for (int i = 0; i < NEXTRA; i++) if (extra[i].cp == cp) return FIRST_EXTRA + i;
    return -1;
}

static int put_utf8(char* d, uint32_t cp)
{
    if (cp < 0x80) { d[0] = (char)cp; return 1; }
    if (cp < 0x800) { d[0] = (char)(0xC0 | cp >> 6); d[1] = (char)(0x80 | (cp & 63)); return 2; }
    d[0] = (char)(0xE0 | cp >> 12); d[1] = (char)(0x80 | (cp >> 6 & 63)); d[2] = (char)(0x80 | (cp & 63));
    return 3;
}

int lang_code_utf8(int code, char* out)
{
    if (code >= FIRST_EXTRA && code < FIRST_EXTRA + NEXTRA) return put_utf8(out, extra[code - FIRST_EXTRA].cp);
    for (size_t i = 0; i < SDL_arraysize(symbols); i++) if (symbols[i].code == code) return symbols[i].cp ? put_utf8(out, symbols[i].cp) : 0;
    if (code > 0 && code < 95 && !strchr(not_ascii, code + 32)) return put_utf8(out, (uint32_t)code + 32);
    return 0;
}

float lang_font_width(int code)
{
    if (code >= 0 && code < FIRST_EXTRA) return FontSz[code];
    if (code >= FIRST_EXTRA && code < FIRST_EXTRA + NEXTRA) {
        int b = extra[code - FIRST_EXTRA].base - 32;
        return extra[code - FIRST_EXTRA].base == '"' ? FontSz[2] : extra[code - FIRST_EXTRA].base == '\'' ? FontSz[7] : FontSz[b];
    }
    return 0;
}

uint64_t lang_hash(const uint16_t* s, int n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    for (int i = 0; i < n; i++) {
        h = (h ^ (s[i] & 0xFF)) * 0x100000001B3ull;
        h = (h ^ (s[i] >> 8)) * 0x100000001B3ull;
    }
    return h;
}

/* ---------------------------------------------------------------- markup */

static const struct { const char* name; uint16_t code; } commands[] = {
    { "p", 0xFF02 }, { "choice", 0xFF04 }, { "fast", 0xFE00 }, { "np", 0xFE08 }, { "default", 0xFE09 }, { "sp", 0xFF01 },
};

int lang_encode(const char* text, uint16_t* out, int max, const char** err)
{
    const char* p = text;
    int n = 0;
#define PUT(c) do { if (n >= max - 1) { *err = "too long"; return -1; } out[n++] = (uint16_t)(c); } while (0)
    while (*p) {
        if (*p == '\n') { PUT(0xFF00); p++; continue; }
        if (*p == ' ') { PUT(0xFF01); p++; continue; }
        if (*p == '{') {
            char name[24];
            const char* e = strchr(p, '}');
            size_t len = e ? (size_t)(e - p - 1) : 0;
            unsigned v = 0;
            int found = 0;
            if (!e || len == 0 || len >= sizeof(name)) { *err = "a { without its }"; return -1; }
            memcpy(name, p + 1, len); name[len] = 0;
            p = e + 1;
            for (size_t i = 0; i < SDL_arraysize(commands) && !found; i++)
                if (!strcmp(name, commands[i].name)) { PUT(commands[i].code); if (commands[i].code == 0xFF02) PUT(0); found = 1; }
            for (size_t i = 0; i < SDL_arraysize(symbols) && !found; i++)
                if (symbols[i].name && !strcmp(name, symbols[i].name)) { PUT(symbols[i].code); found = 1; }
            if (found) continue;
            if (!strcmp(name, "item")) { PUT(0xFF03); PUT(0xFFFF); continue; }
            if (sscanf(name, "item:%u", &v) == 1 && v < 0xFFFF) { PUT(0xFF03); PUT(v); continue; }
            if (sscanf(name, "w:%u", &v) == 1 && v > 0 && v < 0x10000) { PUT(0xFF02); PUT(v); continue; }
            if (sscanf(name, "c:%u", &v) == 1 && v <= 4) { PUT(0xFE01 + v); continue; }
            if (name[0] == 'x' && len == 5 && sscanf(name + 1, "%4x", &v) == 1 && v != 0xFFFF) { PUT(v); continue; }
            *err = "unknown {command}";
            return -1;
        }
        {
            int c = lang_utf8_code(&p);
            if (c < 0) { *err = "a character the game's font does not have"; return -1; }
            PUT(c);
        }
    }
    out[n++] = 0xFFFF;
    return n;
#undef PUT
}

int lang_decode(const uint16_t* s, char* out, int max)
{
    int n = 0;
    char b[16];
#define ADD(str, len) do { if (n + (len) >= max) { out[n] = 0; return n; } memcpy(out + n, str, len); n += (len); } while (0)
    for (int i = 0; s[i] != 0xFFFF && i < MAX_CODES; i++) {
        unsigned c = s[i];
        int k = 0;
        if (c == 0xFF00) k = snprintf(b, sizeof(b), "\n");
        else if (c == 0xFF01) k = snprintf(b, sizeof(b), " ");
        else if (c == 0xFF02) { c = s[++i]; k = c ? snprintf(b, sizeof(b), "{w:%u}", c) : snprintf(b, sizeof(b), "{p}"); }
        else if (c == 0xFF03) { c = s[++i]; k = c == 0xFFFF ? snprintf(b, sizeof(b), "{item}") : snprintf(b, sizeof(b), "{item:%u}", c); }
        else if (c == 0xFF04) k = snprintf(b, sizeof(b), "{choice}");
        else if (c == 0xFE00) k = snprintf(b, sizeof(b), "{fast}");
        else if (c >= 0xFE01 && c <= 0xFE05) k = snprintf(b, sizeof(b), "{c:%u}", c - 0xFE01);
        else if (c == 0xFE08) k = snprintf(b, sizeof(b), "{np}");
        else if (c == 0xFE09) k = snprintf(b, sizeof(b), "{default}");
        else if (c == 2) k = snprintf(b, sizeof(b), "\"");
        else if (c == 7) k = snprintf(b, sizeof(b), "'");
        else {
            const char* nm = NULL;
            for (size_t j = 0; j < SDL_arraysize(symbols); j++) if (symbols[j].code == (int)c && symbols[j].name) nm = symbols[j].name;
            if (nm) k = snprintf(b, sizeof(b), "{%s}", nm);
            else if (c == 0 || c >= 0xFE00 || !(k = lang_code_utf8((int)c, b))) k = snprintf(b, sizeof(b), "{x%04X}", c);
        }
        ADD(b, k);
    }
    out[n] = 0;
    return n;
#undef ADD
}

/* ---------------------------------------------------------------- the files */

static const char* mods_dir(void) { return getenv("RECVX_MODS") ? getenv("RECVX_MODS") : "mods"; }

static char* read_text(const char* path, long limit)
{
    char* text = NULL;
    long size;
    FILE* f = pc_fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > limit || !(text = calloc(1, (size_t)size + 1)) || fread(text, 1, (size_t)size, f) != (size_t)size) {
        host_log("lang: %s: cannot read it (larger than %ld MB?)\n", path, limit >> 20);
        free(text);
        text = NULL;
    }
    fclose(f);
    return text;
}

typedef struct Cached { struct Cached* next; uint64_t hash; const void* src; void* out; } Cached;
static Cached* cache;   /* the tables and strings translated (once each) */

int lang_load_text(const char* code, const char* json)
{
    while (cache) { Cached* c = cache->next; free(cache->out); free(cache); cache = c; }   /* (tests: another language) */
    subs_pairs_free(tr, ntr);
    tr = NULL;
    ntr = 0;
    snprintf(language, sizeof(language), "%s", code ? code : "");
    if (!json || (ntr = subs_json_parse(json, &tr)) < 0) { ntr = 0; return -1; }
    return ntr;
}

void lang_init(const char* code)
{
    char path[1100], *text;
    language[0] = 0;
    if (!code || !*code) return;
    snprintf(path, sizeof(path), "%s/lang/%s/game.json", mods_dir(), code);
    if (!(text = read_text(path, 16 << 20))) { host_log("lang: %s: none, the game in English\n", path); return; }
    if (lang_load_text(code, text) < 0) { host_log("lang: %s: not a JSON object of strings, the game in English\n", path); language[0] = 0; }
    else host_log("lang: %s: %d translations\n", path, ntr);
    free(text);
    if (language[0]) {
        SDL_PathInfo info;
        snprintf(path, sizeof(path), "%s/lang/%s/textures", mods_dir(), code);
        pictures = SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
        snprintf(path, sizeof(path), "%s/lang/%s/map.json", mods_dir(), code);
        if ((text = read_text(path, 16 << 20))) {
            if (lang_load_map(text) < 0) host_log("lang: %s: not a JSON object of strings (the map labels stay)\n", path);
            else host_log("lang: %s: %d map labels\n", path, nmaps);
            free(text);
        }
    }
}

const char* lang_code(void) { return language; }
int lang_active(void) { return language[0] != 0; }
int lang_pictures(void) { return language[0] && pictures; }

typedef struct { char (*codes)[16]; char (*names)[64]; int n, max; } LangList;

static SDL_EnumerationResult SDLCALL lang_cb(void* ud, const char* dir, const char* fname)
{
    LangList* l = ud;
    size_t len = strlen(fname);
    char path[1100], *text;
    SubPair* v;
    int nv;
    if (l->n >= l->max || !len || len >= 16) return SDL_ENUM_CONTINUE;
    for (size_t i = 0; i < len; i++)
        if (!(SDL_isalnum((unsigned char)fname[i]) || fname[i] == '-' || fname[i] == '_')) return SDL_ENUM_CONTINUE;
    snprintf(path, sizeof(path), "%s%s/game.json", dir, fname);
    if (!(text = read_text(path, 16 << 20))) return SDL_ENUM_CONTINUE;
    snprintf(l->codes[l->n], 16, "%s", fname);
    snprintf(l->names[l->n], 64, "%s", fname);
    if ((nv = subs_json_parse(text, &v)) > 0) {
        const char* nm = subs_pairs_get(v, nv, "_name");
        int ascii = nm && *nm;
        for (const char* s = nm; ascii && *s; s++) ascii = *s >= 32 && *s < 127;
        if (ascii) snprintf(l->names[l->n], 64, "%s", nm);   /* (the game's menu font is ASCII) */
        subs_pairs_free(v, nv);
    }
    free(text);
    l->n++;
    return SDL_ENUM_CONTINUE;
}

int lang_languages(char (*codes)[16], char (*names)[64], int max)
{
    char dir[1100];
    LangList l = { codes, names, 0, max };
    snprintf(dir, sizeof(dir), "%s/lang", mods_dir());
    SDL_EnumerateDirectory(dir, lang_cb, &l);
    for (int j = 1; j < l.n; j++)   /* by name */
        for (int i = j; i > 0 && SDL_strcasecmp(names[i - 1], names[i]) > 0; i--) {
            char c[16], nm[64];
            memcpy(c, codes[i], 16); memcpy(codes[i], codes[i - 1], 16); memcpy(codes[i - 1], c, 16);
            memcpy(nm, names[i], 64); memcpy(names[i], names[i - 1], 64); memcpy(names[i - 1], nm, 64);
        }
    return l.n;
}

/* ---------------------------------------------------------------- tables */

static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* a string's codes before its 0xFFFF (the numbers after 0xFF02 / 0xFF03 included); -1 if it does not end in limit codes */
static int str_len(const uint16_t* s, int limit)
{
    for (int i = 0; i < limit; i++) {
        if (s[i] == 0xFFFF) return i;
        if ((s[i] == 0xFF02 || s[i] == 0xFF03) && ++i >= limit) return -1;
    }
    return -1;
}

/* the translation of English string s (n codes) at `place` (room/RM_1000/3): its codes in out, their count with the end; 0 none */
static int translate(const uint16_t* s, int n, const char* place, uint16_t* out, int max)
{
    char key[64];
    const char* t, *err = NULL;
    int k;
    if (!ntr || n <= 0) return 0;   /* (an empty string stays empty: they all share one key) */
    if (!(t = subs_pairs_get(tr, ntr, place))) {
        snprintf(key, sizeof(key), "t/%016llx", (unsigned long long)lang_hash(s, n));
        if (!(t = subs_pairs_get(tr, ntr, key))) return 0;
    }
    if ((k = lang_encode(t, out, max, &err)) < 0) {
        host_log("lang: %s: %s, kept in English\n", place, err);
        return 0;
    }
    return k;
}

static uint64_t bytes_hash(const void* p, size_t n, uint64_t h)
{
    const uint8_t* b = p;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001B3ull;
    return h;
}

/* the game's message tables (sysmes.ald's two, a room's, the title's): u32 count, count u32 offsets
   from the table's start, the strings (u16 codes, 0xFFFF at the end). The copy has the same layout
   and 1 KB of 0xFF after it: EasyDispMessage and its kind copy 1 KB from a string's start. */
const void* pc_lang_table(const void* table, const char* where)
{
    const uint8_t* t = table;
    uint32_t n, head, end = 0, size, *start = NULL, *count = NULL;
    uint16_t* tx = NULL;   /* the strings as the copy has them: translated, or the English ones */
    size_t ntx = 0, cap = 0;
    uint64_t h;
    int done = 0;
    uint8_t* out = NULL;
    Cached* c;
    if (!ntr || !t || (n = rd32(t)) == 0 || n > 4096) return table;
    head = 4 + 4 * n;
    for (uint32_t i = 0; i < n; i++) {   /* a table that is not one (a room without messages) stays as it is */
        uint32_t off = rd32(t + 4 + 4 * i);
        int len;
        if (off < head || off > (8u << 20) || (off & 1) || (len = str_len((const uint16_t*)(t + off), MAX_CODES)) < 0) return table;
        if (off + 2 * (uint32_t)len + 2 > end) end = off + 2 * (uint32_t)len + 2;
    }
    h = bytes_hash(t, end, bytes_hash(where, strlen(where), 0xCBF29CE484222325ull));
    for (c = cache; c; c = c->next)
        if (c->hash == h) return c->out ? c->out : table;
    if (!(start = malloc(n * 4)) || !(count = malloc(n * 4))) goto done;
    for (uint32_t i = 0; i < n; i++) {
        const uint16_t* s = (const uint16_t*)(t + rd32(t + 4 + 4 * i));
        int len = str_len(s, MAX_CODES), k;
        char place[64];
        if (ntx + MAX_CODES > cap) {
            uint16_t* r = realloc(tx, (cap = ntx + MAX_CODES * 4) * 2);
            if (!r) goto done;
            tx = r;
        }
        snprintf(place, sizeof(place), "%s/%u", where, i);
        if ((k = translate(s, len, place, tx + ntx, MAX_CODES)) > 0) done++;
        else { memcpy(tx + ntx, s, (size_t)(len + 1) * 2); k = len + 1; }
        start[i] = (uint32_t)ntx; count[i] = (uint32_t)k;
        ntx += (size_t)k;
    }
    size = head;
    for (uint32_t i = 0; i < n; i++) size += (count[i] * 2 + 3) & ~3u;   /* (each string 4-byte aligned) */
    if (done && (out = malloc(size + 1024))) {
        uint32_t at = head;
        memset(out, 0xFF, size + 1024);
        memcpy(out, t, 4);
        for (uint32_t i = 0; i < n; i++) {
            memcpy(out + 4 + 4 * i, &at, 4);   /* (little-endian, as the game reads it) */
            memcpy(out + at, tx + start[i], count[i] * 2);
            at += (count[i] * 2 + 3) & ~3u;
        }
    }
done:
    free(start); free(count); free(tx);
    if ((c = calloc(1, sizeof(*c)))) { c->hash = h; c->src = table; c->out = out; c->next = cache; cache = c; }
    host_log("lang: %s: %d of %u strings translated\n", where, done, n);
    return out ? out : table;
}

/* the built-in strings (PAUSE, the game over's Retry?, DEMO PLAY ...): translated once */
const unsigned short* pc_lang_str(const unsigned short* s, const char* where)
{
    static uint16_t buf[MAX_CODES];
    Cached* c;
    int len, k;
    if (!ntr || !s) return s;
    for (c = cache; c; c = c->next)
        if (c->src == s && !c->hash) return c->out ? c->out : s;
    len = str_len(s, MAX_CODES);
    k = len < 0 ? 0 : translate(s, len, where, buf, MAX_CODES);
    if (!(c = calloc(1, sizeof(*c)))) return s;
    c->src = s;
    if (k > 0 && (c->out = malloc((size_t)k * 2 + 1024))) {
        memset(c->out, 0xFF, (size_t)k * 2 + 1024);
        memcpy(c->out, buf, (size_t)k * 2);
    }
    c->next = cache;
    cache = c;
    return c->out ? c->out : s;
}

float pc_lang_font_width(unsigned short code)
{
    return lang_font_width(code);
}

/* the computer terminals' lines (sp_evt.c): plain C strings in the program, typed one character
   per cell of their own 29-column font, which has ASCII only. Translated by place only
   (term/<terminal>/<line>); letters with marks lose them, other characters become spaces. */
const char* pc_lang_cstr(const char* s, int terminal, int line)
{
    char key[32], out[64];
    const char* t, *p;
    int n = 0;
    Cached* c;
    if (!ntr || !s) return s;
    for (c = cache; c; c = c->next)
        if (c->src == s && !c->hash) return c->out ? c->out : s;
    snprintf(key, sizeof(key), "term/%d/%d", terminal, line);
    t = subs_pairs_get(tr, ntr, key);
    for (p = t; p && *p && n < 29;) {
        int code = lang_utf8_code(&p);
        if (code >= FIRST_EXTRA && code < FIRST_EXTRA + NEXTRA) out[n++] = extra[code - FIRST_EXTRA].base;   /* (its base letter) */
        else if (code >= 0 && code < 95 && !strchr(not_ascii, code + 32)) out[n++] = (char)(code + 32);
        else out[n++] = ' ';
    }
    out[n] = 0;
    if (!(c = calloc(1, sizeof(*c)))) return s;
    c->src = s;
    if (t) c->out = strdup(out);
    c->next = cache;
    cache = c;
    return c->out ? c->out : s;
}

/* ---------------------------------------------------------------- the map's area labels */

/* The map names the areas next to a floor (Iron Bridge: PRISON) with labels that are models: letters
   as triangles (MRY.AFS floor files, map.c). map.json gives a label in the language as triangles in
   the label object's plane: {"<entry>/<object>": "x z x z ...;a b c a b c ..."}. The model drawn
   instead keeps the original's material, strip flags, normal and bounds, so it draws as it did. */
typedef struct { int32_t* vlist; int16_t* plist; float center[3], r; } CnkModel;   /* NJS_CNK_MODEL (ninjacnk.h) */
typedef struct MapLabel { struct MapLabel* next; int entry, object; CnkModel* model; } MapLabel;
static MapLabel* labels;

int lang_load_map(const char* json)
{
    while (labels) {
        MapLabel* l = labels->next;
        if (labels->model) { free(labels->model->vlist); free(labels->model->plist); free(labels->model); }
        free(labels);
        labels = l;
    }
    subs_pairs_free(maps, nmaps);
    maps = NULL;
    nmaps = 0;
    if (!json || (nmaps = subs_json_parse(json, &maps)) < 0) { nmaps = 0; return -1; }
    return nmaps;
}

/* the label's triangles as a chunk model like `orig` (type 41 vertices: position and normal; one strip chunk) */
static CnkModel* label_model(const char* text, const CnkModel* orig)
{
    const int16_t* p = orig->plist;
    int nv = 0, nt = 0, mat = 0, bad = 0, w, i;
    float nrm[3] = { 0, 1, 0 };
    const char* s;
    char* end;
    CnkModel* m;
    for (s = text; *s && *s != ';'; s++) if (*s == ' ') nv++;
    nv = (nv + 1) / 2;
    if (!(s = strchr(text, ';'))) return NULL;
    for (const char* q = s + 1; *q; q++) if (*q == ' ') nt++;
    nt = (nt + 1) / 3;
    if (nv < 3 || nv > 10000 || nt < 1 || nt > 16000) return NULL;   /* (the chunks' sizes are 16-bit) */
    if (((const uint8_t*)orig->vlist)[0] == 41) memcpy(nrm, (const float*)orig->vlist + 5, sizeof(nrm));   /* (its first vertex's) */
    while (p[mat] != 255 && ((const uint8_t*)&p[mat])[0] < 64) {   /* the chunks before its strips: material ... */
        int t = ((const uint8_t*)&p[mat])[0];
        mat += t < 8 ? 1 : t < 16 ? 2 : 2 + (uint16_t)p[mat + 1];
    }
    if (!(m = calloc(1, sizeof(*m))) || !(m->vlist = malloc(((size_t)nv * 6 + 3) * 4)) ||
        !(m->plist = malloc(((size_t)mat + 4 + (size_t)nt * 4) * 2))) {
        if (m) { free(m->vlist); free(m); }
        return NULL;
    }
    m->vlist[0] = 41 | (int32_t)(1 + nv * 6) << 16;
    m->vlist[1] = (int32_t)nv << 16;   /* (index offset 0) */
    for (i = 0, s = text; i < nv; i++) {
        float* v = (float*)&m->vlist[2 + i * 6];
        v[0] = strtof(s, &end); bad |= end == s; s = end;
        v[1] = 0;
        v[2] = strtof(s, &end); bad |= end == s; s = end;
        memcpy(v + 3, nrm, sizeof(nrm));
    }
    m->vlist[2 + nv * 6] = 255;
    memcpy(m->plist, p, (size_t)mat * 2);
    w = mat;
    m->plist[w++] = (int16_t)(64 | (p[mat] != 255 ? ((const uint8_t*)&p[mat])[1] : 0) << 8);   /* the original's strip flags */
    m->plist[w++] = (int16_t)(1 + nt * 4);
    m->plist[w++] = (int16_t)nt;
    for (i = 0, s = strchr(text, ';') + 1; i < nt; i++) {
        m->plist[w++] = 3;
        for (int k = 0; k < 3; k++) {
            long a = strtol(s, &end, 10);
            bad |= end == s || a < 0 || a >= nv;
            s = end;
            m->plist[w++] = (int16_t)a;
        }
    }
    m->plist[w] = 255;
    memcpy(m->center, orig->center, sizeof(m->center));
    m->r = orig->r;
    if (bad) { free(m->vlist); free(m->plist); free(m); return NULL; }
    return m;
}

void* pc_lang_map_model(int entry, int object, void* model)
{
    char key[24];
    const char* t;
    MapLabel* l;
    if (!nmaps || !model || !language[0]) return model;
    for (l = labels; l; l = l->next)
        if (l->entry == entry && l->object == object) return l->model ? l->model : model;
    snprintf(key, sizeof(key), "%d/%d", entry, object);
    if (!(t = subs_pairs_get(maps, nmaps, key))) return model;   /* (most objects: not a label) */
    if (!(l = calloc(1, sizeof(*l)))) return model;
    l->entry = entry;
    l->object = object;
    if (!(l->model = label_model(t, model))) host_log("lang: map.json %s: not \"x z ...;a b c ...\" (the label stays)\n", key);
    l->next = labels;
    labels = l;
    return l->model ? l->model : model;
}

/* ---------------------------------------------------------------- drawing the letters */

typedef struct { float x, y; } P2;
typedef struct { P2 p[28]; int n; } Poly;

static void circle(Poly* o, float cx, float cy, float r, int n)
{
    o->n = n;
    for (int i = 0; i < n; i++) { float a = 6.2831853f * i / n; o->p[i].x = cx + r * cosf(a); o->p[i].y = cy + r * sinf(a); }
}
static void from_xy(Poly* o, const float* xy, int n)
{
    o->n = n;
    for (int i = 0; i < n; i++) { o->p[i].x = xy[2 * i]; o->p[i].y = xy[2 * i + 1]; }
}
/* a mark's outlines, in cell pixels of the 512 page, relative to its foot (centre, bottom; the
   cedilla hangs from it); its count. The shapes of game_font.c's marks. */
static int mark_shape(int m, Poly* o)
{
    static const float acute[] = { -1.4f, 0, 0.2f, 0, 2.6f, -3.4f, 0.6f, -3.8f };
    static const float grave[] = { 1.4f, 0, -0.2f, 0, -2.6f, -3.4f, -0.6f, -3.8f };
    static const float circ[] = { -3.2f, 0, -1.7f, 0, 0, -2.0f, 1.7f, 0, 3.2f, 0, 0.9f, -3.6f, -0.9f, -3.6f };
    static const float cedilla[] = { -0.6f, -0.6f, 0.6f, -0.6f, 0.6f, 1.0f, 2.2f, 1.8f, 1.8f, 3.5f, -1.8f, 3.7f, -1.8f, 2.9f, 0.4f, 2.7f, 0.6f, 2.1f, -0.6f, 1.5f };
    switch (m) {
    case ACUTE: from_xy(o, acute, 4); return 1;
    case GRAVE: from_xy(o, grave, 4); return 1;
    case CIRCUMFLEX: from_xy(o, circ, 7); return 1;
    case CEDILLA: from_xy(o, cedilla, 10); return 1;
    case TILDE:   /* a wave, up on the left, down on the right */
        o->n = 26;
        for (int i = 0; i < 13; i++) {
            float t = -1 + i / 6.0f, w = -1.9f + 1.1f * sinf(3.14159265f * t);
            o->p[i].x = 3.2f * t; o->p[i].y = w - 0.75f;
            o->p[25 - i].x = 3.2f * t; o->p[25 - i].y = w + 0.75f;
        }
        return 1;
    case DIAERESIS: circle(&o[0], -2.2f, -1.6f, 1.25f, 16); circle(&o[1], 2.2f, -1.6f, 1.25f, 16); return 2;
    case RING: circle(&o[0], 0, -2.2f, 2.0f, 20); circle(&o[1], 0, -2.2f, 0.95f, 20); return 2;   /* (even-odd: the inner one is the hole) */
    }
    return 0;
}

static int inside(const Poly* o, int n, float x, float y)   /* even-odd over all the outlines */
{
    int in = 0;
    for (int k = 0; k < n; k++)
        for (int i = 0, j = o[k].n - 1; i < o[k].n; j = i++) {
            P2 a = o[k].p[i], b = o[k].p[j];
            if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) in = !in;
        }
    return in;
}
static float edge_dist(const Poly* o, int n, float x, float y)
{
    float best = 1e9f;
    for (int k = 0; k < n; k++)
        for (int i = 0, j = o[k].n - 1; i < o[k].n; j = i++) {
            P2 a = o[k].p[j], b = o[k].p[i];
            float dx = b.x - a.x, dy = b.y - a.y, l = dx * dx + dy * dy, t = l > 0 ? ((x - a.x) * dx + (y - a.y) * dy) / l : 0, ex, ey;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            ex = a.x + t * dx - x; ey = a.y + t * dy - y;
            if (ex * ex + ey * ey < best) best = ex * ex + ey * ey;
        }
    return sqrtf(best);
}

typedef struct { float r, g, b, a; } Col;
static float one = 255;   /* the page's opaque alpha: 128 as the GS has it (decoded pages, PCSX2 dumps) or 255 */
static Col col_of(uint32_t v) { Col c = { (float)(v & 255), (float)(v >> 8 & 255), (float)(v >> 16 & 255), (float)(v >> 24) / one }; if (c.a > 1) c.a = 1; return c; }
static uint32_t col_pack(Col c)
{
    int r = (int)(c.r + 0.5f), g = (int)(c.g + 0.5f), b = (int)(c.b + 0.5f), a = (int)(c.a * one + 0.5f);
    return (uint32_t)(r < 0 ? 0 : r > 255 ? 255 : r) | (uint32_t)(g < 0 ? 0 : g > 255 ? 255 : g) << 8 | (uint32_t)(b < 0 ? 0 : b > 255 ? 255 : b) << 16 |
           (uint32_t)(a < 0 ? 0 : a > 255 ? 255 : a) << 24;
}
static Col over(Col s, Col d)   /* s over d, straight alpha */
{
    Col o;
    o.a = s.a + d.a * (1 - s.a);
    if (o.a <= 0) { Col z = { 0, 0, 0, 0 }; return z; }
    o.r = (s.r * s.a + d.r * d.a * (1 - s.a)) / o.a;
    o.g = (s.g * s.a + d.g * d.a * (1 - s.a)) / o.a;
    o.b = (s.b * s.a + d.b * d.a * (1 - s.a)) / o.a;
    return o;
}

typedef struct { uint32_t* px; int w; float s, cell; } Page;

static uint32_t* at(const Page* pg, int x, int y) { return &pg->px[(size_t)y * pg->w + x]; }
/* the ink of a cell (over half opaque): bounding box in page pixels relative to the cell; 0 if empty */
static int ink(const Page* pg, int code, float* x0, float* y0, float* x1, float* y1)
{
    int cx = (int)lroundf((code % 18) * pg->cell), cy = (int)lroundf((code / 18) * pg->cell), c = (int)lroundf(pg->cell), any = 0;
    *x0 = *y0 = 1e9f; *x1 = *y1 = -1e9f;
    for (int y = 0; y < c; y++)
        for (int x = 0; x < c; x++)
            if ((*at(pg, cx + x, cy + y) >> 24) >= one / 2) {
                any = 1;
                if (x < *x0) *x0 = (float)x;
                if (x + 1 > *x1) *x1 = (float)x + 1;
                if (y < *y0) *y0 = (float)y;
                if (y + 1 > *y1) *y1 = (float)y + 1;
            }
    return any;
}

/* cell `code` turned 180 degrees about its ink's centre into cell `dst` (“ from ”) */
static void turned(Page* pg, int code, int dst, uint32_t* tmp)
{
    int c = (int)lroundf(pg->cell), sx = (int)lroundf((code % 18) * pg->cell), sy = (int)lroundf((code / 18) * pg->cell);
    int dx = (int)lroundf((dst % 18) * pg->cell), dy = (int)lroundf((dst / 18) * pg->cell);
    float x0, y0, x1, y1;
    if (!ink(pg, code, &x0, &y0, &x1, &y1)) return;
    for (int y = 0; y < c; y++)
        for (int x = 0; x < c; x++) {
            int u = (int)lroundf(x0 + x1 - 1 - x), v = (int)lroundf(y0 + y1 - 1 - y);
            tmp[y * c + x] = u >= 0 && u < c && v >= 0 && v < c ? *at(pg, sx + u, sy + v) : 0;
        }
    for (int y = 0; y < c; y++) memcpy(at(pg, dx, dy + y), &tmp[y * c], (size_t)c * 4);
}

void lang_font_compose(uint32_t* rgba, int w, int h)
{
    Page pg = { rgba, w, w / 512.0f, 28 * (w / 512.0f) };
    int c = (int)lroundf(pg.cell);
    float cap = 1e9f, base = -1, xh = 1e9f, x0, y0, x1, y1, fill_n = 0, line_n = 0;
    Col fill = { 0, 0, 0, 1 }, line = { 0, 0, 0, 1 };
    uint32_t* tmp;
    if (w < 512 || w != h || (w % 512 && w * 2 % 512 && w * 4 % 512)) {   /* (512 times 1, 1.5, 2, 4 ... the cells on whole pixels) */
        host_log("lang: a font page of %d x %d: not the game's layout, no letters drawn\n", w, h);
        return;
    }
    if (lroundf((FIRST_EXTRA + NEXTRA - 1) / 18 * pg.cell + pg.cell) > h || !(tmp = malloc((size_t)c * c * 4 * 2))) return;
    one = 0;   /* (render thread only) */
    for (int code = 33; code <= 90; code++)
        for (int y = 0; y < c; y++)
            for (int x = 0; x < c; x++) {
                float a = (float)(*at(&pg, (int)lroundf((code % 18) * pg.cell) + x, (int)lroundf((code / 18) * pg.cell) + y) >> 24);
                if (a > one) one = a;
            }
    if (one < 16) { free(tmp); host_log("lang: a font page of %d x %d without letters, none drawn\n", w, h); return; }
    for (const char* p = "HIEFT"; *p; p++) if (ink(&pg, *p - 32, &x0, &y0, &x1, &y1)) { if (y0 < cap) cap = y0; if (y1 > base) base = y1; }
    for (const char* p = "xzvw"; *p; p++) if (ink(&pg, *p - 32, &x0, &y0, &x1, &y1) && y0 < xh) xh = y0;
    if (base < 0 || xh > 1e8f) { free(tmp); host_log("lang: a font page of %d x %d without letters, none drawn\n", w, h); return; }
    {   /* the fill and outline colours: the brightest and the darkest quarter of the letters' opaque pixels */
        int hist[256] = { 0 }, total = 0, lo = 0, hi = 255, k;
        for (int code = 33; code <= 90; code++)
            for (int y = 0; y < c; y++)
                for (int x = 0; x < c; x++) {
                    uint32_t v = *at(&pg, (int)lroundf((code % 18) * pg.cell) + x, (int)lroundf((code / 18) * pg.cell) + y);
                    if ((v >> 24) >= one * 0.78f) { hist[v & 255]++; total++; }
                }
        for (k = 0; lo < total / 4 && k < 256; k++) lo += hist[k];
        lo = k - 1;
        for (k = 255, hi = 0; hi < total / 4 && k >= 0; k--) hi += hist[k];
        hi = k + 1;
        for (int code = 33; code <= 90; code++)
            for (int y = 0; y < c; y++)
                for (int x = 0; x < c; x++) {
                    uint32_t v = *at(&pg, (int)lroundf((code % 18) * pg.cell) + x, (int)lroundf((code / 18) * pg.cell) + y);
                    Col q = col_of(v);
                    if ((v >> 24) < one * 0.78f) continue;
                    if ((int)(v & 255) >= hi) { fill.r += q.r; fill.g += q.g; fill.b += q.b; fill_n++; }
                    if ((int)(v & 255) <= lo) { line.r += q.r; line.g += q.g; line.b += q.b; line_n++; }
                }
        if (fill_n) { fill.r /= fill_n; fill.g /= fill_n; fill.b /= fill_n; }
        if (line_n) { line.r /= line_n; line.g /= line_n; line.b /= line_n; }
    }
    for (int i = 0; i < NEXTRA; i++) {
        int code = FIRST_EXTRA + i, b = extra[i].base == '"' ? 2 : extra[i].base == '\'' ? 7 : extra[i].base - 32, m = extra[i].mark;
        int upper = extra[i].base >= 'A' && extra[i].base <= 'Z', dx = (int)lroundf((code % 18) * pg.cell), dy = (int)lroundf((code / 18) * pg.cell);
        int bx = (int)lroundf((b % 18) * pg.cell), by = (int)lroundf((b / 18) * pg.cell), np;
        int bx0, by0, bx1, by1;
        float s = pg.s, k = 1, foot, cx, fy = upper ? 0.7f : 1, mtop = 0;
        Poly o[2];
        for (int y = 0; y < c; y++) memset(at(&pg, dx, dy + y), 0, (size_t)c * 4);
        if (m == TURNED) { turned(&pg, b, code, tmp); continue; }
        if (!ink(&pg, b, &x0, &y0, &x1, &y1)) continue;
        np = mark_shape(m, o);
        for (int q = 0; q < np; q++) for (int j = 0; j < o[q].n; j++) if (o[q].p[j].y * fy < mtop) mtop = o[q].p[j].y * fy;
        cx = (x0 + x1) / 2 + (m == ACUTE ? 0.4f * s : m == GRAVE ? -0.4f * s : 0);
        if (m == CEDILLA) foot = base - 1.0f * s;   /* (base: the outline's bottom; the mark hangs from the fill's) */
        else if (upper) {   /* no room above a capital: it is squeezed down to its baseline, the mark over it */
            foot = 1.2f * s - mtop * s;
            k = (base - (foot + 1.2f * s - 1.0f * s)) / (base - cap);
            if (k > 1) k = 1;
        } else foot = xh - 0.3f * s;
        for (int y = 0; y < c; y++)   /* the base letter (squeezed: rows taken from base - (base - y) / k, interpolated) */
            for (int x = 0; x < c; x++) {
                float sy = base - (base - (y + 0.5f)) / k - 0.5f;
                int y0i = (int)floorf(sy);
                float f = sy - y0i;
                Col a = { 0, 0, 0, 0 }, d = { 0, 0, 0, 0 }, r;
                if (y0i >= 0 && y0i < c) a = col_of(*at(&pg, bx + x, by + y0i));
                if (y0i + 1 >= 0 && y0i + 1 < c) d = col_of(*at(&pg, bx + x, by + y0i + 1));
                r.a = a.a * (1 - f) + d.a * f;   /* (premultiplied, so the clear pixels do not darken the edge) */
                r.r = r.a > 0 ? (a.r * a.a * (1 - f) + d.r * d.a * f) / r.a : 0;
                r.g = r.a > 0 ? (a.g * a.a * (1 - f) + d.g * d.a * f) / r.a : 0;
                r.b = r.a > 0 ? (a.b * a.a * (1 - f) + d.b * d.a * f) / r.a : 0;
                *at(&pg, dx + x, dy + y) = col_pack(r);
            }
        if (extra[i].base == 'i' && m != CEDILLA) {   /* dotless: the dot (ink wholly above the x-height) cleared */
            for (int y = 0; y < c && y < (int)xh - (int)s; y++) memset(at(&pg, dx, dy + y), 0, (size_t)c * 4);
        }
        for (int q = 0; q < np; q++)   /* to page pixels, cell-relative */
            for (int j = 0; j < o[q].n; j++) { o[q].p[j].x = cx + o[q].p[j].x * s; o[q].p[j].y = foot + o[q].p[j].y * s * fy; }
        {   /* the mark's box, its outline included */
            float mx0 = 1e9f, my0 = 1e9f, mx1 = -1e9f, my1 = -1e9f;
            for (int q = 0; q < np; q++)
                for (int j = 0; j < o[q].n; j++) {
                    if (o[q].p[j].x < mx0) mx0 = o[q].p[j].x;
                    if (o[q].p[j].x > mx1) mx1 = o[q].p[j].x;
                    if (o[q].p[j].y < my0) my0 = o[q].p[j].y;
                    if (o[q].p[j].y > my1) my1 = o[q].p[j].y;
                }
            bx0 = (int)floorf(mx0 - s - 1); by0 = (int)floorf(my0 - s - 1); bx1 = (int)ceilf(mx1 + s + 1); by1 = (int)ceilf(my1 + s + 1);
            if (bx0 < 0) bx0 = 0;
            if (by0 < 0) by0 = 0;
            if (bx1 > c) bx1 = c;
            if (by1 > c) by1 = c;
        }
        for (int y = by0; y < by1; y++)
            for (int x = bx0; x < bx1; x++) {
                int fi = 0, li = 0;
                Col d, q;
                for (int sy = 0; sy < 4; sy++)
                    for (int sx = 0; sx < 4; sx++) {
                        float px = x + (sx + 0.5f) / 4, py = y + (sy + 0.5f) / 4;
                        int in = inside(o, np, px, py);
                        fi += in;
                        li += in || edge_dist(o, np, px, py) <= 1.0f * s;
                    }
                if (!li) continue;
                d = col_of(*at(&pg, dx + x, dy + y));
                q = line; q.a = li / 16.0f;
                d = over(d, q);   /* the outline behind the letter, the fill over both */
                q = fill; q.a = fi / 16.0f;
                d = over(q, d);
                *at(&pg, dx + x, dy + y) = col_pack(d);
            }
    }
    free(tmp);
    host_log("lang: %d letters drawn into a font page of %d x %d\n", NEXTRA, w, h);
}
