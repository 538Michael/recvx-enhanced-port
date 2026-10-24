/*
 * Subtitles (subtitles.h): the SRT cues of the open movie and of the voice clips playing. The game
 * thread loads them (a movie when it opens, a voice archive's file at its first clip) and starts
 * and stops the voice tracks; the render thread looks up what it shows and copies the text out,
 * all under one mutex. A movie opened while the render thread still presents the previous one's
 * last frame (one game frame at most) is looked up in the new cues; movies never follow each
 * other within a frame, so that frame shows nothing or the new movie's first lines. Voice clips
 * are timed by the wall clock from their start: the retail ADXT_Pause is empty, so a clip plays
 * from ADXT_StartAfs until it ends or is stopped (pc/src/audio/adx.c).
 *
 * Languages: the SRTs hold the timings and the English text; <mods>/subtitles/<code>.json replaces
 * the text of the subtitles it has, by key ("MV_000/3": the movie's 3rd subtitle in time order,
 * "VOICE1/18/2": clip 18's 2nd), and names the speakers ("name/Claire"). The game thread applies it
 * as a file loads; a language chosen while playing (subs_set_language, any thread) reloads the voice
 * archives at the next clip and reaches a movie at its next start.
 */
#include "subtitles.h"
#include "../host/fileio.h"
#include <SDL3/SDL.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void host_log(const char* fmt, ...);

#define MAX_ARCHIVES 8
#define MAX_TRACKS 8
#define LINGER_MS 600   /* a clip's last line may stay this long after its end, to be read (the next clip on its slot replaces it) */

static SDL_Mutex* lock;   /* cues, ncues, track[], language; created at start (subs_set_language) or with the first movie or clip */
static SubCue* cues;
static int ncues;
static char parsing[48];  /* the file being parsed, for parse_log (game thread) */
/* voice archives (game thread; never freed while a track can point into them, so the render thread
   reads their text under the lock only) and the clips playing, by ADXT slot */
static struct { char name[32]; SubCue* c; int n; } archive[MAX_ARCHIVES];
static int narchives;
static struct { const SubCue* c; int n, dur_ms; uint64_t start_ns; } track[MAX_TRACKS];
/* the language: its code and generation (subs_set_language), and on the game thread the table
   loaded for the generation `loaded` */
static char language[16];
static int language_gen, loaded;
static SubPair* tr;
static int ntr;

static int blank(const char* s) { return !s[strspn(s, " \t")]; }

/* the cue's number: digits only */
static int is_number(const char* s)
{
    s += strspn(s, " \t");
    if (*s < '0' || *s > '9') return 0;
    s += strspn(s, "0123456789");
    return blank(s);
}

/* [HH:]MM:SS[,mmm] ('.' for ',' too) */
static int parse_time(const char** s, int* ms)
{
    const char* p = *s + strspn(*s, " \t");
    int64_t part[3], t;
    int np = 0, frac = 0, digits = 0;
    for (;;) {
        if (*p < '0' || *p > '9') return 0;
        part[np] = 0;
        while (*p >= '0' && *p <= '9') { if (part[np] < 100000) part[np] = part[np] * 10 + (*p - '0'); p++; }
        if (++np == 3 || *p != ':') break;
        p++;
    }
    if (np < 2) return 0;
    if (*p == ',' || *p == '.') {
        for (p++; *p >= '0' && *p <= '9'; p++) if (digits < 3) { frac = frac * 10 + (*p - '0'); digits++; }
        for (; digits && digits < 3; digits++) frac *= 10;
    }
    t = ((np == 3 ? part[0] * 3600 : 0) + part[np - 2] * 60 + part[np - 1]) * 1000 + frac;
    if (t > 100 * 3600 * 1000) return 0;
    *ms = (int)t;
    *s = p;
    return 1;
}

/* "start --> end"; anything after the end (positions) is ignored */
static int time_line(const char* s, int* a, int* b)
{
    if (!parse_time(&s, a)) return 0;
    s += strspn(s, " \t");
    if (strncmp(s, "-->", 3)) return 0;
    s += 3;
    return parse_time(&s, b);
}

