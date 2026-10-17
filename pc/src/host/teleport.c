/* Teleport destinations for the F2 window and ENHANCED SETTINGS > EXTRAS (cheat.h tp_*): every
   room file on the disc (the decomp's rdx_files[]) and in the mods folder, one row per stage and
   room (its story cases together), with its entry positions and, per entry, the rooms whose doors
   lead there: door triggers (type 0) and the scripts' door calls (op 51 SetDoorCall) and door
   triggers (op 37 EtcAtariSet2 with type 0), of any case (docs/modding/ROOM_FORMAT.md).
   All files are read and expanded once, on a background thread with its own handle on the disc
   image, at the first tp_count() (about 600 MB packed: a few seconds); the table is published
   whole and read-only after that, so any thread may read it.
   Names: the save menu's room name from the game's messages (g_cheat.rooms, by RoomNameSwitch),
   else a label of ours, else "Room n (RM_SRRC)"; a mods-only room is "Custom room RM_SRRC" or
   the name tag in its header (usrname, when it is not the tools' or retail's own tag). */
#include "../ps2_platform.h"
#include "../audio/audio.h"
#include "cheat.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char* rdx_files[205];                    /* ps2_dvd_image.c, sorted names; AFS partition 17 entry i */
unsigned int RoomNameSwitch(unsigned int name); /* bup_00.c: stage * 100 + room -> save menu room name 1..18, 0 none */

enum { TP_SRC = 4, MAX_FILES = 1024 };
typedef struct Row {
    TpRoom r;
    unsigned char nsrc[TP_POS];
    short src[TP_POS][TP_SRC];   /* row indices */
} Row;

static Row* rows;
static int nrows, state;         /* 0 not started, 1 reading, 2 ready, 3 failed */
static short at[1000];           /* stage * 100 + room -> row, -1 */

/* ours, where the room is known for certain (the port's docs and fixes name them) */
static const struct { int stg, rom; const char* name; } labels[] = {
    { 0, 0, "Opening" }, { 0, 2, "Graveyard" }, { 0, 9, "Metal detector" }, { 1, 2, "Hall" }, { 1, 11, "Piano room" },
    { 5, 50, "Battle Game start" }, { 6, 8, "Weapons storage" }, { 7, 25, "Ladder room (Albinoid pool)" },
    { 9, 8, "Weapons storage" }, { 9, 17, "Tiger statue elevator" },
};
/* ours, after the save menu's area names; stages 7..9 are Chris's half of the game */
static const char* const stages[10] = {
    "Claire: Rockfort Prison", "Claire: Rockfort Palace", "Claire: Rockfort Residence", "Claire: Rockfort Training Facility",
    "Claire: Rockfort Airport", "Transport plane / Battle Game", "Claire: Antarctic Base", "Chris: Rockfort Island",
    "Chris: Rockfort Airport", "Chris: Antarctic Base",
};

/* ---------------------------------------------------------------- reading */
static unsigned rd32(const unsigned char* d, long n, long o) { return o >= 0 && o + 4 <= n ? d[o] | d[o + 1] << 8 | d[o + 2] << 16 | (unsigned)d[o + 3] << 24 : 0; }

/* expand.c Expand (docs/modding/ROOM_FORMAT.md "Compression") into *out, grown as needed: the size, -1 if broken */
static long expand(const unsigned char* s, long n, unsigned char** out, long* cap)
{
    const unsigned char* e = s + n;
    long o = 0;
    unsigned field = 0, b;
    int left = 1;
#define BIT() (--left ? 0 : (field = s < e ? *s++ : 0, left = 8), b = field & 1, field >>= 1, b)
    if (s < e) { field = *s++; left = 9; }
    while (s < e) {
        long len, off;
        if (o + 260 > *cap) {
            unsigned char* p = realloc(*out, *cap * 2);
            if (!p) return -1;
            *out = p; *cap *= 2;
        }
        if (BIT()) { (*out)[o++] = *s++; continue; }
        if (!BIT()) {
            len = 2 * BIT(); len += BIT() + 2;
            if (s >= e) return -1;
            off = *s++ - 256;
        } else {
            unsigned w;
            if (s + 2 > e) return -1;
            w = s[0] | s[1] << 8; s += 2;
            if (!w) return o;
            off = (long)(w >> 3) - 8192;
            len = w & 7;
            if (len) len += 2;
            else { if (s >= e) return -1; len = *s++ + 1; }
        }
        if (-off > o) return -1;
        for (long k = 0; k < len; k++, o++) (*out)[o] = (*out)[o + off];
    }
#undef BIT
    return -1;
}

