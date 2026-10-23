/* Resident Evil Outbreak File #2 (SLUS-20984): character models from the user's disc image
   (docs/IMPORT_OUTBREAK.md "Disc format"). Our own reading of the formats; the structure names
   follow the public notes of the Outbreak modding community (Fothsid's libthanatos/OBTool, GPL,
   used as a reference only, no code taken).

   Disc: ISO 9660; NETBIO00.DAT is an AFS archive whose first file, romdata_usa.afs, is an AFS of
   the common data: P<chr>_<costume>.nbd the playable characters, N*.NBD / HN*.NBD NPCs, E*.NBD
   enemies. An NBD: a table of (type, offset, size, count) entries: TEX (SLD-compressed TIM2
   pictures), AMO (model: meshes, materials, texture list) and AHI (node hierarchy), SDW (shadow). */
#include "import.h"
#include "impmodel.h"
#include "../host/fileio.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR 2048

static int64_t rd_at(FILE* f, int64_t off, void* buf, size_t n)
{
    if (fseeko(f, (off_t)off, SEEK_SET) != 0) return 0;
    return fread(buf, 1, n, f) == n;
}
static uint32_t le32(const unsigned char* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* ---------------------------------------------------------------- ISO 9660 (root directory only) */
static int iso_find(FILE* f, const char* name, int64_t* off, uint32_t* size)
{
    unsigned char pvd[SECTOR], *dir;
    if (!rd_at(f, 16 * SECTOR, pvd, SECTOR) || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5)) return 0;
    uint32_t lba = le32(pvd + 156 + 2), len = le32(pvd + 156 + 10);
    if (len > (1 << 20) || !(dir = malloc(len))) return 0;
    int found = 0;
    if (rd_at(f, (int64_t)lba * SECTOR, dir, len))
        for (uint32_t i = 0; i < len && !found;) {
            int l = dir[i];
            if (!l) { i = (i / SECTOR + 1) * SECTOR; continue; }
            if (i + l > len || l < 34) break;
            int nl = dir[i + 32];
            if (nl && 33 + nl <= l) {
                char n[64] = { 0 };
                memcpy(n, dir + i + 33, nl < 63 ? nl : 63);
                char* sc = strchr(n, ';');
                if (sc) *sc = 0;
                if (!SDL_strcasecmp(n, name) && !(dir[i + 25] & 2)) {
                    *off = (int64_t)le32(dir + i + 2) * SECTOR;
                    *size = le32(dir + i + 10);
                    found = 1;
                }
            }
            i += l;
        }
    free(dir);
    return found;
}

/* ---------------------------------------------------------------- AFS */
typedef struct AfsFile { int64_t off; uint32_t size; char name[32]; } AfsFile;

/* the files of the AFS archive at `base` (malloc'd), with their names if it has a name table */
static AfsFile* afs_list(FILE* f, int64_t base, int* count)
{
    unsigned char h[8];
    if (!rd_at(f, base, h, 8) || memcmp(h, "AFS", 4)) return NULL;
    uint32_t n = le32(h + 4);
    if (!n || n > 100000) return NULL;
    unsigned char* t = malloc((size_t)(n + 1) * 8);
    AfsFile* out = calloc(n, sizeof(*out));
    if (!t || !out || !rd_at(f, base + 8, t, (size_t)(n + 1) * 8)) { free(t); free(out); return NULL; }
    uint32_t first = 0xFFFFFFFF;
    for (uint32_t i = 0; i < n; i++) {
        out[i].off = base + le32(t + 8 * i);
        out[i].size = le32(t + 8 * i + 4);
        if (le32(t + 8 * i) && le32(t + 8 * i) < first) first = le32(t + 8 * i);
    }
    uint32_t noff = le32(t + 8 * n), nsz = le32(t + 8 * n + 4);
    unsigned char p[8];
    if ((nsz != n * 48 || !noff) && first != 0xFFFFFFFF && first >= 16 && rd_at(f, base + first - 8, p, 8)) { noff = le32(p); nsz = le32(p + 4); }
    unsigned char* nt = nsz == n * 48 && noff ? malloc(nsz) : NULL;
    if (nt && rd_at(f, base + noff, nt, nsz))
        for (uint32_t i = 0; i < n; i++) memcpy(out[i].name, nt + 48 * i, 31);
    else
        for (uint32_t i = 0; i < n; i++) snprintf(out[i].name, sizeof(out[i].name), "#%u", i);
    free(nt);
    free(t);
    *count = (int)n;
    return out;
}