/* line s added to the cue's text t (len bytes so far), without tags (<i>, </font>, {\an8}) and
   the spaces around it; an empty line adds nothing */
static char* append(char* t, size_t* len, const char* s)
{
    char* r = realloc(t, *len + strlen(s) + 2);
    size_t k, start;
    if (!r) return t;
    start = k = *len + (*len ? 1 : 0);
    for (s += strspn(s, " \t"); *s; s++) {
        const char* e = *s == '<' ? strchr(s, '>') : *s == '{' && s[1] == '\\' ? strchr(s, '}') : NULL;
        if (e) { s = e; continue; }
        r[k++] = *s;
    }
    while (k > start && (r[k - 1] == ' ' || r[k - 1] == '\t')) k--;
    if (k > start) { if (*len) r[*len] = '\n'; *len = k; }
    r[*len] = 0;
    return r;
}

/* a leading speaker tag "<v Claire>" of a cue's first line: the name (malloc'd), *s past the tag */
static char* speaker(const char** s)
{
    const char* p = *s + strspn(*s, " \t"), *e;
    size_t n;
    char* who;
    if (strncmp(p, "<v ", 3) || !(e = strchr(p, '>'))) return NULL;
    p += 3 + strspn(p + 3, " ");
    for (n = (size_t)(e - p); n && p[n - 1] == ' '; n--) {}
    if (!n || !(who = malloc(n + 1))) return NULL;
    memcpy(who, p, n);
    who[n] = 0;
    *s = e + 1;
    return who;
}

int subs_parse(const char* text, SubCue** out, void (*log)(int line, const char* msg))
{
    char* buf = strdup(text), *p, **line = NULL;
    int nl = 0, cl = 0, n = 0, cap = 0, i = 0, a, b;
    SubCue* c = NULL;
    *out = NULL;
    if (!buf) return 0;
    /* lines, without a byte order mark and '\r' */
    for (p = buf + (strncmp(buf, "\xEF\xBB\xBF", 3) ? 0 : 3);;) {
        char* e = p + strcspn(p, "\r\n"), *next = e + (*e ? 1 : 0) + (e[0] == '\r' && e[1] == '\n');
        if (nl == cl) {
            char** r = realloc(line, (cl = cl ? cl * 2 : 256) * sizeof(*line));
            if (!r) break;
            line = r;
        }
        line[nl++] = p;
        if (!*e) break;
        *e = 0;
        p = next;
    }
    while (i < nl) {
        int first = i, id = -1;
        char* t = NULL, *who = NULL;
        size_t len = 0;
        if (blank(line[i])) { i++; continue; }
        if (i + 1 < nl && is_number(line[i])) {   /* the cue's number (optional; a voice archive's clip) */
            long v = strtol(line[i++], NULL, 10);
            id = v > INT_MAX ? INT_MAX : (int)v;
        }
        if (!time_line(line[i], &a, &b) || b <= a) {
            if (log) log(first + 1, time_line(line[i], &a, &b) ? "the end time is not after the start"
                                                                : "not a subtitle (a number, start --> end, the text)");
            while (i < nl && !blank(line[i])) i++;
            continue;
        }
        /* the text: up to a blank line, or the next cue's number and times when the blank line is missing */
        for (i++; i < nl && !blank(line[i]); i++) {
            int x, y;
            const char* s = line[i];
            if (time_line(s, &x, &y) || (is_number(s) && i + 1 < nl && time_line(line[i + 1], &x, &y))) break;
            if (!t && !who) who = speaker(&s);
            t = append(t, &len, s);
        }
        if (!len) { free(t); free(who); continue; }   /* nothing to show */
        if (n == cap) {
            SubCue* r = realloc(c, (cap = cap ? cap * 2 : 64) * sizeof(*c));
            if (!r) { free(t); free(who); break; }
            c = r;
        }
        c[n++] = (SubCue){ id, a, b, t, who };
    }
    for (int j = 1; j < n; j++) {   /* by start, in file order otherwise */
        SubCue k = c[j];
        int m = j;
        for (; m > 0 && c[m - 1].start_ms > k.start_ms; m--) c[m] = c[m - 1];
        c[m] = k;
    }
    free(line);
    free(buf);
    *out = c;
    return n;
}

