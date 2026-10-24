/*
 * The game's lettering for the subtitles (game_font.h). Nothing of it ships: at start the port reads
 * the disc's own font page, the one bhDispFontEx (message.c) draws every message with, and turns it
 * into a TrueType font in memory, as it reads all the game's data from the player's disc.
 *
 * The page: the first 512 x 512 4-bit TIM2 of SYSTEM.AFS entry 1, 28 x 28 cells in ASCII order from
 * ' ', 16 grey levels (palette red): background 0, a dark outline (64..104), the fill (128..200).
 * Each letter's fill is traced: the cell's levels are upsampled 8x (bicubic, as the subtitles are
 * drawn far larger than 28 pixels), contoured at level 115 (marching squares, the crossings
 * interpolated, so the outlines are smooth and sub-pixel), small specks and holes dropped and the
 * outlines simplified. Letters the page lacks are composed from its own: accented Latin letters
 * (Latin-1 and Latin Extended-A) from the base letter and a drawn mark, ł Ł, œ Œ, ¡ ¿, “ ‘ „, the
 * dashes, the ellipsis and « ». The game spaces letters by fixed cell widths (FontSz); a font is
 * spaced by the ink instead, a little on each side.
 *
 * The font: 1000 units = the 28-pixel cell, the baseline under the capitals; glyf outlines made of
 * straight segments (on-curve points), cmap format 4. Read by ImGui (stb_truetype) like a file.
 */
#include "game_font.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void host_log(const char* fmt, ...);
int  cdvd_read_sectors(uint32_t lsn, uint32_t count, void* dst);
int  cdvd_find_file(const char* path, uint32_t* lsn, uint32_t* size);

#define CELL 28          /* a cell, page pixels */
#define UP 8             /* upsampling */
#define PAD 3            /* empty pixels around a cell while tracing */
#define LEVEL 115.0f     /* the fill: 128 and up; the outline 104 and down */
#define SIDE 0.9f        /* ink spacing, cell pixels on each side */
#define SPACE 6.5f       /* ' ' advance */
#define SMOOTH 6         /* smoothing passes over a traced outline */

typedef struct { float x, y; } Pt;                 /* cell pixels, y down */
typedef struct { Pt* p; int n; } Contour;
typedef struct { uint32_t cp; Contour* c; int nc; float adv; } Glyph;

static Glyph* glyphs;
static int nglyphs, capglyphs;

static Glyph* glyph_get(uint32_t cp)
{
    for (int i = 0; i < nglyphs; i++) if (glyphs[i].cp == cp) return &glyphs[i];
    return NULL;
}
static Glyph* glyph_drawn(uint32_t cp)   /* a glyph with ink */
{
    Glyph* g = glyph_get(cp);
    return g && g->nc ? g : NULL;
}
static Glyph* glyph_new(uint32_t cp)
{
    Glyph* g = glyph_get(cp);
    if (g) {
        for (int i = 0; i < g->nc; i++) free(g->c[i].p);
        free(g->c); g->c = NULL; g->nc = 0;
        return g;
    }
    if (nglyphs == capglyphs) { capglyphs = capglyphs ? capglyphs * 2 : 256; glyphs = realloc(glyphs, capglyphs * sizeof(*glyphs)); }
    g = &glyphs[nglyphs++];
    memset(g, 0, sizeof(*g)); g->cp = cp;
    return g;
}
static void glyphs_free(void)
{
    for (int i = 0; i < nglyphs; i++) { for (int k = 0; k < glyphs[i].nc; k++) free(glyphs[i].c[k].p); free(glyphs[i].c); }
    free(glyphs); glyphs = NULL; nglyphs = capglyphs = 0;
}

static float area(const Pt* p, int n)
{
    float a = 0;
    for (int i = 0; i < n; i++) a += p[i].x * p[(i + 1) % n].y - p[(i + 1) % n].x * p[i].y;
    return a / 2;
}
/* a contour (copied); with sign != 0 turned to that winding (outer contours one way, holes the other) */
static void add_contour(Glyph* g, const Pt* p, int n, float sign)
{
    Contour* c;
    if (n < 3) return;
    g->c = realloc(g->c, (g->nc + 1) * sizeof(*g->c));
    c = &g->c[g->nc++];
    c->p = malloc(n * sizeof(Pt)); c->n = n;
    if (sign != 0 && (area(p, n) > 0) != (sign > 0)) for (int i = 0; i < n; i++) c->p[i] = p[n - 1 - i];
    else memcpy(c->p, p, n * sizeof(Pt));
}
/* src's contours into dst, x' = sx x + tx, y' = sy y + ty */
static void add_xform(Glyph* dst, const Glyph* src, float sx, float tx, float sy, float ty)
{
    for (int i = 0; i < src->nc; i++) {
        int n = src->c[i].n;
        Pt* q = malloc(n * sizeof(Pt));
        for (int k = 0; k < n; k++) { q[k].x = sx * src->c[i].p[k].x + tx; q[k].y = sy * src->c[i].p[k].y + ty; }
        if (sx * sy < 0) for (int k = 0; k < n / 2; k++) { Pt t = q[k]; q[k] = q[n - 1 - k]; q[n - 1 - k] = t; }   /* (a mirror keeps the winding) */
        add_contour(dst, q, n, 0);
        free(q);
    }
}
static void bbox(const Glyph* g, float* x0, float* y0, float* x1, float* y1)
{
    *x0 = *y0 = 1e9f; *x1 = *y1 = -1e9f;
    for (int i = 0; i < g->nc; i++)
        for (int k = 0; k < g->c[i].n; k++) {
            Pt p = g->c[i].p[k];
            if (p.x < *x0) *x0 = p.x;
            if (p.y < *y0) *y0 = p.y;
            if (p.x > *x1) *x1 = p.x;
            if (p.y > *y1) *y1 = p.y;
        }
    if (!g->nc) *x0 = *y0 = *x1 = *y1 = 0;
}

/* ---------------- tracing ---------------- */

#define FN ((CELL + 2 * PAD) * UP)   /* the upsampled field's side */
static float field[FN * FN];
static int edge_next[2 * FN * FN];
static Pt edge_pt[2 * FN * FN];
static unsigned char edge_seen[2 * FN * FN];