static void add_src(int stg, int rom, int pos, int from)
{
    Row* t;
    int k;
    if (stg > 9 || rom > 99 || at[stg * 100 + rom] < 0 || pos >= TP_POS) return;
    t = &rows[at[stg * 100 + rom]];
    for (k = 0; k < t->nsrc[pos]; k++) if (t->src[pos][k] == from) return;
    if (k < TP_SRC) { t->src[pos][k] = (short)from; t->nsrc[pos]++; }   /* ponytail: a fifth source is left out; none has more than 4 */
}

/* script instruction length (event.c; pc/tools/rdx.py OP_LEN, CTRL_LEN: derived from the handlers) */
static const unsigned char op_len[256] = {
    2, 2, 2, 2, 6, 6, 4, 6, 4, 4, 4, 4, 4, 4, 6, 2, 2, 2, 2, 4, 4, 4, 2, 8, 2, 8, 2, 6, 6, 4, 6, 4,
    4, 6, 4, 6, 4, 10, 2, 2, 2, 2, 4, 2, 4, 2, 2, 4, 4, 2, 12, 8, 12, 4, 6, 2, 6, 4, 2, 6, 4, 4, 4, 4,
    8, 2, 4, 4, 6, 2, 8, 10, 6, 10, 8, 4, 6, 4, 14, 6, 2, 12, 12, 12, 12, 2, 6, 2, 14, 12, 4, 2, 2, 6, 4, 2,
    6, 2, 6, 2, 1, 4, 1, 1, 6, 1, 2, 2, 8, 22, 22, 12, 2, 2, 14, 22, 4, 4, 4, 6, 14, 6, 2, 10, 10, 4, 4, 6,
    4, 2, 4, 2, 8, 4, 16, 2, 10, 2, 2, 12, 6, 2, 4, 4, 2, 4, 2, 2, 4, 4, 2, 8, 12, 4, 4, 6, 2, 4, 2, 2,
    6, 2, 4, 12, 4, 4, 4, 4, 10, 2, 10, 6, 6, 4, 2, 4, 22, 6, 4, 8, 4, 4, 2, 2, 2, 10, 16, 2, 2, 2, 6, 4,
    28, 2, 6, 4, 8, 2, 2, 4, 8, 4, 2, 2, 4, 2, 2, 2, 4, 10, 10, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 4, 3, 4, 2, 2, 1, 1, 2,
};
/* controller ops 100/102/103/105: sub-command, its length incl. the sub-command byte (an unknown one adds nothing) */
static const unsigned char ctrl100[][2] = { { 150, 3 }, { 129, 11 }, { 130, 1 }, { 141, 5 }, { 142, 1 }, { 131, 3 }, { 140, 7 }, { 12, 5 },
    { 13, 5 }, { 132, 5 }, { 143, 3 }, { 145, 5 }, { 137, 3 }, { 128, 1 }, { 139, 1 }, { 7, 7 }, { 52, 1 }, { 0, 1 } };
static const unsigned char ctrl102[][2] = { { 20, 3 }, { 1, 1 } };
static const unsigned char ctrl103[][2] = { { 128, 1 }, { 139, 1 }, { 144, 1 }, { 143, 5 }, { 146, 7 }, { 147, 7 }, { 148, 3 }, { 149, 3 }, { 0, 1 } };
static const unsigned char ctrl105[][2] = { { 53, 3 }, { 51, 5 }, { 52, 3 }, { 50, 3 }, { 47, 1 }, { 43, 4 }, { 44, 4 }, { 45, 4 }, { 42, 3 },
    { 41, 3 }, { 46, 3 }, { 40, 3 }, { 28, 2 }, { 30, 2 }, { 31, 2 }, { 29, 2 }, { 32, 2 }, { 33, 2 }, { 26, 9 }, { 27, 7 }, { 48, 3 },
    { 49, 3 }, { 25, 7 }, { 35, 7 }, { 24, 7 }, { 34, 9 }, { 23, 3 }, { 5, 5 }, { 6, 5 }, { 12, 5 }, { 13, 5 }, { 7, 7 }, { 11, 5 },
    { 8, 7 }, { 9, 5 }, { 2, 2 }, { 3, 2 }, { 4, 2 }, { 17, 2 }, { 18, 2 }, { 19, 2 }, { 22, 5 }, { 10, 5 }, { 132, 5 }, { 140, 7 },
    { 141, 5 }, { 14, 3 }, { 21, 3 }, { 15, 5 }, { 16, 5 }, { 36, 1 }, { 38, 9 }, { 0, 1 } };