void subs_free(SubCue* c, int n)
{
    for (int i = 0; i < n; i++) { free(c[i].text); free(c[i].who); }
    free(c);
}

/* ---- JSON: one object of string values ---- */

static const char* ws(const char* p) { return p + strspn(p, " \t\r\n"); }

static int hex4(const char* p, unsigned* v)
{
    *v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        *v = *v << 4 | (unsigned)(c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99);
        if (*v & ~0xFFFFu) return 0;
    }
    return 1;
}

static int utf8(char* d, unsigned cp)
{
    if (cp < 0x80) { d[0] = (char)cp; return 1; }
    if (cp < 0x800) { d[0] = (char)(0xC0 | cp >> 6); d[1] = (char)(0x80 | (cp & 63)); return 2; }
    if (cp < 0x10000) { d[0] = (char)(0xE0 | cp >> 12); d[1] = (char)(0x80 | (cp >> 6 & 63)); d[2] = (char)(0x80 | (cp & 63)); return 3; }
    d[0] = (char)(0xF0 | cp >> 18); d[1] = (char)(0x80 | (cp >> 12 & 63)); d[2] = (char)(0x80 | (cp >> 6 & 63)); d[3] = (char)(0x80 | (cp & 63));
    return 4;
}

/* the string at p (past its opening quote), malloc'd UTF-8; *end past its closing quote; NULL if invalid */
static char* json_string(const char* p, const char** end)
{
    size_t cap = 64, n = 0;
    char* s = malloc(cap);
    unsigned cp, lo;
    while (s && *p != '"') {
        if (!*p || (unsigned char)*p < 0x20) goto bad;
        if (n + 8 > cap) {
            char* r = realloc(s, cap *= 2);
            if (!r) goto bad;
            s = r;
        }
        if (*p != '\\') { s[n++] = *p++; continue; }
        switch (*++p) {
        case '"': case '\\': case '/': s[n++] = *p++; break;
        case 'b': s[n++] = '\b'; p++; break;
        case 'f': s[n++] = '\f'; p++; break;
        case 'n': s[n++] = '\n'; p++; break;
        case 'r': s[n++] = '\r'; p++; break;
        case 't': s[n++] = '\t'; p++; break;
        case 'u':
            if (!hex4(p + 1, &cp)) goto bad;
            p += 5;
            if (cp >= 0xD800 && cp < 0xDC00) {   /* a surrogate pair */
                if (p[0] != '\\' || p[1] != 'u' || !hex4(p + 2, &lo) || lo < 0xDC00 || lo > 0xDFFF) goto bad;
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                p += 6;
            } else if (cp >= 0xDC00 && cp < 0xE000) goto bad;
            n += (size_t)utf8(s + n, cp);
            break;
        default: goto bad;
        }
    }
    if (!s) return NULL;
    s[n] = 0;
    *end = p + 1;
    return s;
bad:
    free(s);
    return NULL;
}

/* past a value that is not a string (number, true, an object ...); NULL if it never ends */
static const char* skip_value(const char* p)
{
    int depth = 0;
    for (; *p; p++) {
        if (*p == '"') {
            const char* e;
            char* s = json_string(p + 1, &e);
            if (!s) return NULL;
            free(s);
            p = e - 1;
        } else if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') { if (!depth) return p; depth--; }
        else if (*p == ',' && !depth) return p;
    }
    return NULL;
}

static int pair_cmp(const void* a, const void* b) { return strcmp(((const SubPair*)a)->key, ((const SubPair*)b)->key); }

