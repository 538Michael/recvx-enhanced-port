#include "../host/prof.h"
#include "vu0.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define VU_INLINE static inline __attribute__((always_inline))


/* vf0 is the constant (0,0,0,1) from power-on */
VuState g_vu0 = { .vf[0].f[3] = 1.0f, .mem_mask = 0xFF };
VuState g_vu1 = { .vf[0].f[3] = 1.0f, .mem_mask = 0x3FF };

typedef struct {
    uint8_t uop, umask, fd, fs, ft;
    int8_t  bc;
    uint8_t ebit;
    uint8_t lop, lmask, a, b, c;
    int32_t imm;
    uint8_t fsf, ftf;
    float   fimm;
} VuMicroInstr;

enum VuLowerOp {
    L_NOP, L_SQ, L_LQ, L_ILW, L_ISW, L_IADDIU, L_ISUBIU, L_IADDI, L_IADD, L_ISUB, L_IAND, L_IOR, L_MOVE, L_MR32,
    L_MFIR, L_MTIR, L_DIV, L_SQRT, L_RSQRT, L_WAITQ, L_WAITP, L_FMAND, L_FCGET, L_FCOR, L_FCAND, L_FCEQ, L_FCSET,
    L_IBEQ, L_IBNE, L_IBLEZ, L_IBGTZ, L_IBLTZ, L_IBGEZ, L_B, L_BAL, L_JR, L_JALR, L_LOI,
    L_LQI, L_SQI, L_LQD, L_SQD, L_ILWR, L_ISWR, L_XGKICK, L_XTOP, L_XITOP
};

#include "vu0_labels.h"
#include "vu0_microcode.inc"
#include "vu1_microcode.inc"

VU_INLINE void vf0_fix(VuState* s)
{
    s->vf[0].f[0] = 0.0f; s->vf[0].f[1] = 0.0f; s->vf[0].f[2] = 0.0f; s->vf[0].f[3] = 1.0f;
    s->vi[0] = 0;
}

void vu0_reset(void)
{
    memset(&g_vu0, 0, sizeof(g_vu0));
    memset(&g_vu1, 0, sizeof(g_vu1));
    g_vu0.mem_mask = 0xFF;
    g_vu1.mem_mask = 0x3FF;
    vf0_fix(&g_vu0);
    vf0_fix(&g_vu1);
}

VU_INLINE float bcast(const VuVec* vf, float q, float i, int ft, int bc, int comp)
{
    switch (bc) {
    case BC_NONE: return vf[ft].f[comp];
    case BC_Q:    return q;
    case BC_I:    return i;
    default:      return vf[ft].f[bc];
    }
}

VU_INLINE uint32_t mac_bits(float v, int comp)
{
    uint32_t m = 0;
    if (v == 0.0f) m |= 1u << (3 - comp);
    if (signbit(v)) m |= 1u << (7 - comp);
    return m;
}

VU_INLINE uint32_t upper_on(VuState* s, const VuVec* vf, const VuVec* acc, float q, int op, int mask, int fd, int fs, int ft, int bc,
                            int* wrote_clip, uint32_t* clip_bits)
{
    VuVec r;
    uint32_t mac = 0;
    int to_acc = 0, set_flags = 1, c;
    *wrote_clip = 0;

    switch (op) {
    case U_NOP:
        return s->mac;
    case U_CLIP: {
        float w = fabsf(vf[ft].f[3]);
        uint32_t bits = 0;
        if (vf[fs].f[0] > +w) bits |= 0x01;
        if (vf[fs].f[0] < -w) bits |= 0x02;
        if (vf[fs].f[1] > +w) bits |= 0x04;
        if (vf[fs].f[1] < -w) bits |= 0x08;
        if (vf[fs].f[2] > +w) bits |= 0x10;
        if (vf[fs].f[2] < -w) bits |= 0x20;
        *wrote_clip = 1;
        *clip_bits = bits;
        return s->mac;
    }
    case U_OPMULA:
        r.f[0] = vu_clampf(vf[fs].f[1] * vf[ft].f[2]);
        r.f[1] = vu_clampf(vf[fs].f[2] * vf[ft].f[0]);
        r.f[2] = vu_clampf(vf[fs].f[0] * vf[ft].f[1]);
        r.f[3] = acc->f[3];
        mask = VU_XYZ; to_acc = 1;
        break;
    case U_OPMSUB:
        r.f[0] = vu_clampf(acc->f[0] - vf[fs].f[1] * vf[ft].f[2]);
        r.f[1] = vu_clampf(acc->f[1] - vf[fs].f[2] * vf[ft].f[0]);
        r.f[2] = vu_clampf(acc->f[2] - vf[fs].f[0] * vf[ft].f[1]);
        r.f[3] = vf[fd].f[3];
        mask = VU_XYZ;
        break;
    default: {
        /* op dispatch hoisted out of the component loop (hot path of the interpreter) */
        float A[4], B[4];
        for (c = 0; c < 4; c++) { A[c] = vf[fs].f[c]; B[c] = bcast(vf, q, s->i, ft, bc, c); }
#define FOR_C(stmt) for (c = 0; c < 4; c++) { if (!vu_field_on(mask, c)) { r.u[c] = 0; continue; } stmt; }
        switch (op) {
        case U_ADD:  case U_ADDA:  FOR_C(r.f[c] = vu_clampf(A[c] + B[c])) break;
        case U_SUB:  case U_SUBA:  FOR_C(r.f[c] = vu_clampf(A[c] - B[c])) break;
        case U_MUL:  case U_MULA:  FOR_C(r.f[c] = vu_clampf(A[c] * B[c])) break;
        case U_MADD: case U_MADDA: FOR_C(r.f[c] = vu_clampf(acc->f[c] + vu_clampf(A[c] * B[c]))) break;
        case U_MSUB: case U_MSUBA: FOR_C(r.f[c] = vu_clampf(acc->f[c] - vu_clampf(A[c] * B[c]))) break;
        case U_MAX:  FOR_C(r.f[c] = vu_clampf(A[c] > B[c] ? A[c] : B[c])) set_flags = 0; break;
        case U_MINI: FOR_C(r.f[c] = vu_clampf(A[c] < B[c] ? A[c] : B[c])) set_flags = 0; break;
        case U_ABS:  FOR_C(r.f[c] = vu_clampf(fabsf(A[c]))) set_flags = 0; break;
        case U_FTOI0:  FOR_C(r.i[c] = (int32_t)A[c]) set_flags = 0; break;
        case U_FTOI4:  FOR_C(r.i[c] = (int32_t)(A[c] * 16.0f)) set_flags = 0; break;
        case U_FTOI12: FOR_C(r.i[c] = (int32_t)(A[c] * 4096.0f)) set_flags = 0; break;
        case U_FTOI15: FOR_C(r.i[c] = (int32_t)(A[c] * 32768.0f)) set_flags = 0; break;
        case U_ITOF0:  FOR_C(r.f[c] = vu_clampf((float)vf[fs].i[c])) set_flags = 0; break;
        case U_ITOF4:  FOR_C(r.f[c] = vu_clampf((float)vf[fs].i[c] / 16.0f)) set_flags = 0; break;
        case U_ITOF12: FOR_C(r.f[c] = vu_clampf((float)vf[fs].i[c] / 4096.0f)) set_flags = 0; break;
        case U_ITOF15: FOR_C(r.f[c] = vu_clampf((float)vf[fs].i[c] / 32768.0f)) set_flags = 0; break;
        default: fprintf(stderr, "vu0: bad upper op %d\n", op); abort();
        }
#undef FOR_C
        if (set_flags) for (c = 0; c < 4; c++) if (vu_field_on(mask, c)) mac |= mac_bits(r.f[c], c);
        to_acc = (op == U_ADDA || op == U_SUBA || op == U_MULA || op == U_MADDA || op == U_MSUBA);
        break;
    }
    }

    for (c = 0; c < 4; c++) {
        if (!vu_field_on(mask, c)) continue;
        if (to_acc) s->acc.u[c] = r.u[c];
        else if (fd != 0) s->vf[fd].u[c] = r.u[c];
    }
    if (!set_flags) return s->mac;
    return mac;
}

