/* Imports (pc/src/import, docs/IMPORT_OUTBREAK.md): the SLD decoder, TIM2 decoding (the CSM1
   palette order) and the two writers on a small synthetic humanoid (no disc data). */
#include "../src/import/impmodel.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int ob2_sld(const unsigned char* src, int n, unsigned char* dst, int cap);
void host_log(const char* fmt, ...) { (void)fmt; }
static uint32_t rd(const unsigned char* p) { uint32_t v; memcpy(&v, p, 4); return v; }

/* an 8-bit 16x16 TIM2 (16-byte header): pixel i has index i, palette entry k is (k, 0, 0, 0x80) */
static unsigned char* tim2(int* len)
{
    int n = 16 + 48 + 256 + 1024;
    unsigned char* t = calloc(1, n);
    uint32_t total = 48 + 256 + 1024, clut = 1024, img = 256;
    uint16_t hs = 48, nc = 256, w = 16, h = 16;
    memcpy(t, "TIM2", 4); t[4] = 4; t[6] = 1;
    memcpy(t + 16, &total, 4); memcpy(t + 20, &clut, 4); memcpy(t + 24, &img, 4); memcpy(t + 28, &hs, 2); memcpy(t + 30, &nc, 2);
    t[33] = 1; t[34] = 3; t[35] = 5; memcpy(t + 36, &w, 2); memcpy(t + 38, &h, 2);
    for (int i = 0; i < 256; i++) { t[64 + i] = (unsigned char)i; t[64 + 256 + 4 * i] = (unsigned char)i; t[64 + 256 + 4 * i + 3] = 0x80; }
    *len = n;
    return t;
}