int subs_json_parse(const char* text, SubPair** out)
{
    const char* p = ws(text);
    SubPair* v = NULL;
    int n = 0, cap = 0;
    *out = NULL;
    if (!strncmp(p, "\xEF\xBB\xBF", 3)) p = ws(p + 3);
    if (*p++ != '{') return -1;
    p = ws(p);
    if (*p == '}') return 0;
    for (;;) {
        char* k, *s = NULL;
        if (*p != '"' || !(k = json_string(p + 1, &p))) goto bad;
        p = ws(p);
        if (*p++ != ':') { free(k); goto bad; }
        p = ws(p);
        if (*p == '"') { if (!(s = json_string(p + 1, &p))) { free(k); goto bad; } }
        else if (!(p = skip_value(p))) { free(k); goto bad; }
        if (!s) free(k);
        else {
            if (n == cap) {
                SubPair* r = realloc(v, (size_t)(cap = cap ? cap * 2 : 256) * sizeof(*v));
                if (!r) { free(k); free(s); goto bad; }
                v = r;
            }
            v[n++] = (SubPair){ k, s };
        }
        p = ws(p);
        if (*p == ',') { p = ws(p + 1); continue; }
        if (*p == '}') break;
        goto bad;
    }
    qsort(v, (size_t)n, sizeof(*v), pair_cmp);
    *out = v;
    return n;
bad:
    subs_pairs_free(v, n);
    return -1;
}

void subs_pairs_free(SubPair* v, int n)
{
    for (int i = 0; i < n; i++) { free(v[i].key); free(v[i].val); }
    free(v);
}

const char* subs_pairs_get(const SubPair* v, int n, const char* key)
{
    SubPair k = { (char*)key, NULL };
    const SubPair* e = n > 0 ? bsearch(&k, v, (size_t)n, sizeof(*v), pair_cmp) : NULL;
    return e ? e->val : NULL;
}

/* ---- files ---- */

static void parse_log(int line, const char* msg) { host_log("subtitles: %s line %d: %s\n", parsing, line, msg); }

/* the lock: created by subs_set_language at start (main thread, before the game thread runs) or
   with the first movie or clip (game thread); the render thread only reads it */
static SDL_Mutex* lock_get(void)
{
    SDL_Mutex* m = __atomic_load_n(&lock, __ATOMIC_ACQUIRE);
    if (!m) { m = SDL_CreateMutex(); __atomic_store_n(&lock, m, __ATOMIC_RELEASE); }
    return m;
}

static const char* mods_dir(void) { return getenv("RECVX_MODS") ? getenv("RECVX_MODS") : "mods"; }

/* <mods>/subtitles/<name> (NULL: none or unreadable, at most 2 MB) */
static char* read_file(const char* name, char* path, size_t npath)
{
    char* text = NULL;
    long size;
    FILE* f;
    snprintf(path, npath, "%s/subtitles/%s", mods_dir(), name);
    if (!(f = pc_fopen(path, "rb"))) return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > (2 << 20) || !(text = calloc(1, (size_t)size + 1)) || fread(text, 1, (size_t)size, f) != (size_t)size) {
        host_log("subtitles: %s: cannot read it (larger than 2 MB?)\n", path);
        free(text);
        text = NULL;
    }
    fclose(f);
    return text;
}

/* "MV_016.PSS;1", "VOICE1.AFS" -> base "MV_016" */
static void base_of(const char* disc_name, char* base, size_t n)
{
    size_t k = 0;
    while (disc_name[k] && disc_name[k] != '.' && disc_name[k] != ';' && k < n - 1) { base[k] = disc_name[k]; k++; }
    base[k] = 0;
}

void subs_set_language(const char* code)
{
    SDL_Mutex* m = lock_get();
    SDL_LockMutex(m);
    snprintf(language, sizeof(language), "%s", code ? code : "");
    SDL_UnlockMutex(m);
    __atomic_add_fetch(&language_gen, 1, __ATOMIC_RELEASE);
}

static void reload_archive(int a);