uint32_t vu_upper_on(VuState* s, const VuVec* vf, const VuVec* acc, float q, int op, int mask, int fd, int fs, int ft, int bc,
                     int* wrote_clip, uint32_t* clip_bits)
{
    return upper_on(s, vf, acc, q, op, mask, fd, fs, ft, bc, wrote_clip, clip_bits);
}

/* ---------------- macro mode ---------------- */

/* the strip clip test of njCnkCsUvh/njCnkCsUvn (ps2_NinjaCnk.c, asm2c of the EE asm): per
   vertex, CLIPw of the previous one's transform by the vf24..vf27 matrix, then that
   transform (MULAx/MADDAy/MADDAz/MADDw); the AND of the clip flags. The same macro-mode ops
   (vu_upper, inlined) in one loop instead of an asm2c block per strip; leaves g_vu0 exactly
   as they did. Stops early (0) when the AND is 0. */
uint32_t vu_strip_clip(const uint16_t* idx, int n, const void* vbuf)
{
    const uint8_t* const vb = (const uint8_t*)vbuf;
    uint32_t op0 = 63;
#define XFORM() do { \
        vu_upper(U_MULA, VU_XYZW, 0, 24, 4, BC_X); vu_upper(U_MADDA, VU_XYZW, 0, 25, 4, BC_Y); \
        vu_upper(U_MADDA, VU_XYZW, 0, 26, 4, BC_Z); vu_upper(U_MADD, VU_XYZW, 5, 27, 0, BC_W); } while (0)
    vu_load_vf(4, vb + ((uint32_t)idx[0] << 6) + 32);
    XFORM();
    for (int k = 1; k < n; k++) {
        if (op0 == 0) return 0;
        vu_load_vf(4, vb + ((uint32_t)idx[k * 3] << 6) + 32);
        vu_clipw(5, 5);
        XFORM();
        op0 &= g_vu0.clip;
    }
    vu_clipw(5, 5);
    return op0 & g_vu0.clip;
#undef XFORM
}

void vu_move(int mask, int ft, int fs)
{
    VuVec v = g_vu0.vf[fs];
    for (int c = 0; c < 4; c++) if (vu_field_on(mask, c) && ft) g_vu0.vf[ft].u[c] = v.u[c];
}

void vu_mr32(int mask, int ft, int fs)
{
    VuVec v = g_vu0.vf[fs], r;
    r.u[0] = v.u[1]; r.u[1] = v.u[2]; r.u[2] = v.u[3]; r.u[3] = v.u[0];
    for (int c = 0; c < 4; c++) if (vu_field_on(mask, c) && ft) g_vu0.vf[ft].u[c] = r.u[c];
}

static float vu_divide(float a, float b)
{
    if (b == 0.0f) return (signbit(a) != signbit(b)) ? -FLT_MAX : FLT_MAX;
    return vu_clampf(a / b);
}