static float cubic(float x)   /* bicubic kernel, a = -0.5 (as image resamplers use) */
{
    x = fabsf(x);
    if (x < 1) return (1.5f * x - 2.5f) * x * x + 1;
    if (x < 2) return ((-0.5f * x + 2.5f) * x - 4) * x + 2;
    return 0;
}
/* the cell's levels (with PAD empty pixels around) upsampled UP times into field: sample i at
   (i + 0.5) / UP padded pixels */
static void upsample(const unsigned char* page, int col, int row)
{
    enum { N = CELL + 2 * PAD };
    static float src[N * N], tmp[N * FN];
    memset(src, 0, sizeof(src));
    for (int y = 0; y < CELL; y++)
        for (int x = 0; x < CELL; x++) src[(y + PAD) * N + x + PAD] = page[(row * CELL + y) * 512 + col * CELL + x];
    for (int i = 0; i < FN; i++) {   /* rows, then columns */
        float c = (i + 0.5f) / UP, w[6], ws = 0;
        int j0 = (int)floorf(c - 2.5f);
        for (int k = 0; k < 6; k++) { w[k] = cubic(j0 + k + 0.5f - c); ws += w[k]; }
        for (int y = 0; y < N; y++) {
            float s = 0;
            for (int k = 0; k < 6; k++) { int j = j0 + k; if (j >= 0 && j < N) s += w[k] * src[y * N + j]; }
            tmp[y * FN + i] = s / ws;
        }
    }
    for (int i = 0; i < FN; i++) {
        float c = (i + 0.5f) / UP, w[6], ws = 0;
        int j0 = (int)floorf(c - 2.5f);
        for (int k = 0; k < 6; k++) { w[k] = cubic(j0 + k + 0.5f - c); ws += w[k]; }
        for (int x = 0; x < FN; x++) {
            float s = 0;
            for (int k = 0; k < 6; k++) { int j = j0 + k; if (j >= 0 && j < N) s += w[k] * tmp[j * FN + x]; }
            s /= ws;
            field[i * FN + x] = s == LEVEL ? LEVEL - 0.001f : s;   /* (exactly the level: outside) */
        }
    }
}
/* the crossing on the edge from sample (x0,y0) to (x1,y1), cell pixels */
static Pt crossing(int x0, int y0, int x1, int y1)
{
    float f0 = field[y0 * FN + x0], f1 = field[y1 * FN + x1], t = (LEVEL - f0) / (f1 - f0);
    Pt p = { (x0 + t * (x1 - x0) + 0.5f) / UP - PAD, (y0 + t * (y1 - y0) + 0.5f) / UP - PAD };
    return p;
}
/* edge ids: horizontal (x,y)-(x+1,y): y * FN + x; vertical (x,y)-(x,y+1): FN * FN + y * FN + x */
static void segment(int e0, int e1, int rx, int ry)
{
    Pt p = edge_pt[e0], q = edge_pt[e1], r = { (rx + 0.5f) / UP - PAD, (ry + 0.5f) / UP - PAD };
    float cr = (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
    int in = field[ry * FN + rx] > LEVEL;
    if ((cr > 0) != in) { int t = e0; e0 = e1; e1 = t; }   /* the inside on one side, always the same */
    edge_next[e0] = e1;
}
static float dist_seg(Pt p, Pt a, Pt b)
{
    float dx = b.x - a.x, dy = b.y - a.y, l = dx * dx + dy * dy, t = l > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    dx = a.x + t * dx - p.x; dy = a.y + t * dy - p.y;
    return sqrtf(dx * dx + dy * dy);
}
static void simplify(const Pt* p, int a, int b, float eps, unsigned char* keep)   /* Douglas-Peucker on p[a..b] */
{
    float best = 0;
    int bi = -1;
    for (int i = a + 1; i < b; i++) { float d = dist_seg(p[i], p[a], p[b]); if (d > best) { best = d; bi = i; } }
    if (bi < 0 || best <= eps) return;
    keep[bi] = 1;
    simplify(p, a, bi, eps, keep); simplify(p, bi, b, eps, keep);
}
/* the letter in cell (col,row) into g */
static void trace(const unsigned char* page, int col, int row, Glyph* g)
{
    static Pt loop[FN * FN], out[FN * FN];
    static unsigned char keep[FN * FN];
    upsample(page, col, row);
    memset(edge_next, 0xFF, sizeof(edge_next));
    memset(edge_seen, 0, sizeof(edge_seen));
    for (int y = 0; y + 1 < FN; y++)
        for (int x = 0; x + 1 < FN; x++) {
            int a = field[y * FN + x] > LEVEL, b = field[y * FN + x + 1] > LEVEL;
            int c = field[(y + 1) * FN + x + 1] > LEVEL, d = field[(y + 1) * FN + x] > LEVEL;
            int top = y * FN + x, bot = (y + 1) * FN + x, left = FN * FN + y * FN + x, right = FN * FN + y * FN + x + 1;
            int e[4], n = 0;
            if (a == b && b == c && c == d) continue;
            if (a != b) { edge_pt[top] = crossing(x, y, x + 1, y); e[n++] = top; }
            if (b != c) { edge_pt[right] = crossing(x + 1, y, x + 1, y + 1); e[n++] = right; }
            if (d != c) { edge_pt[bot] = crossing(x, y + 1, x + 1, y + 1); e[n++] = bot; }
            if (a != d) { edge_pt[left] = crossing(x, y, x, y + 1); e[n++] = left; }
            if (n == 2) {   /* the corner farthest from the level tells the sides */
                int rx = x, ry = y;
                float best = -1;
                for (int k = 0; k < 4; k++) {
                    int cx = x + (k == 1 || k == 2), cy = y + (k >= 2);
                    float v = fabsf(field[cy * FN + cx] - LEVEL);
                    if (v > best) { best = v; rx = cx; ry = cy; }
                }
                segment(e[0], e[1], rx, ry);
            } else {   /* a saddle: the centre decides which corners the inside joins */
                float ctr = (field[y * FN + x] + field[y * FN + x + 1] + field[(y + 1) * FN + x + 1] + field[(y + 1) * FN + x]) / 4;
                if ((ctr > LEVEL) == (a != 0)) {   /* corners b and d cut off */
                    segment(top, right, x + 1, y); segment(bot, left, x, y + 1);
                } else {                           /* corners a and c cut off */
                    segment(left, top, x, y); segment(right, bot, x + 1, y + 1);
                }
            }
        }
    for (int s = 0; s < 2 * FN * FN; s++) {
        int n = 0, m = 0;
        if (edge_next[s] < 0 || edge_seen[s]) continue;
        for (int e = s; !edge_seen[e] && edge_next[e] >= 0 && n < FN * FN - 1; e = edge_next[e]) { edge_seen[e] = 1; loop[n++] = edge_pt[e]; }
        if (n < 3 || fabsf(area(loop, n)) < 0.25f) continue;   /* specks and pinholes */
        for (int pass = 0; pass < SMOOTH; pass++) {   /* the pixel steps evened out (a closed moving average), corners kept */
            for (int i = 0; i < n; i++) {
                Pt a = loop[(i + n - 1) % n], b = loop[i], c = loop[(i + 1) % n];
                float ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
                float lu = sqrtf(ux * ux + uy * uy), lv = sqrtf(vx * vx + vy * vy);
                if (lu > 0 && lv > 0 && (ux * vx + uy * vy) / (lu * lv) < -0.2f) { out[i] = b; continue; }   /* a sharp turn: a corner */
                out[i].x = (a.x + 2 * b.x + c.x) / 4; out[i].y = (a.y + 2 * b.y + c.y) / 4;
            }
            memcpy(loop, out, n * sizeof(Pt));
        }
        memset(keep, 0, n);
        {   /* simplified: split at the point farthest from the first */
            int far = 0;
            float best = -1;
            for (int i = 1; i < n; i++) { float dx = loop[i].x - loop[0].x, dy = loop[i].y - loop[0].y; if (dx * dx + dy * dy > best) { best = dx * dx + dy * dy; far = i; } }
            loop[n] = loop[0];
            keep[0] = keep[far] = 1;
            simplify(loop, 0, far, 0.03f, keep); simplify(loop, far, n, 0.03f, keep);
        }
        for (int i = 0; i < n; i++) if (keep[i]) out[m++] = loop[i];
        add_contour(g, out, m, 0);
    }
}

/* ---------------- the letters ---------------- */

enum { GRAVE, ACUTE, CIRCUMFLEX, TILDE, DIAERESIS, RING, CARON, DOT, CEDILLA, OGONEK };
/* Latin-1 and Latin Extended-A letters as base + mark (Unicode's canonical decompositions) */
static const struct { uint16_t cp; char base; unsigned char mark; } composed[] = {
    { 0x00C0, 'A', GRAVE }, { 0x00C1, 'A', ACUTE }, { 0x00C2, 'A', CIRCUMFLEX }, { 0x00C3, 'A', TILDE }, { 0x00C4, 'A', DIAERESIS }, { 0x00C5, 'A', RING },
    { 0x00C7, 'C', CEDILLA }, { 0x00C8, 'E', GRAVE }, { 0x00C9, 'E', ACUTE }, { 0x00CA, 'E', CIRCUMFLEX }, { 0x00CB, 'E', DIAERESIS }, { 0x00CC, 'I', GRAVE },
    { 0x00CD, 'I', ACUTE }, { 0x00CE, 'I', CIRCUMFLEX }, { 0x00CF, 'I', DIAERESIS }, { 0x00D1, 'N', TILDE }, { 0x00D2, 'O', GRAVE }, { 0x00D3, 'O', ACUTE },
    { 0x00D4, 'O', CIRCUMFLEX }, { 0x00D5, 'O', TILDE }, { 0x00D6, 'O', DIAERESIS }, { 0x00D9, 'U', GRAVE }, { 0x00DA, 'U', ACUTE }, { 0x00DB, 'U', CIRCUMFLEX },
    { 0x00DC, 'U', DIAERESIS }, { 0x00DD, 'Y', ACUTE }, { 0x00E0, 'a', GRAVE }, { 0x00E1, 'a', ACUTE }, { 0x00E2, 'a', CIRCUMFLEX }, { 0x00E3, 'a', TILDE },
    { 0x00E4, 'a', DIAERESIS }, { 0x00E5, 'a', RING }, { 0x00E7, 'c', CEDILLA }, { 0x00E8, 'e', GRAVE }, { 0x00E9, 'e', ACUTE }, { 0x00EA, 'e', CIRCUMFLEX },
    { 0x00EB, 'e', DIAERESIS }, { 0x00EC, 'i', GRAVE }, { 0x00ED, 'i', ACUTE }, { 0x00EE, 'i', CIRCUMFLEX }, { 0x00EF, 'i', DIAERESIS }, { 0x00F1, 'n', TILDE },
    { 0x00F2, 'o', GRAVE }, { 0x00F3, 'o', ACUTE }, { 0x00F4, 'o', CIRCUMFLEX }, { 0x00F5, 'o', TILDE }, { 0x00F6, 'o', DIAERESIS }, { 0x00F9, 'u', GRAVE },
    { 0x00FA, 'u', ACUTE }, { 0x00FB, 'u', CIRCUMFLEX }, { 0x00FC, 'u', DIAERESIS }, { 0x00FD, 'y', ACUTE }, { 0x00FF, 'y', DIAERESIS }, { 0x0104, 'A', OGONEK },
    { 0x0105, 'a', OGONEK }, { 0x0106, 'C', ACUTE }, { 0x0107, 'c', ACUTE }, { 0x0108, 'C', CIRCUMFLEX }, { 0x0109, 'c', CIRCUMFLEX }, { 0x010A, 'C', DOT },
    { 0x010B, 'c', DOT }, { 0x010C, 'C', CARON }, { 0x010D, 'c', CARON }, { 0x010E, 'D', CARON }, { 0x010F, 'd', CARON }, { 0x0116, 'E', DOT },
    { 0x0117, 'e', DOT }, { 0x0118, 'E', OGONEK }, { 0x0119, 'e', OGONEK }, { 0x011A, 'E', CARON }, { 0x011B, 'e', CARON }, { 0x011C, 'G', CIRCUMFLEX },
    { 0x011D, 'g', CIRCUMFLEX }, { 0x0120, 'G', DOT }, { 0x0121, 'g', DOT }, { 0x0122, 'G', CEDILLA }, { 0x0123, 'g', CEDILLA }, { 0x0124, 'H', CIRCUMFLEX },
    { 0x0125, 'h', CIRCUMFLEX }, { 0x0128, 'I', TILDE }, { 0x0129, 'i', TILDE }, { 0x012E, 'I', OGONEK }, { 0x012F, 'i', OGONEK }, { 0x0130, 'I', DOT },
    { 0x0134, 'J', CIRCUMFLEX }, { 0x0135, 'j', CIRCUMFLEX }, { 0x0136, 'K', CEDILLA }, { 0x0137, 'k', CEDILLA }, { 0x0139, 'L', ACUTE }, { 0x013A, 'l', ACUTE },
    { 0x013B, 'L', CEDILLA }, { 0x013C, 'l', CEDILLA }, { 0x013D, 'L', CARON }, { 0x013E, 'l', CARON }, { 0x0143, 'N', ACUTE }, { 0x0144, 'n', ACUTE },
    { 0x0145, 'N', CEDILLA }, { 0x0146, 'n', CEDILLA }, { 0x0147, 'N', CARON }, { 0x0148, 'n', CARON }, { 0x0154, 'R', ACUTE }, { 0x0155, 'r', ACUTE },
    { 0x0156, 'R', CEDILLA }, { 0x0157, 'r', CEDILLA }, { 0x0158, 'R', CARON }, { 0x0159, 'r', CARON }, { 0x015A, 'S', ACUTE }, { 0x015B, 's', ACUTE },
    { 0x015C, 'S', CIRCUMFLEX }, { 0x015D, 's', CIRCUMFLEX }, { 0x015E, 'S', CEDILLA }, { 0x015F, 's', CEDILLA }, { 0x0160, 'S', CARON }, { 0x0161, 's', CARON },
    { 0x0162, 'T', CEDILLA }, { 0x0163, 't', CEDILLA }, { 0x0164, 'T', CARON }, { 0x0165, 't', CARON }, { 0x0168, 'U', TILDE }, { 0x0169, 'u', TILDE },
    { 0x016E, 'U', RING }, { 0x016F, 'u', RING }, { 0x0172, 'U', OGONEK }, { 0x0173, 'u', OGONEK }, { 0x0174, 'W', CIRCUMFLEX }, { 0x0175, 'w', CIRCUMFLEX },
    { 0x0176, 'Y', CIRCUMFLEX }, { 0x0177, 'y', CIRCUMFLEX }, { 0x0178, 'Y', DIAERESIS }, { 0x0179, 'Z', ACUTE }, { 0x017A, 'z', ACUTE }, { 0x017B, 'Z', DOT },
    { 0x017C, 'z', DOT }, { 0x017D, 'Z', CARON }, { 0x017E, 'z', CARON },
};

static float outer;   /* the winding of outer contours (area sign, cell pixels): the O's largest */
static float baseline = 22;   /* under the capitals, cell pixels from the top */

static void poly(Glyph* g, const float* xy, int n, float cx, float y, float sign)   /* points relative to (cx, y) */
{
    Pt p[24];
    for (int i = 0; i < n; i++) { p[i].x = cx + xy[2 * i]; p[i].y = y + xy[2 * i + 1]; }
    add_contour(g, p, n, sign);
}
static void dot_at(Glyph* g, float cx, float cy, float r, int n, float sign)
{
    Pt p[24];
    for (int i = 0; i < n; i++) { float a = 6.2831853f * i / n; p[i].x = cx + r * cosf(a); p[i].y = cy + r * sinf(a); }
    add_contour(g, p, n, sign);
}
/* a mark centred on cx, sitting on y (above a letter) or hanging from it (cedilla, ogonek) */
static void mark(Glyph* g, int m, float cx, float y)
{
    static const float acute[] = { -1.4f, 0, 0.2f, 0, 2.6f, -3.4f, 0.6f, -3.8f };
    static const float grave[] = { 1.4f, 0, -0.2f, 0, -2.6f, -3.4f, -0.6f, -3.8f };
    static const float circ[] = { -3.2f, 0, -1.7f, 0, 0, -2.0f, 1.7f, 0, 3.2f, 0, 0.9f, -3.6f, -0.9f, -3.6f };
    static const float caron[] = { -3.2f, -3.6f, -1.7f, -3.6f, 0, -1.6f, 1.7f, -3.6f, 3.2f, -3.6f, 0.9f, 0, -0.9f, 0 };
    static const float cedilla[] = { -0.6f, -0.6f, 0.6f, -0.6f, 0.6f, 1.0f, 2.2f, 1.8f, 1.8f, 3.5f, -1.8f, 3.7f, -1.8f, 2.9f, 0.4f, 2.7f, 0.6f, 2.1f, -0.6f, 1.5f };
    static const float ogonek[] = { 0.2f, -0.6f, 1.6f, -0.6f, 0.6f, 1.4f, 1.0f, 2.5f, 2.6f, 2.6f, 2.4f, 3.5f, -0.4f, 3.4f, -1.0f, 2.0f };
    switch (m) {
    case ACUTE: poly(g, acute, 4, cx, y, outer); break;
    case GRAVE: poly(g, grave, 4, cx, y, outer); break;
    case CIRCUMFLEX: poly(g, circ, 7, cx, y, outer); break;
    case CARON: poly(g, caron, 7, cx, y, outer); break;
    case CEDILLA: poly(g, cedilla, 10, cx, y, outer); break;
    case OGONEK: poly(g, ogonek, 8, cx, y, outer); break;
    case TILDE: {   /* a wave, up on the left, down on the right */
        float xy[52];
        for (int i = 0; i < 13; i++) {
            float t = -1 + i / 6.0f, w = -1.9f + 1.1f * sinf(3.14159265f * t);
            xy[2 * i] = 3.2f * t; xy[2 * i + 1] = w - 0.75f;
            xy[2 * (25 - i)] = 3.2f * t; xy[2 * (25 - i) + 1] = w + 0.75f;
        }
        { Pt p[26]; for (int i = 0; i < 26; i++) { p[i].x = cx + xy[2 * i]; p[i].y = y + xy[2 * i + 1]; } add_contour(g, p, 26, outer); }
        break;
    }
    case DIAERESIS: dot_at(g, cx - 2.2f, y - 1.6f, 1.25f, 16, outer); dot_at(g, cx + 2.2f, y - 1.6f, 1.25f, 16, outer); break;
    case DOT: dot_at(g, cx, y - 1.6f, 1.35f, 16, outer); break;
    case RING: dot_at(g, cx, y - 2.2f, 2.0f, 20, outer); dot_at(g, cx, y - 2.2f, 0.95f, 20, -outer); break;
    }
}
static void chevron(Glyph* g, float cx, float cy, float d)   /* half of « or » */
{
    float xy[] = { d * 1.6f, -2.6f, d * 2.8f, -2.6f, d * 1.0f, 0, d * 2.8f, 2.6f, d * 1.6f, 2.6f, -d * 0.4f, 0 };
    poly(g, xy, 6, cx, cy, outer);
}

/* the page's letters and the composed ones into glyphs; the letters traced, 0 if the page has none */
static int letters(const unsigned char* page)
{
    static const char skip[] = "#$&*<=>@[\\]^`{|}";   /* their cells hold the game's own symbols */
    static const struct { int code; uint32_t cp; } extra[] = { { 2, 0x201D }, { 7, 0x2019 }, { 60, 0x00D7 }, { 62, 0x00B7 }, { 91, 0x03B1 }, { 92, 0x03A3 }, { 95, 0x00AE } };
    float base = 22, xh = 9, cap = 1.5f, x0, y0, x1, y1;
    int got = 0;
    Glyph* g, *s;
    for (int code = 1; code < 96; code++) {
        uint32_t cp = code < 95 ? (uint32_t)(code + 32) : 0;
        for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); i++) if (extra[i].code == code) cp = extra[i].cp;
        if (!cp || (cp < 128 && strchr(skip, (int)cp))) continue;
        g = glyph_new(cp);
        trace(page, code % 18, code / 18, g);
        if (g->nc) got++;
        else { free(g->c); nglyphs--; }   /* an empty cell: no glyph, so a merged font supplies the letter */
    }
    if (!got) return 0;
    glyph_new(' ')->adv = SPACE; glyph_new(0xA0)->adv = SPACE;
    {   /* the baseline under the capitals, the x-height and cap tops */
        float b = -1, xt = 1e9f, ct = 1e9f;
        for (const char* c = "HIEFLT"; *c; c++) if ((g = glyph_drawn(*c))) { bbox(g, &x0, &y0, &x1, &y1); if (y1 > b) b = y1; }
        for (const char* c = "xzvw"; *c; c++) if ((g = glyph_drawn(*c))) { bbox(g, &x0, &y0, &x1, &y1); if (y0 < xt) xt = y0; }
        for (const char* c = "HIEFT"; *c; c++) if ((g = glyph_drawn(*c))) { bbox(g, &x0, &y0, &x1, &y1); if (y0 < ct) ct = y0; }
        if (b > 0) base = b;
        if (xt < 1e9f) xh = xt;
        if (ct < 1e9f) cap = ct;
        baseline = base;
    }
    outer = 1;
    if ((g = glyph_drawn('O'))) {
        float big = 0;
        for (int i = 0; i < g->nc; i++) { float a = area(g->c[i].p, g->c[i].n); if (fabsf(a) > big) { big = fabsf(a); outer = a > 0 ? 1.0f : -1.0f; } }
    }
    for (int k = 0; k < 2; k++) {   /* dotless i, j: without the dot (the contours wholly above the x-height) */
        Glyph* src = glyph_drawn(k ? 'j' : 'i');
        if (!src) continue;
        g = glyph_new(k ? 0x237 : 0x131);
        src = glyph_get(k ? 'j' : 'i');
        for (int i = 0; i < src->nc; i++) {
            float lo = -1e9f;
            for (int p = 0; p < src->c[i].n; p++) if (src->c[i].p[p].y > lo) lo = src->c[i].p[p].y;
            if (lo > xh + 1.0f) add_contour(g, src->c[i].p, src->c[i].n, 0);
        }
    }
    for (size_t i = 0; i < sizeof(composed) / sizeof(composed[0]); i++) {
        int m = composed[i].mark, upper = composed[i].base >= 'A' && composed[i].base <= 'Z';
        uint32_t b = (uint32_t)composed[i].base;
        if (m != CEDILLA && m != OGONEK && (b == 'i' || b == 'j')) b = b == 'i' ? 0x131 : 0x237;
        if (!(s = glyph_drawn(b))) continue;
        g = glyph_new(composed[i].cp);
        s = glyph_get(b);   /* (glyph_new may move the table) */
        add_xform(g, s, 1, 0, 1, 0);
        bbox(s, &x0, &y0, &x1, &y1);
        if (m == CEDILLA) mark(g, m, (x0 + x1) / 2, base);
        else if (m == OGONEK) mark(g, m, x1 - 1.6f, base);
        else {
            float top = (upper ? cap : xh) - 1.2f;
            if (strchr("bdfhklt", composed[i].base)) top = y0 - 1.0f;
            mark(g, m, (x0 + x1) / 2 + (m == ACUTE ? 0.4f : 0), top);
        }
    }
    for (int k = 0; k < 2; k++) {   /* ł Ł: a stroke across the stem */
        uint32_t b = k ? 'L' : 'l';
        if (!glyph_drawn(b)) continue;
        g = glyph_new(k ? 0x141 : 0x142); s = glyph_get(b);
        add_xform(g, s, 1, 0, 1, 0);
        bbox(s, &x0, &y0, &x1, &y1);
        {
            float cx = x0 + (x1 - x0) * (k ? 0.3f : 0.5f), cy = (y0 + base) / 2 + (k ? 1 : 0);
            float xy[] = { -3.0f, 1.9f, -3.0f, 0.5f, 3.0f, -1.9f, 3.0f, -0.5f };
            poly(g, xy, 4, cx, cy, outer);
        }
    }
    for (int k = 0; k < 2; k++) {   /* ¡ ¿: ! ? turned, under the baseline */
        uint32_t b = k ? '?' : '!';
        if (!glyph_drawn(b)) continue;
        g = glyph_new(k ? 0xBF : 0xA1); s = glyph_get(b);
        bbox(s, &x0, &y0, &x1, &y1);
        add_xform(g, s, -1, x0 + x1, -1, y0 + base + 3.0f);
    }
    for (int k = 0; k < 2; k++) {   /* “ ‘: ” ’ turned */
        uint32_t b = k ? 0x2019 : 0x201D;
        if (!glyph_drawn(b)) continue;
        g = glyph_new(k ? 0x2018 : 0x201C); s = glyph_get(b);
        bbox(s, &x0, &y0, &x1, &y1);
        add_xform(g, s, -1, x0 + x1, -1, y0 + y1);
    }
    if (glyph_drawn(0x201D)) {   /* „: ” lowered; " ': the page's curly quotes */
        g = glyph_new(0x201E); s = glyph_get(0x201D);
        bbox(s, &x0, &y0, &x1, &y1);
        add_xform(g, s, 1, 0, 1, base + 3.0f - y1);
        g = glyph_new('"'); add_xform(g, glyph_get(0x201D), 1, 0, 1, 0);
    }
    if (glyph_drawn(0x2019)) { g = glyph_new('\''); add_xform(g, glyph_get(0x2019), 1, 0, 1, 0); }
    if (glyph_drawn('-')) {   /* – —: the hyphen drawn out */
        for (int k = 0; k < 2; k++) {
            float w = k ? 16.0f : 9.0f;
            g = glyph_new(k ? 0x2014 : 0x2013); s = glyph_get('-');
            bbox(s, &x0, &y0, &x1, &y1);
            if (x1 > x0) add_xform(g, s, w / (x1 - x0), 1.0f - x0 * w / (x1 - x0), 1, 0);
        }
    }
    if (glyph_drawn('.')) {   /* …: three full stops */
        g = glyph_new(0x2026); s = glyph_get('.');
        bbox(s, &x0, &y0, &x1, &y1);
        for (int k = 0; k < 3; k++) add_xform(g, s, 1, -x0 + 1.0f + k * (x1 - x0 + 1.6f), 1, 0);
    }
    {   /* « » */
        float mid = (xh + base) / 2;
        g = glyph_new(0xAB); chevron(g, 3.0f, mid, 1); chevron(g, 6.6f, mid, 1);
        g = glyph_new(0xBB); chevron(g, 9.0f, mid, -1); chevron(g, 5.4f, mid, -1);
    }
    for (int k = 0; k < 2; k++) {   /* œ Œ: o e / O E run together */
        uint32_t a = k ? 'O' : 'o', b = k ? 'E' : 'e';
        float xa0, ya0, xa1, ya1;
        if (!glyph_drawn(a) || !glyph_drawn(b)) continue;
        g = glyph_new(k ? 0x152 : 0x153);
        bbox(glyph_get(a), &xa0, &ya0, &xa1, &ya1); bbox(glyph_get(b), &x0, &y0, &x1, &y1);
        add_xform(g, glyph_get(a), 1, 0, 1, 0);
        add_xform(g, glyph_get(b), 1, xa1 - x0 - (k ? 2.2f : 1.4f), 1, 0);
    }
    for (int i = 0; i < nglyphs; i++) {   /* spaced by the ink */
        Glyph* h = &glyphs[i];
        if (!h->nc) continue;
        bbox(h, &x0, &y0, &x1, &y1);
        for (int c = 0; c < h->nc; c++) for (int p = 0; p < h->c[c].n; p++) h->c[c].p[p].x += SIDE - x0;
        h->adv = x1 - x0 + 2 * SIDE;
    }
    return got;
}