/* game thread: the language's table, after a change (and the voice archives in it again) */
static void language_sync(void)
{
    int g = __atomic_load_n(&language_gen, __ATOMIC_ACQUIRE);
    char code[16], name[32], path[1100], *text;
    SDL_Mutex* m;
    if (g == loaded) return;
    loaded = g;
    m = lock_get();
    SDL_LockMutex(m);
    snprintf(code, sizeof(code), "%s", language);
    SDL_UnlockMutex(m);
    subs_pairs_free(tr, ntr);
    tr = NULL;
    ntr = 0;
    if (code[0]) {
        snprintf(name, sizeof(name), "%s.json", code);
        if ((text = read_file(name, path, sizeof(path)))) {
            if ((ntr = subs_json_parse(text, &tr)) < 0) { host_log("subtitles: %s: not a JSON object of strings\n", path); ntr = 0; }
            else host_log("subtitles: %s: %d lines\n", path, ntr);
            free(text);
        } else host_log("subtitles: %s: none, subtitles in English\n", path);
    }
    for (int a = 0; a < narchives; a++) reload_archive(a);
}

/* the language's text for each cue of file `base` (key: base/n, or base/clip/n with clips), and the
   speaker as a prefix: \x01<id>\x1f<name shown>\x02; the number translated */
static int localize(SubCue* c, int n, const char* base, int clips)
{
    char key[64];
    int done = 0;
    for (int i = 0, k = 0; i < n; i++) {
        const char* t;
        k = clips && i && c[i - 1].id == c[i].id ? k + 1 : clips ? 1 : i + 1;
        if (clips) snprintf(key, sizeof(key), "%s/%d/%d", base, c[i].id, k);
        else snprintf(key, sizeof(key), "%s/%d", base, k);
        if ((t = subs_pairs_get(tr, ntr, key))) {
            char* d = strdup(t);
            if (d) { free(c[i].text); c[i].text = d; done++; }
        }
        if (c[i].who) {
            const char* shown;
            char* d;
            snprintf(key, sizeof(key), "name/%s", c[i].who);
            shown = subs_pairs_get(tr, ntr, key);
            if (!shown) shown = c[i].who;
            if ((d = malloc(strlen(c[i].who) + strlen(shown) + strlen(c[i].text) + 4))) {
                sprintf(d, "\x01%s\x1f%s\x02%s", c[i].who, shown, c[i].text);
                free(c[i].text);
                c[i].text = d;
            }
        }
    }
    return done;
}

/* cue text added to buf (len bytes so far, '\n' between cues); -1 when it does not fit whole */
static int add_cue(char* buf, int n, int len, const char* text)
{
    size_t k = strlen(text);
    if ((size_t)len + (len ? 1 : 0) + k >= (size_t)n) return -1;
    if (len) buf[len++] = '\n';
    memcpy(buf + len, text, k);
    return len + (int)k;
}

/* ---- movies ---- */

int subs_load_text(const char* name, const char* text)
{
    SDL_Mutex* m = lock_get();
    SubCue* c = NULL, *old;
    int k, oldn, done;
    snprintf(parsing, sizeof(parsing), "%s.srt", name);
    k = text ? subs_parse(text, &c, parse_log) : 0;
    done = localize(c, k, name, 0);
    if (ntr && k) host_log("subtitles: %s: %d of %d subtitles translated\n", name, done, k);
    SDL_LockMutex(m);
    old = cues; oldn = ncues;
    cues = c; ncues = k;
    SDL_UnlockMutex(m);
    subs_free(old, oldn);
    return k;
}

int subs_movie_open(const char* disc_name)
{
    char base[16], file[32], path[1100], *text;
    int n;
    language_sync();
    base_of(disc_name, base, sizeof(base));
    snprintf(file, sizeof(file), "%s.srt", base);
    text = read_file(file, path, sizeof(path));
    n = subs_load_text(base, text);
    if (text) host_log("subtitles: %s: %d subtitles\n", path, n);
    free(text);
    return n;
}