/* ---------------------------------------------------------------- SLD (the textures' compression)
   16-bit words. A flag word gives the kind of the next 16 items, highest bit first: 0 a literal
   word; 1 a copy word c: count = c >> 11 (0: the count follows), distance back = c & 0x7ff
   (0: that many zero words). Returns the output size in bytes, -1 if the input is broken. */
int ob2_sld(const unsigned char* src, int n, unsigned char* dst, int cap)
{
    const uint16_t* s = (const uint16_t*)src;
    uint16_t* d = (uint16_t*)dst;
    int sn = n / 2, dn = cap / 2, i = 0, o = 0, bits = 0;
    unsigned flag = 0;
    while (i < sn) {
        if (!bits) { flag = s[i++]; bits = 16; if (i >= sn) break; }
        if (flag & 0x8000) {
            unsigned c = s[i++], cnt = c >> 11, dist = c & 0x7ff;
            if (!cnt) { if (i >= sn) return -1; cnt = s[i++]; }
            if (!cnt && !dist) break;   /* the end marker */
            if (dist > (unsigned)o) return -1;
            for (unsigned k = 0; k < cnt; k++, o++) {
                if (o >= dn) return -1;
                d[o] = dist ? d[o - dist] : 0;
            }
        } else {
            if (o >= dn) return -1;
            d[o++] = s[i++];
        }
        flag <<= 1;
        bits--;
    }
    return o * 2;
}

/* ---------------------------------------------------------------- NBD */
typedef struct Rd { const unsigned char* d; uint32_t n; int bad; } Rd;
static uint32_t u32(Rd* r, uint32_t o) { if (o > r->n || r->n - o < 4) { r->bad = 1; return 0; } return le32(r->d + o); }
static float f32(Rd* r, uint32_t o) { uint32_t v = u32(r, o); float f; memcpy(&f, &v, 4); return isfinite(f) ? f : 0; }

/* chunk header: type, count, size (including the 12-byte header) */
#define T_MODEL 1
#define T_MESHLIST 2
#define T_MESH 4
#define T_PRIMS 5
#define T_MATLIST 9
#define T_TEXLIST 0xA
#define T_MATREF 0x50000
#define T_STRIPMAT 0x60000
#define T_VERT 0x70000
#define T_NORM 0x80000
#define T_UV 0xA0000
#define T_WEIGHT 0xC0000
#define T_JOINTS 0x100000
#define T_HIER 0xC0000000u

typedef struct Mesh { uint32_t at, end; } Mesh;
typedef struct Amo {
    int nmesh; Mesh mesh[64];
    int nmat; int mattex[256];   /* per material its first texture-list entry, -1 none */
    int ntexl; int texl[64];     /* texture list: the NBD's texture slot */
} Amo;

static void euler_quat(float x, float y, float z, float* q)   /* rotation x, then y, then z */
{
    float cx = cosf(x / 2), sx = sinf(x / 2), cy = cosf(y / 2), sy = sinf(y / 2), cz = cosf(z / 2), sz = sinf(z / 2);
    q[0] = sx * cy * cz - cx * sy * sz;
    q[1] = cx * sy * cz + sx * cy * sz;
    q[2] = cx * cy * sz - sx * sy * cz;
    q[3] = cx * cy * cz + sx * sy * sz;
}

static int read_ahi(Rd* r, uint32_t o, ImpModel* m, int* meshnode)
{
    uint32_t cnt = u32(r, o + 4), rs = u32(r, o + 12 + 8);
    if (u32(r, o) != T_HIER || r->bad || cnt < 2 || cnt - 1 > IMP_NODES) return 0;
    uint32_t p = o + 12 + rs;
    m->nn = (int)cnt - 1;
    for (int i = 0; i < m->nn; i++, p += u32(r, p + 8)) {
        uint32_t type = u32(r, p), b = p + 12;
        ImpNode* n = &m->node[i];
        int id = (int)u32(r, b), par = (int)u32(r, b + 4);
        if (id != i || par >= m->nn || par < -1) return 0;
        n->parent = par;
        for (int k = 0; k < 3; k++) n->t[k] = f32(r, b + 16 + 32 + 4 * k);
        float e[3];
        for (int k = 0; k < 3; k++) e[k] = f32(r, b + 16 + 16 + 4 * k) * (type == 0x40000003 ? -1.0f : 1.0f);   /* (a joint's is stored negated) */
        euler_quat(e[0], e[1], e[2], n->q);
        meshnode[i] = (int)u32(r, b + 64);
        n->keep = (int)u32(r, b + 68);   /* (the group for now: decided by the caller) */
        if (u32(r, p + 8) < 12 + 72) return 0;
    }
    for (int i = 0; i < m->nn; i++) {   /* no cycles: every chain of parents ends at a root */
        int a = i, k = 0;
        while (a >= 0 && k++ <= m->nn) a = m->node[a].parent;
        if (a >= 0) return 0;
    }
    return !r->bad;
}