/* ---------------- TrueType ---------------- */

typedef struct { unsigned char* d; int n, cap; } Buf;
static void put(Buf* b, const void* p, int n)
{
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 256; b->d = realloc(b->d, b->cap); }
    memcpy(b->d + b->n, p, n); b->n += n;
}
static void u8(Buf* b, int v) { unsigned char c = (unsigned char)v; put(b, &c, 1); }
static void u16(Buf* b, int v) { u8(b, v >> 8); u8(b, v); }
static void u32(Buf* b, uint32_t v) { u16(b, (int)(v >> 16)); u16(b, (int)(v & 0xFFFF)); }
static void set16(Buf* b, int at, int v) { b->d[at] = (unsigned char)(v >> 8); b->d[at + 1] = (unsigned char)v; }
static void set32(Buf* b, int at, uint32_t v) { set16(b, at, (int)(v >> 16)); set16(b, at + 2, (int)(v & 0xFFFF)); }
static uint32_t checksum(const unsigned char* d, int n)
{
    uint32_t s = 0;
    for (int i = 0; i < n; i += 4) s += (uint32_t)d[i] << 24 | (uint32_t)(i + 1 < n ? d[i + 1] : 0) << 16 | (uint32_t)(i + 2 < n ? d[i + 2] : 0) << 8 | (i + 3 < n ? d[i + 3] : 0);
    return s;
}
static int cmp_cp(const void* a, const void* b) { uint32_t x = ((const Glyph*)a)->cp, y = ((const Glyph*)b)->cp; return x < y ? -1 : x > y; }