void vu_div(int fs, int fsf, int ft, int ftf)   { g_vu0.q = vu_divide(g_vu0.vf[fs].f[fsf], g_vu0.vf[ft].f[ftf]); }
void vu_sqrt(int ft, int ftf)                   { g_vu0.q = vu_clampf(sqrtf(fabsf(g_vu0.vf[ft].f[ftf]))); }
void vu_rsqrt(int fs, int fsf, int ft, int ftf) { g_vu0.q = vu_divide(g_vu0.vf[fs].f[fsf], sqrtf(fabsf(g_vu0.vf[ft].f[ftf]))); }

void vu_mtir(int it, int fs, int fsf) { if (it) g_vu0.vi[it] = (uint16_t)g_vu0.vf[fs].u[fsf]; }

void vu_mfir(int mask, int ft, int is)
{
    int32_t v = (int16_t)g_vu0.vi[is];
    for (int c = 0; c < 4; c++) if (vu_field_on(mask, c) && ft) g_vu0.vf[ft].i[c] = v;
}

uint32_t vu_cfc2(int reg)
{
    if (reg < 16) return g_vu0.vi[reg];
    switch (reg) {
    case 16: return g_vu0.status;
    case 17: return g_vu0.mac;
    case 18: return g_vu0.clip;
    case 21: { union { float f; uint32_t u; } c = { g_vu0.i }; return c.u; }
    case 22: { union { float f; uint32_t u; } c = { g_vu0.q }; return c.u; }
    case 27: return g_vu0.cmsar0;
    case 29: return 0; /* VPU-STAT: VU0 idle */
    }
    fprintf(stderr, "vu0: cfc2 of unsupported reg %d\n", reg);
    return 0;
}

void vu_ctc2(int reg, uint32_t v)
{
    if (reg < 16) { if (reg) g_vu0.vi[reg] = (uint16_t)v; return; }
    switch (reg) {
    case 16: g_vu0.status = v; return;
    case 18: g_vu0.clip = v & 0xFFFFFF; return;
    case 21: { union { uint32_t u; float f; } c = { v }; g_vu0.i = c.f; return; }
    case 22: { union { uint32_t u; float f; } c = { v }; g_vu0.q = c.f; return; }
    case 27: g_vu0.cmsar0 = v; return;
    case 28: return; /* FBRST */
    }
    fprintf(stderr, "vu0: ctc2 of unsupported reg %d\n", reg);
}

/* ---------------- micro mode ---------------- */
/*
 * Timing model (what the hand-scheduled microcode depends on):
 *  - FMAC results: readers stall until issue+4; values are committed at issue and
 *    stalls only shift the cycle counter (sequential semantics + correct timing).
 *  - MAC/clip flags become visible to FMAND/FCxxx at issue+4.
 *  - Q: DIV/SQRT latency 7, RSQRT 13; Q readers see the newest finished result,
 *    a new divide (or WAITQ) stalls until the previous one has finished.
 *  - Both halves of a pair read pre-pair register values; branches have one delay
 *    slot; [E] ends the program after the following pair.
 */
#define FMAC_LATENCY 4
#define FLAG_HISTORY 16

typedef struct { int64_t visible; uint32_t value; } FlagEntry;

typedef struct {
    int64_t   cyc;
    int64_t   vf_ready[32][4];
    FlagEntry mac[FLAG_HISTORY];   int mac_n;
    FlagEntry clip[FLAG_HISTORY];  int clip_n;
    uint32_t  clip_latest;
    int64_t   q_ready;
    float     q_pending;
} MicroPipe;

/* ring of the last FLAG_HISTORY flag writes; *n counts all pushes */
VU_INLINE void flag_push(FlagEntry* f, int* n, int64_t vis, uint32_t v)
{
    FlagEntry* e = &f[*n & (FLAG_HISTORY - 1)];
    e->visible = vis; e->value = v; (*n)++;
}

VU_INLINE uint32_t flag_read(const FlagEntry* f, int n, int64_t cyc, uint32_t dflt)
{
    int lo = n > FLAG_HISTORY ? n - FLAG_HISTORY : 0;
    for (int k = n - 1; k >= lo; k--) if (f[k & (FLAG_HISTORY - 1)].visible <= cyc) return f[k & (FLAG_HISTORY - 1)].value;
    return dflt;
}

VU_INLINE void need_vf(MicroPipe* p, int r, int mask)
{
    if (r == 0) return;
    for (int c = 0; c < 4; c++)
        if (vu_field_on(mask, c) && p->vf_ready[r][c] > p->cyc) p->cyc = p->vf_ready[r][c];
}

VU_INLINE void wrote_vf(MicroPipe* p, int r, int mask, int lat)
{
    for (int c = 0; c < 4; c++) if (vu_field_on(mask, c)) p->vf_ready[r][c] = p->cyc + lat;
}

VU_INLINE float q_now(MicroPipe* p, VuState* s)
{
    if (p->q_ready && p->q_ready <= p->cyc) { s->q = p->q_pending; p->q_ready = 0; }
    return s->q;
}

VU_INLINE void write_fields(VuVec* dst, const VuVec* src, int mask)
{
    for (int c = 0; c < 4; c++) if (vu_field_on(mask, c)) dst->u[c] = src->u[c];
}

typedef struct {
    MicroPipe P;
    int branch_to, ending;
    uint32_t mac_seed, mm;
    int no_kick;          /* reference run of RECVX_VU_CHECK: no GS output */
} MicroRun;

static void run_begin(MicroRun* R, VuState* s)
{
    memset(&R->P, 0, sizeof(R->P));
    R->branch_to = -1; R->ending = 0; R->no_kick = 0;
    R->mac_seed = s->mac; R->mm = s->mem_mask;
    R->P.clip_latest = s->clip;
    flag_push(R->P.mac, &R->P.mac_n, -1000, R->mac_seed);
    flag_push(R->P.clip, &R->P.clip_n, -1000, s->clip);
}