static int read_amo(Rd* r, uint32_t o, Amo* a)
{
    uint32_t cnt = u32(r, o + 4), p = o + 12;
    memset(a, 0, sizeof(*a));
    if (u32(r, o) != T_MODEL) return 0;
    for (uint32_t c = 0; c < cnt && !r->bad; c++) {
        uint32_t t = u32(r, p), n = u32(r, p + 4), sz = u32(r, p + 8), q = p + 12;
        if (sz < 12) return 0;
        if (t == T_MESHLIST)
            for (uint32_t k = 0; k < n && a->nmesh < 64; k++) {
                uint32_t ms = u32(r, q + 8);
                if (ms < 12 || u32(r, q) != T_MESH) return 0;
                a->mesh[a->nmesh].at = q; a->mesh[a->nmesh++].end = q + ms;
                q += ms;
            }
        else if (t == T_MATLIST)
            for (uint32_t k = 0; k < n && a->nmat < 256; k++) {
                uint32_t ms = u32(r, q + 8), nt = u32(r, q + 12 + 52);
                a->mattex[a->nmat++] = nt ? (int)u32(r, q + 12 + 56 + 200) : -1;
                if (ms < 12) return 0;
                q += ms;
            }
        else if (t == T_TEXLIST)
            for (uint32_t k = 0; k < n && a->ntexl < 64; k++) {
                uint32_t ms = u32(r, q + 8);
                a->texl[a->ntexl++] = (int)u32(r, q + 12);
                if (ms < 12) return 0;
                q += ms;
            }
        p += sz;
    }
    return !r->bad;
}

/* append mesh `me` of the AMO to the model (weights to its joints; a mesh without weights hangs
   on the node that names it, its points in that node's frame) */