static unsigned char* font_file(float base, int* size)
{
    const float U = 1000.0f / CELL;
    int asc = (int)lroundf(base * U), desc = (int)lroundf((CELL - base) * U), n = nglyphs + 1;
    int xmin = 0, ymin = 0, xmax = 0, ymax = 0, maxpts = 0, maxctr = 0, advmax = 0, minlsb = 0, minrsb = 0, maxext = 0, avg = 0, navg = 0;
    Buf glyf = { 0 }, loca = { 0 }, hmtx = { 0 }, cmap = { 0 }, head = { 0 }, hhea = { 0 }, maxp = { 0 }, os2 = { 0 }, name = { 0 }, post = { 0 }, out = { 0 };
    int* lsb = calloc(n, sizeof(int)), *adv = calloc(n, sizeof(int));
    int reverse = outer < 0;   /* y flips the winding; TrueType's outer contours are clockwise (negative area) */
    qsort(glyphs, nglyphs, sizeof(Glyph), cmp_cp);
    adv[0] = 500;
    u32(&loca, 0); u32(&loca, 0);   /* .notdef: empty */
    for (int i = 1; i < n; i++) {   /* glyf: on-curve points only, coordinates as 16-bit deltas */
        Glyph* g = &glyphs[i - 1];
        int npt = 0, nc = 0, gx0 = 32767, gy0 = 32767, gx1 = -32768, gy1 = -32768;
        short* xs = NULL, *ys = NULL;
        int* ends = NULL;
        adv[i] = (int)lroundf(g->adv * U);
        for (int c = 0; c < g->nc; c++) {
            int first = npt;
            xs = realloc(xs, (npt + g->c[c].n) * sizeof(short)); ys = realloc(ys, (npt + g->c[c].n) * sizeof(short));
            for (int k = 0; k < g->c[c].n; k++) {
                Pt p = g->c[c].p[reverse ? g->c[c].n - 1 - k : k];
                short x = (short)lroundf(p.x * U), y = (short)lroundf((base - p.y) * U);
                if (npt > first && xs[npt - 1] == x && ys[npt - 1] == y) continue;   /* (rounded together) */
                xs[npt] = x; ys[npt++] = y;
            }
            while (npt - first > 1 && xs[npt - 1] == xs[first] && ys[npt - 1] == ys[first]) npt--;
            if (npt - first < 3) { npt = first; continue; }
            ends = realloc(ends, (nc + 1) * sizeof(int)); ends[nc++] = npt - 1;
        }
        for (int k = 0; k < npt; k++) {
            if (xs[k] < gx0) gx0 = xs[k];
            if (xs[k] > gx1) gx1 = xs[k];
            if (ys[k] < gy0) gy0 = ys[k];
            if (ys[k] > gy1) gy1 = ys[k];
        }
        if (nc) {
            int px = 0, py = 0;
            u16(&glyf, nc); u16(&glyf, gx0); u16(&glyf, gy0); u16(&glyf, gx1); u16(&glyf, gy1);
            for (int c = 0; c < nc; c++) u16(&glyf, ends[c]);
            u16(&glyf, 0);   /* no instructions */
            for (int k = 0; k < npt; k++) u8(&glyf, 1);
            for (int k = 0; k < npt; k++) { u16(&glyf, xs[k] - px); px = xs[k]; }
            for (int k = 0; k < npt; k++) { u16(&glyf, ys[k] - py); py = ys[k]; }
            while (glyf.n & 3) u8(&glyf, 0);
            lsb[i] = gx0;
            if (gx0 < xmin || i == 1) xmin = gx0;
            if (gy0 < ymin) ymin = gy0;
            if (gx1 > xmax) xmax = gx1;
            if (gy1 > ymax) ymax = gy1;
            if (npt > maxpts) maxpts = npt;
            if (nc > maxctr) maxctr = nc;
            if (gx0 < minlsb) minlsb = gx0;
            if (adv[i] - gx1 < minrsb) minrsb = adv[i] - gx1;
            if (gx1 > maxext) maxext = gx1;
        }
        if (adv[i] > advmax) advmax = adv[i];
        if (adv[i]) { avg += adv[i]; navg++; }
        u32(&loca, (uint32_t)glyf.n);
        free(xs); free(ys); free(ends);
    }
    for (int i = 0; i < n; i++) { u16(&hmtx, adv[i]); u16(&hmtx, lsb[i]); }
    {   /* cmap: format 4, one segment per run of consecutive characters (their glyphs are consecutive too) */
        Buf seg = { 0 };
        int nseg = 0, len;
        uint16_t sc[300], ec[300], dl[300];
        for (int i = 0; i < nglyphs && nseg < 299; ) {
            int j = i;
            while (j + 1 < nglyphs && glyphs[j + 1].cp == glyphs[j].cp + 1) j++;
            sc[nseg] = (uint16_t)glyphs[i].cp; ec[nseg] = (uint16_t)glyphs[j].cp; dl[nseg] = (uint16_t)(i + 1 - (int)glyphs[i].cp);
            nseg++; i = j + 1;
        }
        sc[nseg] = ec[nseg] = 0xFFFF; dl[nseg] = 1; nseg++;
        {
            int sr = 2, es = 0;
            while (sr * 2 <= nseg * 2) { sr *= 2; es++; }
            for (int i = 0; i < nseg; i++) u16(&seg, ec[i]);
            u16(&seg, 0);
            for (int i = 0; i < nseg; i++) u16(&seg, sc[i]);
            for (int i = 0; i < nseg; i++) u16(&seg, dl[i]);
            for (int i = 0; i < nseg; i++) u16(&seg, 0);
            len = 14 + seg.n;
            u16(&cmap, 0); u16(&cmap, 1); u16(&cmap, 3); u16(&cmap, 1); u32(&cmap, 12);
            u16(&cmap, 4); u16(&cmap, len); u16(&cmap, 0); u16(&cmap, nseg * 2); u16(&cmap, sr); u16(&cmap, es); u16(&cmap, nseg * 2 - sr);
            put(&cmap, seg.d, seg.n);
        }
        free(seg.d);
    }
    u32(&head, 0x00010000); u32(&head, 0x00010000); u32(&head, 0); u32(&head, 0x5F0F3CF5); u16(&head, 1); u16(&head, 1000);
    for (int i = 0; i < 4; i++) u32(&head, 0);   /* created, modified */
    u16(&head, xmin); u16(&head, ymin); u16(&head, xmax); u16(&head, ymax); u16(&head, 0); u16(&head, 6); u16(&head, 2); u16(&head, 1); u16(&head, 0);
    u32(&hhea, 0x00010000); u16(&hhea, asc); u16(&hhea, -desc); u16(&hhea, 0); u16(&hhea, advmax); u16(&hhea, minlsb); u16(&hhea, minrsb); u16(&hhea, maxext);
    u16(&hhea, 1); u16(&hhea, 0); u16(&hhea, 0); for (int i = 0; i < 4; i++) u16(&hhea, 0); u16(&hhea, 0); u16(&hhea, n);
    u32(&maxp, 0x00010000); u16(&maxp, n); u16(&maxp, maxpts); u16(&maxp, maxctr); u16(&maxp, 0); u16(&maxp, 0); u16(&maxp, 2);
    for (int i = 0; i < 8; i++) u16(&maxp, 0);
    {   /* OS/2 version 4 */
        Glyph* xg = glyph_get('x'), *hg = glyph_get('H');
        int xht = 0, cht = 0;
        for (int k = 0; xg && k < xg->nc; k++) for (int p = 0; p < xg->c[k].n; p++) { int y = (int)lroundf((base - xg->c[k].p[p].y) * U); if (y > xht) xht = y; }
        for (int k = 0; hg && k < hg->nc; k++) for (int p = 0; p < hg->c[k].n; p++) { int y = (int)lroundf((base - hg->c[k].p[p].y) * U); if (y > cht) cht = y; }
        u16(&os2, 4); u16(&os2, navg ? avg / navg : 500); u16(&os2, 400); u16(&os2, 5); u16(&os2, 0);
        u16(&os2, 650); u16(&os2, 600); u16(&os2, 0); u16(&os2, 75); u16(&os2, 650); u16(&os2, 600); u16(&os2, 0); u16(&os2, 350); u16(&os2, 50); u16(&os2, 300);
        u16(&os2, 0);
        for (int i = 0; i < 10; i++) u8(&os2, 0);   /* panose */
        u32(&os2, 0x80000087u); u32(&os2, 0); u32(&os2, 0); u32(&os2, 0);   /* Basic Latin, Latin-1, Latin Extended-A, Greek, punctuation */
        put(&os2, "NONE", 4); u16(&os2, 0x40); u16(&os2, glyphs[0].cp); u16(&os2, glyphs[nglyphs - 1].cp > 0xFFFF ? 0xFFFF : (int)glyphs[nglyphs - 1].cp);
        u16(&os2, asc); u16(&os2, -desc); u16(&os2, 0); u16(&os2, asc + 200); u16(&os2, desc); u32(&os2, 1); u32(&os2, 0);
        u16(&os2, xht); u16(&os2, cht); u16(&os2, 0); u16(&os2, ' '); u16(&os2, 0);
    }
    {   /* name: family, style, full name, PostScript name */
        static const char* s[] = { "CVX Subtitles", "Regular", "CVX Subtitles", "CVXSubtitles" };
        static const int id[] = { 1, 2, 4, 6 };
        int off = 0;
        u16(&name, 0); u16(&name, 4); u16(&name, 6 + 4 * 12);
        for (int i = 0; i < 4; i++) { int l = (int)strlen(s[i]) * 2; u16(&name, 3); u16(&name, 1); u16(&name, 0x409); u16(&name, id[i]); u16(&name, l); u16(&name, off); off += l; }
        for (int i = 0; i < 4; i++) for (const char* c = s[i]; *c; c++) u16(&name, *c);
    }
    u32(&post, 0x00030000); u32(&post, 0); u16(&post, -100); u16(&post, 50); for (int i = 0; i < 5; i++) u32(&post, 0);
    {   /* the file: the table directory (tags in order), the tables 4-aligned */
        struct { const char* tag; Buf* b; } t[] = { { "OS/2", &os2 }, { "cmap", &cmap }, { "glyf", &glyf }, { "head", &head }, { "hhea", &hhea },
                                                    { "hmtx", &hmtx }, { "loca", &loca }, { "maxp", &maxp }, { "name", &name }, { "post", &post } };
        int nt = 10, off = 12 + 16 * nt, head_at = 0;
        u32(&out, 0x00010000); u16(&out, nt); u16(&out, 128); u16(&out, 3); u16(&out, nt * 16 - 128);
        for (int i = 0; i < nt; i++) {
            put(&out, t[i].tag, 4); u32(&out, checksum(t[i].b->d, t[i].b->n)); u32(&out, (uint32_t)off); u32(&out, (uint32_t)t[i].b->n);
            if (!strcmp(t[i].tag, "head")) head_at = off;
            off += (t[i].b->n + 3) & ~3;
        }
        for (int i = 0; i < nt; i++) { put(&out, t[i].b->d, t[i].b->n); while (out.n & 3) u8(&out, 0); free(t[i].b->d); }
        set32(&out, head_at + 8, 0xB1B0AFBAu - checksum(out.d, out.n));
    }
    free(lsb); free(adv);
    *size = out.n;
    return out.d;
}