static void run_end(MicroRun* R, VuState* s)
{
    R->P.cyc += 64;
    q_now(&R->P, s);
    s->mac = flag_read(R->P.mac, R->P.mac_n, R->P.cyc, R->mac_seed);
    s->clip = R->P.clip_latest;
}

/* One instruction pair; returns the next pc, or -1 at the end of the program. Always
   inlined: called with a constant instruction (vu_static.inc), the compiler folds every
   field, which makes the per-address switch a static recompilation of the microcode with
   exactly the interpreter's semantics. */
VU_INLINE int vu_step(VuState* s, MicroRun* R, const VuMicroInstr* in, int pc, int unit)
{
    MicroPipe* const pp = &R->P;
    const uint32_t mm = R->mm;
    VuVec la, lb, lc;          /* lower-op VF sources, read before the upper op writes */
    int next_pc, take = -1;
    (void)unit;

    /* ---- stalls ---- */
    if (in->uop != U_NOP) {
        int m = in->umask, ftm = m;
        if (in->uop == U_CLIP) { m = VU_XYZ; ftm = VU_W; }
        else if (in->bc >= BC_X && in->bc <= BC_W) ftm = 8 >> in->bc;
        need_vf(pp, in->fs, m);
        if (in->bc != BC_Q && in->bc != BC_I && in->uop != U_ABS && in->uop < U_FTOI0) need_vf(pp, in->ft, ftm);
    }
    switch (in->lop) {
    case L_SQ: case L_SQI: case L_SQD: need_vf(pp, in->a, in->lmask); break;
    case L_MOVE: case L_MR32: need_vf(pp, in->b, 15); break;
    case L_MTIR: need_vf(pp, in->b, 8 >> in->fsf); break;
    case L_DIV: case L_RSQRT:
        need_vf(pp, in->b, 8 >> in->fsf); need_vf(pp, in->c, 8 >> in->ftf);
        if (pp->q_ready > pp->cyc) pp->cyc = pp->q_ready;
        break;
    case L_SQRT:
        need_vf(pp, in->c, 8 >> in->ftf);
        if (pp->q_ready > pp->cyc) pp->cyc = pp->q_ready;
        break;
    case L_WAITQ:
        if (pp->q_ready > pp->cyc) pp->cyc = pp->q_ready;
        break;
    }

    /* only for lower ops that read VF registers (the upper op writes first) */
    switch (in->lop) {
    case L_SQ: case L_SQI: case L_SQD: la = s->vf[in->a & 31]; break;
    case L_MOVE: case L_MR32: case L_MTIR: lb = s->vf[in->b & 31]; break;
    case L_DIV: case L_RSQRT: lb = s->vf[in->b & 31]; lc = s->vf[in->c & 31]; break;
    case L_SQRT: lc = s->vf[in->c & 31]; break;
    default: break;
    }

    /* ---- upper ---- */
    if (in->uop != U_NOP) {
        int wc; uint32_t bits = 0;
        int fd = in->fd == 0xFF ? 0 : in->fd;
        float q = q_now(pp, s);
        uint32_t m = upper_on(s, s->vf, &s->acc, q, in->uop, in->umask, fd, in->fs, in->ft, in->bc, &wc, &bits);
        if (wc) {
            pp->clip_latest = ((pp->clip_latest << 6) | bits) & 0xFFFFFF;
            flag_push(pp->clip, &pp->clip_n, pp->cyc + FMAC_LATENCY, pp->clip_latest);
        } else if (in->uop != U_MAX && in->uop != U_MINI && in->uop != U_ABS && in->uop < U_FTOI0) {
            flag_push(pp->mac, &pp->mac_n, pp->cyc + FMAC_LATENCY, m);
        }
        if (in->fd != 0xFF && in->uop != U_CLIP) wrote_vf(pp, fd, in->umask, FMAC_LATENCY);
    }

    /* ---- lower ---- */
    next_pc = pc + 1;
    switch (in->lop) {
    case L_NOP: break;
    case L_LQ:
        if (in->a) write_fields(&s->vf[in->a], &s->mem[(s->vi[in->b] + in->imm) & mm], in->lmask);
        wrote_vf(pp, in->a, in->lmask, FMAC_LATENCY);
        break;
    case L_LQI:
        if (in->a) write_fields(&s->vf[in->a], &s->mem[s->vi[in->b] & mm], in->lmask);
        wrote_vf(pp, in->a, in->lmask, FMAC_LATENCY);
        if (in->b) s->vi[in->b]++;
        break;
    case L_LQD:
        if (in->b) s->vi[in->b]--;
        if (in->a) write_fields(&s->vf[in->a], &s->mem[s->vi[in->b] & mm], in->lmask);
        wrote_vf(pp, in->a, in->lmask, FMAC_LATENCY);
        break;
    case L_SQ:  write_fields(&s->mem[(s->vi[in->b] + in->imm) & mm], &la, in->lmask); break;
    case L_SQI: write_fields(&s->mem[s->vi[in->b] & mm], &la, in->lmask); if (in->b) s->vi[in->b]++; break;
    case L_SQD: if (in->b) s->vi[in->b]--; write_fields(&s->mem[s->vi[in->b] & mm], &la, in->lmask); break;
    case L_ILW: case L_ILWR: {
        VuVec* m = &s->mem[(s->vi[in->b] + (in->lop == L_ILW ? in->imm : 0)) & mm];
        for (int c = 0; c < 4; c++) if (vu_field_on(in->lmask, c)) { if (in->a) s->vi[in->a] = (uint16_t)m->u[c]; break; }
        break;
    }
    case L_ISW: case L_ISWR: {
        VuVec* m = &s->mem[(s->vi[in->b] + (in->lop == L_ISW ? in->imm : 0)) & mm];
        for (int c = 0; c < 4; c++) if (vu_field_on(in->lmask, c)) m->u[c] = s->vi[in->a];
        break;
    }
    case L_IADDIU: if (in->a) s->vi[in->a] = (uint16_t)(s->vi[in->b] + in->imm); break;
    case L_ISUBIU: if (in->a) s->vi[in->a] = (uint16_t)(s->vi[in->b] - in->imm); break;
    case L_IADDI:  if (in->a) s->vi[in->a] = (uint16_t)(s->vi[in->b] + in->imm); break;
    case L_IADD:   if (in->a) s->vi[in->a] = (uint16_t)(s->vi[in->b] + s->vi[in->c]); break;
    case L_ISUB:   if (in->a) s->vi[in->a] = (uint16_t)(s->vi[in->b] - s->vi[in->c]); break;
    case L_IAND:   if (in->a) s->vi[in->a] = s->vi[in->b] & s->vi[in->c]; break;
    case L_IOR:    if (in->a) s->vi[in->a] = s->vi[in->b] | s->vi[in->c]; break;
    case L_MOVE:
        if (in->a) write_fields(&s->vf[in->a], &lb, in->lmask);
        wrote_vf(pp, in->a, in->lmask, FMAC_LATENCY);
        break;
    case L_MR32: {
        VuVec r; r.u[0] = lb.u[1]; r.u[1] = lb.u[2]; r.u[2] = lb.u[3]; r.u[3] = lb.u[0];
        if (in->a) write_fields(&s->vf[in->a], &r, in->lmask);
        wrote_vf(pp, in->a, in->lmask, FMAC_LATENCY);
        break;
    }
    case L_MFIR: {
        VuVec r; for (int c = 0; c < 4; c++) r.i[c] = (int16_t)s->vi[in->b];
        if (in->a) write_fields(&s->vf[in->a], &r, in->lmask);
        wrote_vf(pp, in->a, in->lmask, FMAC_LATENCY);
        break;
    }
    case L_MTIR: if (in->a) s->vi[in->a] = (uint16_t)lb.u[in->fsf]; break;
    case L_DIV:
        q_now(pp, s);
        pp->q_pending = vu_divide(lb.f[in->fsf], lc.f[in->ftf]);
        pp->q_ready = pp->cyc + 7;
        break;
    case L_SQRT:
        q_now(pp, s);
        pp->q_pending = vu_clampf(sqrtf(fabsf(lc.f[in->ftf])));
        pp->q_ready = pp->cyc + 7;
        break;
    case L_RSQRT:
        q_now(pp, s);
        pp->q_pending = vu_divide(lb.f[in->fsf], sqrtf(fabsf(lc.f[in->ftf])));
        pp->q_ready = pp->cyc + 13;
        break;
    case L_WAITQ: q_now(pp, s); break;
    case L_WAITP: break;
    case L_FMAND: if (in->a) s->vi[in->a] = (uint16_t)(flag_read(pp->mac, pp->mac_n, pp->cyc, R->mac_seed) & s->vi[in->b]); break;
    case L_FCGET: if (in->a) s->vi[in->a] = (uint16_t)(flag_read(pp->clip, pp->clip_n, pp->cyc, 0) & 0xFFF); break;
    case L_FCOR:
        if (in->a) s->vi[in->a] = ((flag_read(pp->clip, pp->clip_n, pp->cyc, 0) | (uint32_t)in->imm) & 0xFFFFFF) == 0xFFFFFF;
        break;
    case L_FCAND: if (in->a) s->vi[in->a] = (flag_read(pp->clip, pp->clip_n, pp->cyc, 0) & (uint32_t)in->imm) != 0; break;
    case L_FCEQ:  if (in->a) s->vi[in->a] = (flag_read(pp->clip, pp->clip_n, pp->cyc, 0) & 0xFFFFFF) == (uint32_t)in->imm; break;
    case L_FCSET:
        pp->clip_latest = (uint32_t)in->imm & 0xFFFFFF;
        flag_push(pp->clip, &pp->clip_n, pp->cyc + FMAC_LATENCY, pp->clip_latest);
        break;
    case L_IBEQ:  if (s->vi[in->a] == s->vi[in->b]) take = in->imm; break;
    case L_IBNE:  if (s->vi[in->a] != s->vi[in->b]) take = in->imm; break;
    case L_IBLEZ: if ((int16_t)s->vi[in->a] <= 0) take = in->imm; break;
    case L_IBGTZ: if ((int16_t)s->vi[in->a] > 0) take = in->imm; break;
    case L_IBLTZ: if ((int16_t)s->vi[in->a] < 0) take = in->imm; break;
    case L_IBGEZ: if ((int16_t)s->vi[in->a] >= 0) take = in->imm; break;
    case L_B:     take = in->imm; break;
    case L_BAL:   take = in->imm; if (in->a) s->vi[in->a] = (uint16_t)(pc + 2); break;
    case L_JR:    take = s->vi[in->b]; break;
    case L_JALR:  take = s->vi[in->b]; if (in->a) s->vi[in->a] = (uint16_t)(pc + 2); break;
    case L_LOI:   s->i = in->fimm; break;
    case L_XTOP:  if (in->a) s->vi[in->a] = (uint16_t)s->top; break;
    case L_XITOP: if (in->a) s->vi[in->a] = (uint16_t)s->itop; break;
    case L_XGKICK: if (!R->no_kick) vu1_xgkick(s->mem, s->vi[in->b], mm); break;
    default: fprintf(stderr, "vu%d: bad lower op %d at %d\n", unit, in->lop, pc); abort();
    }
    vf0_fix(s);
    pp->cyc++;

    if (R->branch_to >= 0) { next_pc = R->branch_to; R->branch_to = -1; }
    if (take >= 0) R->branch_to = take;
    if (R->ending) return -1;
    if (in->ebit) R->ending = 1;
    return next_pc;
}