static int insn_len(const unsigned char* d, long n, long o)
{
    const unsigned char (*t)[2] = d[o] == 100 ? ctrl100 : d[o] == 102 ? ctrl102 : d[o] == 103 ? ctrl103 : d[o] == 105 ? ctrl105 : NULL;
    int len = op_len[d[o]],
        nt = d[o] == 100 ? SDL_arraysize(ctrl100) : d[o] == 102 ? SDL_arraysize(ctrl102) : d[o] == 103 ? SDL_arraysize(ctrl103) : SDL_arraysize(ctrl105);
    if (t && o + 1 < n)
        for (int k = 0; k < nt; k++) if (t[k][0] == d[o + 1]) { len += t[k][1]; break; }
    return o + len <= n ? len : 0;
}

/* one script, swept as pc/tools/rdx.py disasm_block: to its End (an event's also at op 255) outside any if-block */
static void sweep(const unsigned char* d, long n, long o, long hint, int event, int from)
{
    long ends[64];
    int ne = 0;
    while (o < n) {
        int op = d[o], len;
        while (ne && ends[ne - 1] <= o) ne--;
        if (!(len = insn_len(d, n, o))) return;
        if (op == 51 || (op == 37 && d[o + 8] == 0)) add_src(d[o + 4], d[o + 5], d[o + 6], from);   /* stage, room, entry */
        if (op == 1 && ne < 64) ends[ne++] = o + d[o + 1] + 1;    /* IfelCk: to its Endif / ElseCk */
        else if (op == 2) { if (ne) ne--; if (ne < 64) ends[ne++] = o + d[o + 1]; }   /* ElseCk */
        else if (op == 3 && ne) ne--;                              /* Endif */
        o += len;
        if (!ne && (op == 0 || (event && op == 255))) return;
        if (hint && o > hint) return;
    }
}

/* the expanded room file of row `from`: entries, door triggers, scripts, name tag */
static void parse(const unsigned char* d, long n, int from)
{
    Row* r = &rows[from];
    long rom = rd32(d, n, 0x10), etc = rd32(d, n, rom + 7 * 4), base = rd32(d, n, 0x1C);
    unsigned npos = rd32(d, n, rom + 0x80 + 9 * 4), netc = rd32(d, n, rom + 0x80 + 7 * 4), first, offs[256];
    int k = 0;
    if (npos > r->r.npos) r->r.npos = npos > TP_POS ? TP_POS : npos;
    for (unsigned i = 0; etc && i < netc && i < 4096; i++) {   /* ATR_WORK 0x24: type at 1, prm0..3 at 0x20 */
        long t = etc + i * 0x24;
        if (t + 0x24 <= n && d[t + 1] == 0) add_src(d[t + 0x20], d[t + 0x21], d[t + 0x22], from);
    }
    first = rd32(d, n, base);   /* hed03: offsets of scd0, scd1, events, up to the first script */
    for (unsigned i = 0; base && i * 4 < first && k < 256; i++) {
        unsigned v = rd32(d, n, base + i * 4);
        offs[k++] = v;
        if (v && v < first) first = v;
    }
    for (int i = 0; i < k; i++) {
        unsigned next = 0;
        for (int j = 0; j < k; j++) if (offs[j] > offs[i] && (!next || offs[j] < next)) next = offs[j];
        sweep(d, n, base + offs[i], next ? base + next : 0, i >= 2, from);
    }
    if (r->r.custom && n >= 0x80 && d[0x60] && memcmp(d + 0x60, "rdxbuild", 9) && memcmp(d + 0x60, "N-MIKAMI", 9)) {
        int j;
        for (j = 0; j < (int)sizeof(r->r.tag) - 1 && d[0x60 + j] >= 0x20 && d[0x60 + j] < 0x7F; j++) r->r.tag[j] = (char)d[0x60 + j];
        r->r.tag[j] = 0;
    }
}

typedef struct File { char name[12]; int disc, mod; } File;

static int file_key(const char* nm) { return (nm[3] - '0') * 100 + (nm[4] - '0') * 10 + (nm[5] - '0'); }
static int is_room_name(const char* nm)
{
    return strlen(nm) == 11 && !SDL_strncasecmp(nm, "RM_", 3) && !SDL_strcasecmp(nm + 7, ".RDX") && SDL_isdigit(nm[3]) && SDL_isdigit(nm[4]) &&
           SDL_isdigit(nm[5]) && SDL_isdigit(nm[6]);
}