int main(int argc, char** argv)
{
    const char* dir = argc > 1 ? argv[1] : ".";
    char p[1024];
    /* SLD: literals 1, 2; a copy of 3 words from 2 back; 2 zero words; the end */
    uint16_t in[] = { 0x3800, 1, 2, (3 << 11) | 2, (2 << 11) | 0, 0, 0 }, out[16];   /* flags: lit, lit, copy, copy, copy */
    CHECK(ob2_sld((unsigned char*)in, sizeof(in), (unsigned char*)out, sizeof(out)) == 7 * 2);
    CHECK(out[0] == 1 && out[1] == 2 && out[2] == 1 && out[3] == 2 && out[4] == 1 && out[5] == 0 && out[6] == 0);
    CHECK(ob2_sld((unsigned char*)in, sizeof(in), (unsigned char*)out, 8) < 0);   /* (no room: refused, not overrun) */

    /* TIM2: CSM1 swaps palette entries 8..15 and 16..23 of every 32 */
    int len, w, h;
    unsigned char *t = tim2(&len), *px;
    CHECK(imp_tim2_rgba(t, len, &w, &h, &px) && w == 16 && h == 16);
    CHECK(px[4 * 8] == 16 && px[4 * 16] == 8 && px[4 * 7] == 7 && px[4 * 24] == 24 && px[3] == 255);
    free(px);

    /* a humanoid: root, pelvis, spine, chest, neck, head, two arms of 3, two legs of 3, and a finger
       (zero length, merged into its hand) */
    static const struct { int par; float t[3]; int keep; } sk[] = {
        { -1, { 0, 0, 0 }, 1 }, { 0, { 0, 100, 0 }, 1 }, { 1, { 0, 10, 0 }, 1 }, { 2, { 0, 20, 0 }, 1 },
        { 3, { 0, 20, 0 }, 1 }, { 4, { 0, 8, 0 }, 1 },
        { 3, { 10, 15, 0 }, 1 }, { 6, { 25, 0, 0 }, 1 }, { 7, { 25, 0, 0 }, 1 },
        { 3, { -10, 15, 0 }, 1 }, { 9, { -25, 0, 0 }, 1 }, { 10, { -25, 0, 0 }, 1 },
        { 1, { 8, -10, 0 }, 1 }, { 12, { 0, -40, 0 }, 1 }, { 13, { 0, -40, 0 }, 1 },
        { 1, { -8, -10, 0 }, 1 }, { 15, { 0, -40, 0 }, 1 }, { 16, { 0, -40, 0 }, 1 },
        { 8, { 0, 0, 0 }, 0 },
    };
    ImpModel m;
    memset(&m, 0, sizeof(m));
    m.nn = (int)(sizeof(sk) / sizeof(sk[0]));
    for (int i = 0; i < m.nn; i++) { m.node[i].parent = sk[i].par; memcpy(m.node[i].t, sk[i].t, 12); m.node[i].q[3] = 1; m.node[i].keep = sk[i].keep; }
    m.nv = 4;
    m.pos = calloc(4, sizeof(*m.pos)); m.nrm = calloc(4, sizeof(*m.nrm)); m.uv = calloc(4, sizeof(*m.uv));
    m.jnt = calloc(4, sizeof(*m.jnt)); m.wgt = calloc(4, sizeof(*m.wgt));
    for (int i = 0; i < 4; i++) {
        m.pos[i][0] = 30.0f + 5 * (i & 1); m.pos[i][1] = 140.0f + 5 * (i >> 1); m.nrm[i][2] = 1;
        m.uv[i][0] = 2.25f + 0.5f * (i & 1); m.uv[i][1] = 0.5f * (i >> 1);   /* (u past 1: moved back by whole tiles) */
        m.jnt[i][0] = 18; m.wgt[i][0] = 0.75f; m.jnt[i][1] = 7; m.wgt[i][1] = 0.25f;   /* the finger: its hand takes it */
    }
    m.ns = 1; m.nidx = 4;
    m.strip = calloc(1, sizeof(ImpStrip)); m.idx = calloc(4, sizeof(int));
    for (int i = 0; i < 4; i++) m.idx[i] = i;
    m.strip[0].n = 4;
    m.ntex = 2; m.tim2[0] = t; m.tim2_len[0] = len; m.tim2[1] = tim2(&m.tim2_len[1]);
    CHECK(imp_tris(&m) == 2);

    char err[160] = "";
    snprintf(p, sizeof(p), "%s/test_import.cvxm", dir);
    CHECK(imp_write_cvxm(&m, p, 0.1f, 0xF00000, err, sizeof(err)) || (printf("%s\n", err), 0));
    FILE* f = fopen(p, "rb");
    CHECK(f);
    static unsigned char d[1 << 20];
    size_t n = fread(d, 1, sizeof(d), f);
    fclose(f);
    CHECK(rd(d) == CVXM_MAGIC && rd(d + 4) == CVXM_VERSION && rd(d + 8) == 60 && rd(d + 20) == 4 && rd(d + 24) == 18);   /* 18 nodes kept */
    CHECK(!memcmp(d + 64, "MDL", 3) && d[67] == 0xbc && rd(d + 60) + 64 + 4 <= rd(d + 12));
    {   /* the texture chunks follow each other (the game's loader steps by 32 + size) up to the end chunk */
        uint32_t q = rd(d + 12), k = 0;
        while (q + 8 <= n && rd(d + q) == 0x324D4954) { CHECK(rd(d + q + 32 + 8) == 0xF00000 + k); q += 32 + rd(d + q + 4); k++; }
        CHECK(k == 2 && rd(d + q) == 0xFFFFFFFF);
    }
    {   /* skin: the finger's weight went to its hand, the largest first, normalised */
        const CvxmSkin* s = (const CvxmSkin*)(d + rd(d + 16));
        CHECK(s[0].w[0] > 0.74f && s[0].w[0] < 0.76f && s[0].w[1] > 0.24f && s[0].w[2] == 0 && s[0].node[0] != s[0].node[1]);
        CHECK(s[0].pos[0] < -2.99f && s[0].pos[0] > -3.01f && s[0].pos[1] > 13.99f && s[0].pos[1] < 14.01f);   /* turned (x negated) and scaled */
    }
    snprintf(p, sizeof(p), "%s/test_import.glb", dir);
    CHECK(imp_write_glb(&m, p, "test", 0.01f, err, sizeof(err)) || (printf("%s\n", err), 0));
    f = fopen(p, "rb");
    CHECK(f);
    n = fread(d, 1, sizeof(d), f);
    fclose(f);
    CHECK(rd(d) == 0x46546C67 && rd(d + 4) == 2 && rd(d + 8) == n && rd(d + 16) == 0x4E4F534A);
    CHECK(strstr((char*)d + 20, "\"skins\"") && strstr((char*)d + 20, "\"inverseBindMatrices\""));
    imp_free(&m);
    printf("import: ok\n");
    return 0;
}
