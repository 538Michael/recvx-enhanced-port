/* VU0 core self-check: macro ops and a microprogram against hand-computed values.
   Build/run: ctest (target test_vu). */
#include "../src/ps2/vu0.h"
#include "../src/ps2/vu0_labels.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* vu.c reports to the profiler; the test runs without it */
int prof_enabled;
void prof_push(int cat) { (void)cat; }
void prof_pop(void) {}

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) <= 1e-4f * (1.0f + fabsf(b)))

void vu1_xgkick(const VuVec* mem, uint32_t qw, uint32_t mask) { (void)mem; (void)qw; (void)mask; }

int main(void)
{
    putenv((char*)"RECVX_VU_CHECK=1");   /* every microprogram call is also interpreted and compared (read at the first call) */
    /* no reset: the power-on state must already have vf0 = (0,0,0,1) (njInitMatrix is the first VU op) */
    /* njInitMatrix: vaddw.xyz vf1, vf0, vf0w ; vmul.w vf1, vf0, vf0 -> (1,1,1,1) */
    vu_upper(U_ADD, VU_XYZ, 1, 0, 0, BC_W);
    vu_upper(U_MUL, VU_W, 1, 0, 0, BC_NONE);
    for (int c = 0; c < 4; c++) CHECK(g_vu0.vf[1].f[c] == 1.0f, "vf1[%d] = %f, want 1", c, g_vu0.vf[1].f[c]);

    /* broadcast + MADD chain: vf18 = M * (1,2,3,1) with M rows vf28..vf31 */
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) g_vu0.vf[28 + r].f[c] = (float)(r * 4 + c);
    g_vu0.vf[4].f[0] = 1; g_vu0.vf[4].f[1] = 2; g_vu0.vf[4].f[2] = 3; g_vu0.vf[4].f[3] = 1;
    vu_upper(U_MULA, VU_XYZW, 0, 28, 4, BC_X);
    vu_upper(U_MADDA, VU_XYZW, 0, 29, 4, BC_Y);
    vu_upper(U_MADDA, VU_XYZW, 0, 30, 4, BC_Z);
    vu_upper(U_MADD, VU_XYZW, 18, 31, 0, BC_W);
    for (int c = 0; c < 4; c++) {
        float want = 1 * (0 + c) + 2 * (4 + c) + 3 * (8 + c) + (12 + c);
        CHECK(NEAR(g_vu0.vf[18].f[c], want), "vf18[%d] = %f, want %f", c, g_vu0.vf[18].f[c], want);
    }

    /* microprogram: VU0_WAVE_INIT sets vf4 = vf5 = (0,-1,0,0) */
    vu0_callms(VU0_ADDR_VU0_WAVE_INIT);
    CHECK(g_vu0.vf[4].f[1] == -1.0f && g_vu0.vf[5].f[1] == -1.0f && g_vu0.vf[4].f[0] == 0.0f, "WAVE_INIT");

    /* CALCPOINT's native path (vu.c calcpoint_fast) against the interpreter: random matrices,
       vertices and leftover state, lights off, directional and point (point: interpreted), chained as a node's
       vertices are; RECVX_VU_CHECK compares the whole VU state after every call */
    {
        extern long vu_check_mismatches(void);
        uint32_t seed = 12345;
#define RND() ((seed = seed * 1103515245u + 12345u) >> 8 & 0xFFFF)
#define RF(lo, hi) ((float)(lo) + ((float)(hi) - (float)(lo)) * (float)RND() / 65535.0f)
        for (int node = 0; node < 200; node++) {
            for (int r = 1; r < 32; r++) for (int c = 0; c < 4; c++) g_vu0.vf[r].f[c] = RF(-50, 50);
            for (int r = 1; r < 16; r++) g_vu0.vi[r] = (uint16_t)RND();
            g_vu0.vf[17].f[0] = RF(100, 400); g_vu0.vf[17].f[1] = RF(100, 400); g_vu0.vf[23].f[2] = RF(0.5, 2);
            g_vu0.vf[31].f[2] = RF(50, 300);   /* view z in front: 1/z */
            g_vu0.q = RF(-1, 1); g_vu0.clip = RND() * 256u;
            g_vu0.mem[67].u[0] = RND() & 0xFF;   /* light mask: lights on/off, point or directional */
            if (node % 7 == 0) g_vu0.vf[12].f[0] = 0.0f;
            vu0_callms(VU0_ADDR_VU0_CALCPOINT_INIT);
            for (int v = 0; v < 6; v++) {
                for (int c = 0; c < 3; c++) { g_vu0.vf[4].f[c] = RF(-200, 200); g_vu0.vf[5].f[c] = RF(-1, 1); }
                g_vu0.vf[4].f[3] = 1.0f;
                if (v == 5) g_vu0.vf[4].f[0] = -0.0f;
                if (node % 5 == 1) g_vu0.vf[4].f[v % 3] *= 1e37f;    /* overflow: clamped to +-FLT_MAX */
                if (node % 5 == 2) g_vu0.vf[5].f[v % 3] *= 1e-37f;   /* denormal products: flushed */
                vu0_callms(VU0_ADDR_VU0_CALCPOINT);
            }
        }
        CHECK(vu_check_mismatches() == 0, "CALCPOINT native path: %ld calls differ from the interpreter", vu_check_mismatches());
#undef RF
#undef RND
    }

    /* the strip clip test (vu_strip_clip) against the asm2c sequence it replaces */
    {
        static uint8_t vbuf[16 * 64] __attribute__((aligned(16)));
        uint16_t idx[16 * 3];
        uint32_t seed = 777;
#define RND() ((seed = seed * 1103515245u + 12345u) >> 8 & 0xFFFF)
        for (int t = 0; t < 500; t++) {
            VuState a, b;
            uint32_t ra, rb = 63;
            int n = 3 + (int)(RND() % 12);
            for (int k = 0; k < 16 * 16; k++) ((float*)vbuf)[k] = (float)((int)RND() - 32768) / 64.0f;
            for (int k = 0; k < n; k++) idx[k * 3] = (uint16_t)(RND() % 16);
            for (int r = 24; r < 28; r++) for (int c = 0; c < 4; c++) g_vu0.vf[r].f[c] = (float)((int)RND() - 32768) / 16384.0f;
            g_vu0.clip = RND();
            a = g_vu0;
            ra = vu_strip_clip(idx, n, vbuf);
            b = g_vu0; g_vu0 = a;
            vu_load_vf(4, vbuf + idx[0] * 64 + 32);
            vu_upper(U_MULA, 15, 0, 24, 4, BC_X); vu_upper(U_MADDA, 15, 0, 25, 4, BC_Y);
            vu_upper(U_MADDA, 15, 0, 26, 4, BC_Z); vu_upper(U_MADD, 15, 5, 27, 0, BC_W);
            for (int k = 1; k < n && rb; k++) {
                vu_load_vf(4, vbuf + idx[k * 3] * 64 + 32);
                vu_clipw(5, 5);
                vu_upper(U_MULA, 15, 0, 24, 4, BC_X); vu_upper(U_MADDA, 15, 0, 25, 4, BC_Y);
                vu_upper(U_MADDA, 15, 0, 26, 4, BC_Z); vu_upper(U_MADD, 15, 5, 27, 0, BC_W);
                rb &= vu_cfc2(18);
                if (k == n - 1) { vu_clipw(5, 5); rb &= vu_cfc2(18); }
            }
            CHECK(ra == rb && !memcmp(&b, &g_vu0, sizeof(b)), "vu_strip_clip differs (test %d: %u vs %u)", t, ra, rb);
        }
#undef RND
    }

    if (fails) { fprintf(stderr, "%d check(s) failed\n", fails); return 1; }
    puts("test_vu: ok");
    return 0;
}