static int SDLCALL scan(void* ud)
{
    const char* mods = SDL_getenv("RECVX_MODS") ? SDL_getenv("RECVX_MODS") : "mods";
    Uint64 t0 = SDL_GetTicks();
    File* f = calloc(MAX_FILES, sizeof(File));
    FILE* iso = pc_fopen(g_config.iso_path, "rb");
    char** list;
    int nf = 0, nmod = 0, ent = 0, named = 0, bad = 0, i, key;
    long cap = 16 << 20, pcap = 4 << 20;
    unsigned char *out = malloc(cap), *in = malloc(pcap);
    (void)ud;
    if (!f || !out || !in || !iso || !(rows = calloc(1000, sizeof(Row)))) {
        host_log("teleport: cannot read the room list (%s)\n", iso ? "out of memory" : "disc image");
        __atomic_store_n(&state, 3, __ATOMIC_RELEASE);
        if (iso) fclose(iso);
        free(f); free(out); free(in);
        return 0;
    }
    for (i = 0; i < 205; i++) { SDL_strlcpy(f[nf].name, rdx_files[i], sizeof(f[nf].name)); f[nf].disc = i; f[nf++].mod = 0; }
    if ((list = SDL_GlobDirectory(mods, "RM_*.RDX", SDL_GLOB_CASEINSENSITIVE, &key))) {   /* key: the count here */
        for (int m = 0; m < key && nf < MAX_FILES; m++) {
            char nm[12];
            int j;
            if (!is_room_name(list[m])) continue;
            for (j = 0; j < 11; j++) nm[j] = (char)SDL_toupper(list[m][j]);
            nm[11] = 0;
            for (j = 0; j < nf && strcmp(f[j].name, nm); j++) {}
            if (j == nf) { SDL_strlcpy(f[nf].name, nm, sizeof(f[nf].name)); f[nf++].disc = -1; }
            f[j].mod = 1;
            nmod++;
        }
        SDL_free(list);
    }
    /* rows in stage/room order; a row is custom when no case of it is on the disc */
    for (i = 0; i < 1000; i++) at[i] = -1;
    for (key = 0; key < 1000; key++)
        for (i = 0; i < nf; i++) {
            if (file_key(f[i].name) != key) continue;
            if (at[key] < 0) {
                Row* r = &rows[at[key] = (short)nrows++];
                r->r.stg = (unsigned char)(key / 100); r->r.rom = (unsigned char)(key % 100); r->r.custom = 1;
                SDL_strlcpy(r->r.file, f[i].name, 8);
            }
            if (f[i].disc >= 0) rows[at[key]].r.custom = 0;
        }
    for (i = 0; i < nf; i++) {
        long n = -1, m;
        uint32_t lsn, size;
        FILE* h = NULL;
        if (f[i].mod) {
            char path[1100];
            snprintf(path, sizeof(path), "%s/%s", mods, f[i].name);
            if ((h = pc_fopen(path, "rb"))) { fseek(h, 0, SEEK_END); size = (uint32_t)ftell(h); fseek(h, 0, SEEK_SET); }
        } else if (adx_afs_locate(17, f[i].disc, &lsn, &size) && !fseeko(iso, (off_t)lsn * 2048, SEEK_SET)) h = iso;
        if (h && size > (uint32_t)pcap) { unsigned char* p = realloc(in, size); if (p) { in = p; pcap = size; } }
        if (h && size <= (uint32_t)pcap && fread(in, 1, size, h) == size) n = size;
        if (h && h != iso) fclose(h);
        if (n < 0 || (m = expand(in, n, &out, &cap)) < 0x80) { bad++; host_log("teleport: cannot read %s%s\n", f[i].mod ? "mods/" : "", f[i].name); continue; }
        parse(out, m, at[file_key(f[i].name)]);
    }
    for (i = 0; i < nrows; i++) {
        const Row* r = &rows[i];
        int k = RoomNameSwitch(r->r.stg * 100 + r->r.rom);
        named += k != 0;
        for (int p = 0; p < r->r.npos; p++) ent += r->nsrc[p] != 0;
    }
    {
        int npos = 0, own = 0, custom = 0;
        for (i = 0; i < nrows; i++) { npos += rows[i].r.npos; custom += rows[i].r.custom; }
        for (i = 0; i < (int)SDL_arraysize(labels); i++) own += at[labels[i].stg * 100 + labels[i].rom] >= 0;
        host_log("teleport: %d rooms from %d disc and %d mods room files (%d unreadable); names: %d from the save menu, %d ours, %d custom, "
                 "%d numbered; %d entries, %d labelled with the door they come from (%u ms)\n", nrows, 205, nmod, bad, named, own, custom,
                 nrows - named - own - custom, npos, ent, (unsigned)(SDL_GetTicks() - t0));
    }
    fclose(iso);
    free(f); free(out); free(in);
    __atomic_store_n(&state, 2, __ATOMIC_RELEASE);
    return 0;
}