static int read_mesh(Rd* r, const Mesh* me, const Amo* a, int mi, const int* meshnode, ImpModel* m, float (*W)[16])
{
    uint32_t cnt = u32(r, me->at + 4), p = me->at + 12;
    uint32_t vert = 0, nrm = 0, uv = 0, wgt = 0, jnt = 0, prims = 0, matref = 0, stripmat = 0, nv = 0, nmr = 0, nsm = 0, nj = 0;
    for (uint32_t c = 0; c < cnt && p + 12 <= me->end && !r->bad; c++) {
        uint32_t t = u32(r, p), n = u32(r, p + 4), sz = u32(r, p + 8);
        if (sz < 12) break;
        if (t == T_VERT) { vert = p + 12; nv = n; }
        else if (t == T_NORM && n == nv) nrm = p + 12;
        else if (t == T_UV && n == nv) uv = p + 12;
        else if (t == T_WEIGHT && n == nv) wgt = p + 12;
        else if (t == T_JOINTS) { jnt = p + 12; nj = n; }
        else if (t == T_PRIMS) prims = p;
        else if (t == T_MATREF) { matref = p + 12; nmr = n; }
        else if (t == T_STRIPMAT) { stripmat = p + 12; nsm = n; }
        p += sz;
    }
    if (!vert || !nv || !prims || nv > 65535 || r->bad) return 0;
    int base = m->nv, rigid = -1;
    if (!wgt || !jnt)
        for (int i = 0; i < m->nn; i++) if (meshnode[i] == mi) rigid = i;
    void* q;
    if (!(q = realloc(m->pos, sizeof(*m->pos) * (base + nv)))) return 0;
    m->pos = q;
    if (!(q = realloc(m->nrm, sizeof(*m->nrm) * (base + nv)))) return 0;
    m->nrm = q;
    if (!(q = realloc(m->uv, sizeof(*m->uv) * (base + nv)))) return 0;
    m->uv = q;
    if (!(q = realloc(m->jnt, sizeof(*m->jnt) * (base + nv)))) return 0;
    m->jnt = q;
    if (!(q = realloc(m->wgt, sizeof(*m->wgt) * (base + nv)))) return 0;
    m->wgt = q;
    m->nv += nv;
    uint32_t wp = wgt;
    for (uint32_t i = 0; i < nv; i++) {
        float* P = m->pos[base + i], *N = m->nrm[base + i];
        for (int k = 0; k < 3; k++) { P[k] = f32(r, vert + 12 * i + 4 * k); N[k] = nrm ? f32(r, nrm + 12 * i + 4 * k) : 0; }
        m->uv[base + i][0] = uv ? f32(r, uv + 8 * i) : 0;
        m->uv[base + i][1] = uv ? f32(r, uv + 8 * i + 4) : 0;
        memset(m->jnt[base + i], 0, 4);
        memset(m->wgt[base + i], 0, 16);
        if (rigid >= 0) {   /* into model space */
            const float* w = W[rigid];
            float x = P[0], y = P[1], z = P[2], nx = N[0], ny = N[1], nz = N[2];
            for (int k = 0; k < 3; k++) {
                P[k] = w[k] * x + w[4 + k] * y + w[8 + k] * z + w[12 + k];
                N[k] = w[k] * nx + w[4 + k] * ny + w[8 + k] * nz;
            }
            m->jnt[base + i][0] = (unsigned char)rigid;
            m->wgt[base + i][0] = 1;
        } else if (wgt) {   /* (joint index into the mesh's joint list, weight in percent); the 4 largest */
            uint32_t k = u32(r, wp);
            wp += 4;
            for (uint32_t j = 0; j < k && j < 64; j++) {
                uint32_t id = u32(r, wp + 8 * j);
                float w = f32(r, wp + 8 * j + 4) / 100.0f;
                int node = id < nj ? (int)u32(r, jnt + 4 * id) : -1;
                if (node < 0 || node >= m->nn || w <= 0) continue;
                int s = 0;
                for (int x = 1; x < 4; x++) if (m->wgt[base + i][x] < m->wgt[base + i][s]) s = x;
                if (w > m->wgt[base + i][s]) { m->wgt[base + i][s] = w; m->jnt[base + i][s] = (unsigned char)node; }
            }
            wp += 8 * k;
        }
    }
    /* strips: up to two lists (single- and multi-weight); a strip's material by its order over both */
    uint32_t nl = u32(r, prims + 4), lp = prims + 12, si = 0;
    for (uint32_t l = 0; l < nl && l < 2 && !r->bad; l++) {
        uint32_t ns = u32(r, lp + 4), sp = lp + 12, lsz = u32(r, lp + 8);
        if (lsz < 12) return 0;
        for (uint32_t s = 0; s < ns && !r->bad; s++, si++) {
            uint32_t n = u32(r, sp) & 0x7FFFFFFF;
            sp += 4;
            if (n > 65535 || sp + 4 * n > r->n) return 0;
            if (!(q = realloc(m->idx, sizeof(int) * (m->nidx + n)))) return 0;
    m->idx = q;
            if (!(q = realloc(m->strip, sizeof(ImpStrip) * (m->ns + 1)))) return 0;
    m->strip = q;
            ImpStrip* st = &m->strip[m->ns++];
            st->first = m->nidx; st->n = (int)n; st->tex = -1;
            for (uint32_t k = 0; k < n; k++) {
                uint32_t x = u32(r, sp + 4 * k);
                m->idx[m->nidx++] = x < nv ? base + (int)x : base;
            }
            sp += 4 * n;
            int mr = si < nsm ? (int)u32(r, stripmat + 4 * si) : 0, mat = mr >= 0 && (uint32_t)mr < nmr ? (int)u32(r, matref + 4 * mr) : -1;
            int tl = mat >= 0 && mat < a->nmat ? a->mattex[mat] : -1;
            st->tex = tl >= 0 && tl < a->ntexl ? a->texl[tl] : -1;
            if (st->tex >= m->ntex) st->tex = -1;
        }
        lp += lsz;
    }
    return !r->bad;
}