static void vu_micro_run_k(VuState* s, const VuMicroInstr* code, int count, int addr, int unit, int no_kick)
{
    MicroRun R;
    int pc = addr;
    long steps = 0;
    run_begin(&R, s);
    R.no_kick = no_kick;
    while (pc >= 0) {
        if (pc >= count || ++steps > 4000000) {
            fprintf(stderr, "vu%d: microprogram ran away (pc=%d, start=%d)\n", unit, pc, addr);
            abort();
        }
        pc = vu_step(s, &R, &code[pc], pc, unit);
    }
    run_end(&R, s);
}

static void vu_micro_run(VuState* s, const VuMicroInstr* code, int count, int addr, int unit)
{
    vu_micro_run_k(s, code, count, addr, unit, 0);
}

/* the statically recompiled programs (one case per micro address, see vu_step) */
#define VU_STATIC(name, code, cases, unit)                                                          \
    static void name(VuState* s, int addr)                                                         \
    {                                                                                              \
        MicroRun R;                                                                                \
        int pc = addr;                                                                             \
        long steps = 0;                                                                            \
        run_begin(&R, s);                                                                          \
    dispatch:                                                                                      \
        if (pc < 0) { run_end(&R, s); return; }                                                    \
        if (++steps > 4000000) { fprintf(stderr, "vu%d: microprogram ran away (pc=%d)\n", unit, pc); abort(); } \
        switch (pc) {                                                                              \
            cases(code, unit)                                                           \
        default: fprintf(stderr, "vu%d: bad pc %d\n", unit, pc); abort();                           \
        }                                                                                          \
    }