unsigned char* game_font_from_page(const unsigned char* page, int* size, int* count)
{
    unsigned char* ttf = NULL;
    int got = letters(page);
    *size = 0;
    if (count) *count = got ? nglyphs + 1 : 0;
    if (got) ttf = font_file(baseline, size);
    glyphs_free();
    return ttf;
}

/* ---------------- from the disc ---------------- */

static unsigned char* built;
static int built_size;

static uint32_t le32(const unsigned char* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

int game_font_build(void)
{
    uint32_t lsn, size, off, len;
    unsigned char toc[2048], *d, *page = NULL;
    int glyph_count = 0;
    clock_t t0 = clock();
    char own[1024];
    FILE* f;
    if (built) return 1;
    snprintf(own, sizeof(own), "%s/subtitles/fonts/cvx.otf", getenv("RECVX_MODS") ? getenv("RECVX_MODS") : "mods");   /* (as overlay.cpp's mods_dir) */
    if ((f = fopen(own, "rb"))) { fclose(f); return 0; }   /* the player's own font is used instead */
    if (!cdvd_find_file("\\SYSTEM.AFS;1", &lsn, &size) || !cdvd_read_sectors(lsn, 1, toc) || memcmp(toc, "AFS", 3) || le32(toc + 4) < 2) {
        host_log("subtitles: no SYSTEM.AFS on the disc, the game's lettering is not available\n");
        return 0;
    }
    off = le32(toc + 16); len = le32(toc + 20);   /* entry 1 */
    if (off % 2048 || !len || len > (16u << 20) || !(d = malloc((len + 2047) / 2048 * 2048))) return 0;
    if (cdvd_read_sectors(lsn + off / 2048, (len + 2047) / 2048, d)) {
        for (uint32_t p = 0; p + 192 <= len && !page; p += 4) {   /* the first 512 x 512 4-bit TIM2, 16 colours of 32 bits */
            uint32_t q = p + (d[p + 5] ? 128 : 16), clut, img, hdr;
            if (memcmp(d + p, "TIM2", 4) || q + 48 > len) continue;
            clut = le32(d + q + 4); img = le32(d + q + 8); hdr = d[q + 12] | d[q + 13] << 8;
            if (d[q + 19] != 4 || (d[q + 20] | d[q + 21] << 8) != 512 || (d[q + 22] | d[q + 23] << 8) != 512 || (d[q + 14] | d[q + 15] << 8) != 16 ||
                (d[q + 18] & 0x3F) != 3 || img < 512 * 256 || clut < 64 || q + hdr + img + clut > len) continue;
            if ((page = malloc(512 * 512)))
                for (int i = 0; i < 512 * 256; i++) {
                    unsigned char v = d[q + hdr + i];
                    page[2 * i] = d[q + hdr + img + (v & 15) * 4];          /* the palette's red: the level */
                    page[2 * i + 1] = d[q + hdr + img + (v >> 4) * 4];
                }
        }
    }
    free(d);
    if (!page) { host_log("subtitles: the disc's font page was not found, the game's lettering is not available\n"); return 0; }
    built = game_font_from_page(page, &built_size, &glyph_count);
    free(page);
    if (built) host_log("subtitles: the game's lettering made from the disc's font (%d glyphs, %d bytes, in memory, %d ms)\n", glyph_count, built_size,
                        (int)((clock() - t0) * 1000 / CLOCKS_PER_SEC));
    return built != NULL;
}

const unsigned char* game_font_data(int* size)
{
    *size = built_size;
    return built;
}