/* model + textures of an NBD file's first AMO/AHI pair */
static int read_nbd(const unsigned char* d, uint32_t n, ImpModel* m, char* err, int en)
{
    Rd r = { d, n, 0 };
    memset(m, 0, sizeof(*m));
    uint32_t hs = u32(&r, 4);   /* the table: 16 bytes per entry, ends where the textures start */
    if (r.bad || memcmp(d, "TEX", 4) || hs < 48 || hs > 256) { snprintf(err, en, "not an NBD model"); return 0; }
    uint32_t amo = 0, ahi = 0;
    for (uint32_t e = 16; e + 16 <= hs; e += 16) {
        if (!memcmp(d + e, "AMO", 4) && !amo) amo = u32(&r, e + 4);
        if (!memcmp(d + e, "AHI", 4) && !ahi) ahi = u32(&r, e + 4);
    }
    if (!amo || !ahi) { snprintf(err, en, "no model/hierarchy in the file"); return 0; }
    /* textures: count entries of u32 size + SLD data */
    uint32_t tp = u32(&r, 4), tc = u32(&r, 12);
    for (uint32_t k = 0; k < tc && k < IMP_TEX && !r.bad; k++) {
        uint32_t sz = u32(&r, tp);
        tp += 4;
        if (sz > n - tp) { r.bad = 1; break; }
        int cap = 8 << 20, got;
        unsigned char* out = malloc(cap);
        if (!out || (got = ob2_sld(d + tp, (int)sz, out, cap)) < 64) { free(out); snprintf(err, en, "texture %u: bad compressed data", k); imp_free(m); return 0; }
        unsigned char* fit = realloc(out, got);
        m->tim2[m->ntex] = fit ? fit : out;
        m->tim2_len[m->ntex++] = got;
        tp += sz;
    }
    Amo a;
    int meshnode[IMP_NODES];
    float (*W)[16];
    if (!read_ahi(&r, ahi, m, meshnode)) { snprintf(err, en, "bad node hierarchy"); imp_free(m); return 0; }
    if (!read_amo(&r, amo, &a)) { snprintf(err, en, "bad model data"); imp_free(m); return 0; }
    if (!(W = calloc(m->nn, sizeof(*W)))) { imp_free(m); return 0; }
    imp_world(m, W);
    int meshes = 0;
    for (int i = 0; i < a.nmesh; i++) meshes += read_mesh(&r, &a.mesh[i], &a, i, meshnode, m, W);
    free(W);
    if (!meshes || !m->nv) { snprintf(err, en, "no readable mesh"); imp_free(m); return 0; }
    m->nmesh = meshes;
    return 1;
}

/* ---------------------------------------------------------------- the extraction */
static const char* const cast[8] = { "Kevin Ryman", "Mark Wilkins", "Jim Chapman", "George Hamilton",
                                     "David King", "Alyssa Ashcroft", "Yoko Suzuki", "Cindy Lennox" };

/* the nodes the player model keeps: the body (groups 0 lower, 1 upper body) where it bends; the
   face (2) and fingers (3, 4) and the zero-length pivots (the hips' second node, the face root)
   move with their parent */
static void keep_body(ImpModel* m)
{
    for (int i = 0; i < m->nn; i++) {
        ImpNode* n = &m->node[i];
        int group = n->keep;
        float l = fabsf(n->t[0]) + fabsf(n->t[1]) + fabsf(n->t[2]);
        n->keep = n->parent < 0 || (group <= 1 && l > 0.01f);
    }
}