/* VU_CASE(n): run address n; fall through to n + 1 unless control goes elsewhere */
#define VU_CASE(code, unit, n) case n: { int nx_ = vu_step(s, &R, &code[n], n, unit); if (nx_ != n + 1) { pc = nx_; goto dispatch; } }
#include "vu_static.inc"
VU_STATIC(vu0_static_run, vu0_micro_code, VU0_CASES, 0)

#define LD(r) v4ld(&s->vf[r])
#define LM(a) v4ld(&s->mem[a])
#define ST(r, v) v4st(&s->vf[r], v)

/* VU0_CALCPOINT (vsm/ps2_vu0.vsm): one vertex of a chunk model, called once per vertex of
   every node drawn (205k a frame in a whole-village room, ~3.7k in a retail room). The same
   operations in program order with the interpreter's arithmetic (upper_on: products and sums
   clamped per component, here four lanes at a time), and its timing resolved for every path
   (the program has no loops; stalls only delay, they never change a value):
   - Q: MULq.xyz vf06 reads the DIV of vf18z (ready exactly then); in a point light's path
     (LIGHTk_POINT) the MULq in the branch delay slot reads that light's RSQRT (13 cycles
     after it, ready) and the MULq after the six NOPs the DIV issued in that slot; Q at the
     end is the last DIV/RSQRT issued, so the other lights' RSQRTs are not computed.
   - MAC flags: each FMAND reads those of the FMAC 4 cycles before it; at the end the ADDz.x's.
   - CLIP flags: FCGET reads the CLIPw's (4 cycles before it).
   Leaves the whole VU state as the interpreter does (RECVX_VU_CHECK=1 compares every call;
   pc/tests/test_vu.c). */