/* ---------------------------------------------------------------- the table */
int tp_count(void)
{
    int s = __atomic_load_n(&state, __ATOMIC_ACQUIRE), zero = 0;
    if (s == 2) return nrows;
    if (!s && __atomic_compare_exchange_n(&state, &zero, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        SDL_Thread* t = SDL_CreateThread(scan, "rooms", NULL);
        if (t) SDL_DetachThread(t);
        else __atomic_store_n(&state, 3, __ATOMIC_RELEASE);
    }
    return 0;
}

int tp_reading(void) { return __atomic_load_n(&state, __ATOMIC_ACQUIRE) == 1; }
const TpRoom* tp_room(int i) { return i >= 0 && i < tp_count() ? &rows[i].r : NULL; }
int tp_find(int stg, int rom) { return tp_count() && stg >= 0 && stg < 10 && rom >= 0 && rom < 100 ? at[stg * 100 + rom] : -1; }

int tp_stage_first(int stg, int* n)
{
    int i = 0, k = 0, c = tp_count();
    while (i < c && rows[i].r.stg < stg) i++;
    while (i + k < c && rows[i + k].r.stg == stg) k++;
    *n = k;
    return i;
}

const char* tp_stage_name(int stg) { return stg >= 0 && stg < 10 ? stages[stg] : "?"; }

const char* tp_room_name(int i, int with_file, char* b, int n)
{
    const TpRoom* r = tp_room(i);
    const char* nm = NULL;
    unsigned k;
    if (!r) { snprintf(b, n, "-"); return b; }
    k = RoomNameSwitch(r->stg * 100 + r->rom);
    if (k && k < SDL_arraysize(g_cheat.rooms) && g_cheat.rooms[k][0]) nm = g_cheat.rooms[k];
    for (int j = 0; !nm && j < (int)SDL_arraysize(labels); j++) if (labels[j].stg == r->stg && labels[j].rom == r->rom) nm = labels[j].name;
    if (r->custom && !r->tag[0]) snprintf(b, n, "Custom room %s", r->file);
    else if (r->custom || nm) snprintf(b, n, with_file ? "%s (%s)" : "%s", r->custom ? r->tag : nm, r->file);
    else snprintf(b, n, "Room %d (%s)", r->rom, r->file);
    return b;
}

int tp_entries(int i) { const TpRoom* r = tp_room(i); return r ? (r->npos ? r->npos : 1) : 0; }

const char* tp_entry_name(int i, int pos, char* b, int n)
{
    const Row* r = tp_room(i) ? &rows[i] : NULL;
    char from[64];
    int k = r && pos >= 0 && pos < TP_POS ? r->nsrc[pos] : 0;
    if (r && !r->r.npos) snprintf(b, n, "Entry %d (none in the room file)", pos);
    else if (!k) snprintf(b, n, "Entry %d", pos);
    else {
        if (r->src[pos][0] == i) snprintf(from, sizeof(from), "this room");
        else tp_room_name(r->src[pos][0], 0, from, sizeof(from));
        if (k > 1) snprintf(b, n, "Entry %d: from %s +%d", pos, from, k - 1);
        else snprintf(b, n, "Entry %d: from %s", pos, from);
    }
    return b;
}

const char* tp_entry_sources(int i, int pos, char* b, int n)
{
    const Row* r = tp_room(i) ? &rows[i] : NULL;
    char nm[64];
    b[0] = 0;
    for (int k = 0; r && pos >= 0 && pos < TP_POS && k < r->nsrc[pos]; k++) {
        tp_room_name(r->src[pos][k], 1, nm, sizeof(nm));
        snprintf(b + strlen(b), n - strlen(b), "%s%s", k ? "\n" : "", r->src[pos][k] == i ? "this room" : nm);
    }
    return b;
}