int ob2_extract(const char* isopath, const char* outdir, int what, ImportItem* items, int max, int* count)
{
    FILE* f = pc_fopen(isopath, "rb");
    int64_t off, cnf_off;
    uint32_t size, cnf_size;
    int nfiles = 0, nrom = 0, fails = 0;
    char cnf[128] = { 0 };
    ImportProgress* pg = &g_import_progress;
    *count = 0;
    if (!f) { snprintf(pg->phase, sizeof(pg->phase), "cannot open %s", isopath); return -1; }
    if (!iso_find(f, "SYSTEM.CNF", &cnf_off, &cnf_size) || !rd_at(f, cnf_off, cnf, cnf_size < 127 ? cnf_size : 127) || !strstr(cnf, "SLUS_209.84")) {
        snprintf(pg->phase, sizeof(pg->phase), "not Resident Evil Outbreak File #2 (US, SLUS-20984)");
        fclose(f);
        return -1;
    }
    AfsFile *top = iso_find(f, "NETBIO00.DAT", &off, &size) ? afs_list(f, off, &nfiles) : NULL, *rom = NULL;
    for (int i = 0; top && i < nfiles && !rom; i++)
        if (!SDL_strcasecmp(top[i].name, "romdata_usa.afs")) rom = afs_list(f, top[i].off, &nrom);
    free(top);
    if (!rom) { snprintf(pg->phase, sizeof(pg->phase), "NETBIO00.DAT / romdata_usa.afs not found"); fclose(f); return -1; }

    /* what to extract: the players (P<chr>_<costume>.nbd), NPCs (N*.NBD), enemies (E*.NBD) */
    int* list = malloc(sizeof(int) * nrom), nl = 0;
    for (int i = 0; list && i < nrom; i++) {
        const char* n = rom[i].name;
        const char* dot = strrchr(n, '.');
        if (!dot || SDL_strcasecmp(dot, ".nbd")) continue;
        int pl = (n[0] == 'P' && n[1] >= '0' && n[1] <= '9' && n[3] == '_');
        int npc = n[0] == 'N' && n[1] >= '0' && n[1] <= '9';
        int en = n[0] == 'E' && n[1] >= '0' && n[1] <= '9';
        if ((pl && (what & IMPORT_PLAYERS)) || (npc && (what & IMPORT_NPCS)) || (en && (what & IMPORT_ENEMIES))) list[nl++] = i;
    }
    pg->total = nl;
    for (int li = 0; li < nl && *count < max; li++) {
        const AfsFile* e = &rom[list[li]];
        ImportItem* it = &items[(*count)++];
        ImpModel m;
        char id[16], dir[1200], path[1300], err[160] = "";
        memset(it, 0, sizeof(*it));
        int idl = (int)(strrchr(e->name, '.') - e->name);
        snprintf(id, sizeof(id), "%.*s", idl < 15 ? idl : 15, e->name);
        for (char* c = id; *c; c++) *c = (char)SDL_toupper((unsigned char)*c);
        snprintf(it->id, sizeof(it->id), "%s", id);
        if (id[0] == 'P') {
            int c = atoi(id + 1), v = atoi(id + 4);   /* P<chr 2 digits>_<costume> */
            snprintf(it->name, sizeof(it->name), "%s", c >= 0 && c < 8 ? cast[c] : "?");
            snprintf(it->variant, sizeof(it->variant), "Costume %d", v + 1);
            it->female = c >= 5;
        } else {
            snprintf(it->name, sizeof(it->name), "%s %s", id[0] == 'N' ? "NPC" : "Enemy", id);
            it->female = -1;
        }
        snprintf(pg->phase, sizeof(pg->phase), "%s %s (%s)", it->name, it->variant, e->name);
        unsigned char* data = e->size > 0 && e->size < (64u << 20) ? malloc(e->size) : NULL;
        if (!data || !rd_at(f, e->off, data, e->size)) snprintf(err, sizeof(err), "cannot read %s from the disc", e->name);
        else if (read_nbd(data, e->size, &m, err, sizeof(err))) {
            it->nodes = m.nn; it->verts = m.nv; it->tris = imp_tris(&m); it->textures = m.ntex;
            it->meshes = m.nmesh;
            snprintf(dir, sizeof(dir), "%s/%s", outdir, id);
            SDL_CreateDirectory(dir);
            snprintf(path, sizeof(path), "%s/%s.glb", dir, id);
            it->ok = imp_write_glb(&m, path, id, 0.01f, err, sizeof(err));
            for (int k = 0; k < m.ntex; k++) {
                snprintf(path, sizeof(path), "%s/%s_tex%d.png", dir, id, k);
                if (!imp_write_png(&m, k, path) && !err[0]) snprintf(err, sizeof(err), "texture %d: unsupported picture", k);
            }
            if (it->ok && it->female >= 0) {   /* the playable characters: "You appear as" */
                keep_body(&m);
                snprintf(path, sizeof(path), "%s/%s.cvxm", dir, id);
                char e2[160] = "";
                if (imp_write_cvxm(&m, path, 0.1f, 0x00F00000u + 16u * (unsigned)list[li], e2, sizeof(e2))) it->player = 1;
                else snprintf(err, sizeof(err), "no player model: %s", e2);
            }
            imp_free(&m);
        }
        free(data);
        snprintf(it->note, sizeof(it->note), "%s", err);
        fails += !it->ok;
        pg->failed = fails;
        pg->done = li + 1;
    }
    free(list);
    free(rom);
    fclose(f);
    return fails;
}