static void calcpoint_fast(VuState* s)
{
    /* registers the program only reads are read where used (x86-32 has 8 vector registers) */
#define r0 LD(0)
#define r12 LD(12)
#define r13 LD(13)
#define r17 LD(17)
#define r24 LD(24)
#define r25 LD(25)
#define r26 LD(26)
#define r27 LD(27)
    V4 r4 = LD(4), r5 = LD(5), r6 = LD(6), r7 = LD(7), r8 = LD(8), r9 = LD(9), r10 = LD(10), r11 = LD(11);
    V4 r18, r19 = LD(19), r28 = LD(28), r29 = LD(29), r30 = LD(30), r31 = LD(31), acc;
    uint16_t vi1, vi12, vi13, vi14 = s->vi[10], vi15 = s->vi[15];
    uint32_t cb;
    float q;
    int q_rsqrt = 3;   /* the last Q operation: light k's RSQRT, or -1: q (a DIV) */
    acc = v4mul(r28, V4BC(r4, 0));                             /* MULAx.xyzw ACC, vf28, vf04x */
    r6 = v4sel(r6, LD(1), VU_Z);                               /*  | MOVE.z vf06, vf01 */
    acc = v4madd(acc, r29, V4BC(r4, 1));
    acc = v4madd(acc, r30, V4BC(r4, 2));
    r18 = v4madd(acc, r31, V4BC(r0, 3));                       /* vf18: view position */
    acc = v4sel(acc, v4mul(r28, V4BC(r5, 0)), VU_XYZ);
    acc = v4sel(acc, v4madd(acc, r29, V4BC(r5, 1)), VU_XYZ);
    r19 = v4sel(r19, v4madd(acc, r30, V4BC(r5, 2)), VU_XYZ);   /* vf19: normal */
    q = vu_divide(v4lane(r0, 3), v4lane(r18, 2));             /*  | DIV Q, vf00w, vf18z */
    r6 = v4sel(r6, v4mul(r18, r17), VU_X | VU_Y);
    r8 = v4sub(r8, r8);
    r9 = v4sub(r24, V4BC(r18, 0));  r28 = LM(56);
    r10 = v4sub(r25, V4BC(r18, 1)); r29 = LM(57);
    r11 = v4sub(r26, V4BC(r18, 2)); r30 = LM(58);
    r31 = LM(59);
    r6 = v4sel(r6, v4mul(r0, V4BC(r19, 2)), VU_W);
    r6 = v4sel(r6, v4mul(r6, v4set1(q)), VU_XYZ);              /* MULq.xyz: the DIV is done */
    acc = v4mul(r9, r9);
    acc = v4madd(acc, r10, r10);
    r5 = v4madd(acc, r11, r11);                                /* vf05: squared distances */
    acc = v4mul(r9, V4BC(r19, 0));
    acc = v4madd(acc, r10, V4BC(r19, 1));
    r4 = v4madd(acc, r11, V4BC(r19, 2));                       /* vf04: (light - point) . normal */
    vi13 = (uint16_t)(v4mac(r4, VU_XYZW) & s->vi[9]);           /* FMAND x3, IOR */
    vi12 = (uint16_t)(v4mac(v4sub(r12, r5), VU_XYZW) & s->vi[9]);   /* SUB.xyzw vf00, vf12, vf05 */
    vi1 = (uint16_t)(v4mac(v4sub(r13, r5), VU_XYZW) & s->vi[9]);
    vi13 |= vi1;
#define RSQRT(k) vu_divide(v4lane(r0, 3), sqrtf(fabsf(v4lane(r5, k))))   /* RSQRT Q, vf00w, vf05[k] */
    /* The lights' writes of ACC and vf10.xyz are dead (overwritten at LIGHT4 before any read), so
       each light is computed on its own values and only its vf08 lane is kept: no dependency
       from one light to the next. */
#define LIGHT(k) do { /* LIGHTk: header, then LIGHTk_PARA (directional) or LIGHTk_POINT */          \
        vi1 = vi14 & s->vi[7];                                                                     \
        vi15 = vi14 & s->vi[8];                                                                    \
        vi14 = (uint16_t)(vi14 + vi14);                                                            \
        q_rsqrt = k;                                                                               \
        if (vi1 && !vi15) {          /* vf08[k] = max(L . N, 0) */                                \
            const V4 l = v4mul(r19, V4BC(r27, k));   /* MULk.xyz vf10, vf19, vf27[k] */            \
            V4 t = v4madd(v4madd(v4mul(r24, V4BC(l, 0)), r25, V4BC(l, 1)), r26, V4BC(l, 2));       \
            r8 = v4sel(r8, v4max(t, V4BC(r0, 0)), VU_X >> k);                                      \
        } else if (vi1) {            /* point: facing and in range */                              \
            vi1 = vi13 & s->vi[7 - k];                                                             \
            vi15 = vi12 & s->vi[7 - k];                                                            \
            if (!vi1) {                                                                            \
                V4 t = v4mul(r4, v4set1(RSQRT(k)));  /* MULq | DIV */                              \
                q = vu_divide(v4lane(r12, k), v4lane(r5, k));                                     \
                q_rsqrt = -1;                                                                      \
                if (vi15) t = v4mul(t, v4set1(q));                                                 \
                r8 = v4sel(r8, t, VU_X >> k);                                                      \
            }                                                                                      \
        } } while (0)
    LIGHT(0); LIGHT(1); LIGHT(2); LIGHT(3);
#undef LIGHT
    if (q_rsqrt == 3) q = RSQRT(3);
#undef RSQRT
    acc = v4mul(r28, V4BC(r18, 0));           r7 = v4sel(r7, LM(71), VU_XYZ);
    acc = v4madd(acc, r29, V4BC(r18, 1));     r9 = v4sel(r9, LM(72), VU_XYZ);
    acc = v4madd(acc, r30, V4BC(r18, 2));     r10 = v4sel(r10, LM(73), VU_XYZ);
    r5 = v4madd(acc, r31, V4BC(r0, 3));       r11 = v4sel(r11, LM(74), VU_XYZ);   /* clip-space position */
    acc = v4sel(acc, v4mul(r7, V4BC(r8, 0)), VU_XYZ);          r28 = LM(60);
    acc = v4sel(acc, v4madd(acc, r9, V4BC(r8, 1)), VU_XYZ);    r29 = LM(61);
    acc = v4sel(acc, v4madd(acc, r10, V4BC(r8, 2)), VU_XYZ);   r30 = LM(62);
    {   /* CLIPw.xyz vf05, vf05w */
        float w = fabsf(v4lane(r5, 3)), x = v4lane(r5, 0), y = v4lane(r5, 1), z = v4lane(r5, 2);
        cb = (uint32_t)((x > w) | (x < -w) << 1 | (y > w) << 2 | (y < -w) << 3 | (z > w) << 4 | (z < -w) << 5);
    }                                                          r31 = LM(63);
    r7 = v4sel(r7, v4madd(acc, r11, V4BC(r8, 3)), VU_XYZ);     /* colour */
    r11 = v4sel(r11, v4add(r0, V4BC(r6, 2)), VU_X);            /* ADDz.x vf11, vf00, vf06z */
    s->mac = v4mac(r11, VU_X);
    s->clip = ((s->clip << 6) | cb) & 0xFFFFFF;
    s->vi[1] = (uint16_t)(s->clip & 0xFFF);                    /* FCGET */
    s->vi[11] &= s->vi[1];
    s->vi[12] = vi12; s->vi[13] = vi13; s->vi[14] = vi14; s->vi[15] = vi15;
    s->q = q;
    ST(4, r4);   ST(5, r5);   ST(6, r6);   ST(7, r7);   ST(8, r8);   ST(9, r9);   ST(10, r10); ST(11, r11);
    ST(18, r18); ST(19, r19); ST(28, r28); ST(29, r29); ST(30, r30); ST(31, r31);