void subs_movie_close(void) { if (__atomic_load_n(&lock, __ATOMIC_ACQUIRE)) subs_load_text("", NULL); }

int subs_text_at(int frame, char* buf, int n)
{
    SDL_Mutex* m = __atomic_load_n(&lock, __ATOMIC_ACQUIRE);
    int64_t ms = (int64_t)frame * 1001 / 30;   /* 29.97 Hz, as movie_glue.c readMpeg counts */
    int len = 0;
    if (!m || n <= 0) return 0;
    SDL_LockMutex(m);
    for (int i = 0; i < ncues && cues[i].start_ms <= ms; i++) {   /* every cue on screen, the earliest first */
        int k;
        if (ms >= cues[i].end_ms) continue;
        if ((k = add_cue(buf, n, len, cues[i].text)) < 0) break;   /* whole cues only */
        len = k;
    }
    SDL_UnlockMutex(m);
    buf[len] = 0;
    return len;
}

/* ---- voice clips ---- */

/* archive a's cues from SRT text (NULL: none), by clip then start, in the current language; they
   replace the previous ones, and no track keeps pointing into those */
static int install_archive(int a, const char* text)
{
    SDL_Mutex* m = lock_get();
    SubCue* c = NULL, *old;
    int k, oldn, done;
    char base[32];
    base_of(archive[a].name, base, sizeof(base));
    snprintf(parsing, sizeof(parsing), "%s.srt", base);
    k = text ? subs_parse(text, &c, parse_log) : 0;
    for (int j = 1; j < k; j++) {   /* by clip, then start: a clip's cues together (subs_parse sorted them by start) */
        SubCue x = c[j];
        int i = j;
        for (; i > 0 && c[i - 1].id > x.id; i--) c[i] = c[i - 1];
        c[i] = x;
    }
    done = localize(c, k, base, 1);
    if (ntr && k) host_log("subtitles: %s: %d of %d subtitles translated\n", base, done, k);
    SDL_LockMutex(m);
    for (int t = 0; t < MAX_TRACKS; t++)
        if (track[t].n && track[t].c >= archive[a].c && track[t].c < archive[a].c + archive[a].n) track[t].n = 0;
    old = archive[a].c; oldn = archive[a].n;
    archive[a].c = c; archive[a].n = k;
    SDL_UnlockMutex(m);
    subs_free(old, oldn);
    return k;
}

/* <mods>/subtitles/<archive base>.srt */
static void reload_archive(int a)
{
    char base[32], file[48], path[1100], *text;
    int k;
    base_of(archive[a].name, base, sizeof(base));
    snprintf(file, sizeof(file), "%s.srt", base);
    text = read_file(file, path, sizeof(path));
    k = install_archive(a, text);
    if (text) host_log("subtitles: %s: %d subtitles\n", path, k);
    free(text);
}

static int archive_of(const char* name)
{
    int a;
    for (a = 0; a < narchives && strcmp(archive[a].name, name); a++) {}
    if (a == narchives) {
        if (a == MAX_ARCHIVES) return -1;
        snprintf(archive[a].name, sizeof(archive[a].name), "%s", name);
        narchives++;
    }
    return a;
}

int subs_voice_load_text(const char* name, const char* text)
{
    int a = archive_of(name);
    return a < 0 ? 0 : install_archive(a, text);
}

void subs_voice_start(int slot, const char* name, int fid, int dur_ms, uint64_t now_ns)
{
    SDL_Mutex* m;
    const SubCue* c = NULL;
    int a, n = 0, known;
    if (slot < 0 || slot >= MAX_TRACKS) return;
    language_sync();
    for (a = 0; a < narchives && strcmp(archive[a].name, name); a++) {}
    known = a < narchives;
    if (!known && (a = archive_of(name)) >= 0) reload_archive(a);   /* its first clip: the file, once */
    if (a >= 0)
        for (int i = 0; i < archive[a].n; i++)
            if (archive[a].c[i].id == fid) { if (!n) c = &archive[a].c[i]; n++; }
    m = lock_get();
    SDL_LockMutex(m);
    track[slot].c = c; track[slot].n = n;
    track[slot].dur_ms = dur_ms; track[slot].start_ns = now_ns;
    SDL_UnlockMutex(m);
}

void subs_voice_stop(int slot)
{
    SDL_Mutex* m = __atomic_load_n(&lock, __ATOMIC_ACQUIRE);
    if (!m || slot < 0 || slot >= MAX_TRACKS) return;
    SDL_LockMutex(m);
    track[slot].n = 0;
    SDL_UnlockMutex(m);
}

int subs_voice_text(uint64_t now_ns, char* buf, int n)
{
    SDL_Mutex* m = __atomic_load_n(&lock, __ATOMIC_ACQUIRE);
    int len = 0;
    if (!m || n <= 0) return 0;
    SDL_LockMutex(m);
    for (int t = 0; t < MAX_TRACKS; t++) {
        int64_t ns = (int64_t)(now_ns - track[t].start_ns), ms = ns / 1000000;
        if (!track[t].n || ns < 0 || (track[t].dur_ms > 0 && ms >= track[t].dur_ms + LINGER_MS)) continue;   /* none, not yet, or over */
        for (int i = 0; i < track[t].n; i++) {
            int k;
            if (ms < track[t].c[i].start_ms || ms >= track[t].c[i].end_ms) continue;
            if ((k = add_cue(buf, n, len, track[t].c[i].text)) >= 0) len = k;
        }
    }
    SDL_UnlockMutex(m);
    buf[len] = 0;
    return len;
}

/* ---- the languages there are ---- */

typedef struct { char (*codes)[16]; char (*names)[64]; int n, max; } LangList;

static SDL_EnumerationResult SDLCALL lang_cb(void* ud, const char* dir, const char* fname)
{
    LangList* l = ud;
    size_t len = strlen(fname);
    char path[1100], *text;
    SubPair* v;
    int nv;
    (void)dir;
    if (l->n >= l->max || len < 6 || len - 5 >= 16 || SDL_strcasecmp(fname + len - 5, ".json")) return SDL_ENUM_CONTINUE;
    for (size_t i = 0; i < len - 5; i++)
        if (!(SDL_isalnum((unsigned char)fname[i]) || fname[i] == '-' || fname[i] == '_')) return SDL_ENUM_CONTINUE;
    snprintf(l->codes[l->n], 16, "%.*s", (int)(len - 5), fname);
    snprintf(l->names[l->n], 64, "%s", l->codes[l->n]);
    if ((text = read_file(fname, path, sizeof(path)))) {
        if ((nv = subs_json_parse(text, &v)) > 0) {
            const char* nm = subs_pairs_get(v, nv, "_name");
            int ascii = nm && *nm;
            for (const char* s = nm; ascii && *s; s++) ascii = *s >= 32 && *s < 127;
            if (ascii) snprintf(l->names[l->n], 64, "%s", nm);   /* (the game's menu font is ASCII) */
            subs_pairs_free(v, nv);
        }
        free(text);
    }
    l->n++;
    return SDL_ENUM_CONTINUE;
}

int subs_languages(char (*codes)[16], char (*names)[64], int max)
{
    char dir[1100];
    LangList l = { codes, names, 0, max };
    snprintf(dir, sizeof(dir), "%s/subtitles", mods_dir());
    SDL_EnumerateDirectory(dir, lang_cb, &l);
    for (int j = 1; j < l.n; j++)   /* by name */
        for (int i = j; i > 0 && SDL_strcasecmp(names[i - 1], names[i]) > 0; i--) {
            char c[16], nm[64];
            memcpy(c, codes[i], 16); memcpy(codes[i], codes[i - 1], 16); memcpy(codes[i - 1], c, 16);
            memcpy(nm, names[i], 64); memcpy(names[i], names[i - 1], 64); memcpy(names[i - 1], nm, 64);
        }
    return l.n;
}