#undef r0
#undef r12
#undef r13
#undef r17
#undef r24
#undef r25
#undef r26
#undef r27
    v4st(&s->acc, acc);
}
#undef LD
#undef LM
#undef ST
#undef V4BC
VU_STATIC(vu1_static_run, vu1_micro_code, VU1_CASES, 1)

/* RECVX_VU_INTERP=1: run the microcode through the interpreter instead (reference) */
static int use_interp(void)
{
    static int v = -1;
    if (v < 0) v = getenv("RECVX_VU_INTERP") && atoi(getenv("RECVX_VU_INTERP"));
    return v;
}

/* RECVX_VU_CHECK=1: run every program both ways and compare the whole VU state */
static VuState vu_ref;
static int vu_check(void)
{
    static int v = -1;
    if (v < 0) v = getenv("RECVX_VU_CHECK") != NULL;
    return v;
}
static long check_calls, check_bad;
long vu_check_mismatches(void) { return check_bad; }   /* test_vu */
static void check_against(const VuState* s, int unit, int addr)
{
    long* const calls = &check_calls;
    long* const bad_p = &check_bad;
#define calls (*calls)
#define bad (*bad_p)
    calls++;
    if (memcmp(s, &vu_ref, sizeof(*s))) {
        if (bad++ < 20) {
            fprintf(stderr, "vu%d static/interp mismatch at program %d (call %ld)\n", unit, addr, calls);
            for (int r = 0; r < 32; r++) if (memcmp(&s->vf[r], &vu_ref.vf[r], 16)) fprintf(stderr, "  vf%d\n", r);
            for (int r = 0; r < 16; r++) if (s->vi[r] != vu_ref.vi[r]) fprintf(stderr, "  vi%d %d vs %d\n", r, s->vi[r], vu_ref.vi[r]);
            if (memcmp(s->mem, vu_ref.mem, sizeof(s->mem))) fprintf(stderr, "  mem\n");
            if (s->q != vu_ref.q || s->mac != vu_ref.mac || s->clip != vu_ref.clip) fprintf(stderr, "  q/mac/clip\n");
        }
    }
    if (calls % 100000 == 0) fprintf(stderr, "vu check: %ld calls, %ld mismatches\n", calls, bad);
#undef calls
#undef bad
}

static void dump_vf(const char* tag, int r) { fprintf(stderr, "  %s vf%-2d = %12.5f %12.5f %12.5f %12.5f\n", tag, r, g_vu0.vf[r].f[0], g_vu0.vf[r].f[1], g_vu0.vf[r].f[2], g_vu0.vf[r].f[3]); }

void vu0_callms(int addr)
{
    static int dbg = -1, ndump;
    if (dbg < 0) dbg = getenv("RECVX_DUMP_CALCPOINT") ? atoi(getenv("RECVX_DUMP_CALCPOINT")) : 0;
    if (dbg && addr == VU0_ADDR_VU0_CALCPOINT && ++ndump == dbg) {
        static const int regs[] = { 0, 1, 4, 5, 12, 13, 17, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
        fprintf(stderr, "CALCPOINT call %d, inputs:\n", ndump);
        for (unsigned i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) dump_vf("in ", regs[i]);
        g_vu0.cmsar0 = (uint32_t)addr;
        vu_micro_run(&g_vu0, vu0_micro_code, VU0_MICRO_COUNT, addr, 0);
        fprintf(stderr, "outputs:\n");
        { static const int outs[] = { 4, 5, 6, 7, 8, 11, 18, 19 }; for (unsigned i = 0; i < sizeof(outs) / sizeof(outs[0]); i++) dump_vf("out", outs[i]); }
        fprintf(stderr, "  Q = %f\n", g_vu0.q);
        return;
    }
    g_vu0.cmsar0 = (uint32_t)addr;
    if (!g_vu0.mem_mask) g_vu0.mem_mask = 0xFF;
    /* ~100 ns: not timed (the clock reads cost as much), so it counts as game logic */
    if (addr == VU0_ADDR_VU0_CALCPOINT && !vu_check() && !use_interp()) { calcpoint_fast(&g_vu0); return; }
    prof_push(PROF_VU0);   /* (the time: --profile) */
    if (vu_check()) { vu_ref = g_vu0; vu_micro_run_k(&vu_ref, vu0_micro_code, VU0_MICRO_COUNT, addr, 0, 1); }
    if (use_interp()) vu_micro_run(&g_vu0, vu0_micro_code, VU0_MICRO_COUNT, addr, 0);
    else if (addr == VU0_ADDR_VU0_CALCPOINT) calcpoint_fast(&g_vu0);
    else vu0_static_run(&g_vu0, addr);
    if (vu_check()) check_against(&g_vu0, 0, addr);
    prof_pop();
}


void vu1_callms(int addr)
{
    if (!g_vu1.mem_mask) { g_vu1.mem_mask = 0x3FF; vf0_fix(&g_vu1); }
    prof_push(PROF_VU1);
    if (vu_check()) { vu_ref = g_vu1; vu_micro_run_k(&vu_ref, vu1_micro_code, VU1_MICRO_COUNT, addr, 1, 1); }
    if (use_interp()) vu_micro_run(&g_vu1, vu1_micro_code, VU1_MICRO_COUNT, addr, 1);
    else vu1_static_run(&g_vu1, addr);
    if (vu_check()) check_against(&g_vu1, 1, addr);
    prof_pop();
}
