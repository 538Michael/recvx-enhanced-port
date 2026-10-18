/* Pure decisions of the PC hooks, kept apart so pc/tests/test_rules.c can check them without the game.
   Needs the decomp's types (types.h) before it. */
#pragma once
#include "../src/gs/native.h"
#include <math.h>

/* ---------------------------------------------------------------- added enemies (decomp eneset.c) */
typedef void (*PcEnemyProc)(BH_PWORK*);

/* a part's or child's parent enemy: up to 3 links, each into the enemy array ene0[n] */
static inline BH_PWORK* pc_root_in(BH_PWORK* ep, BH_PWORK* ene0, int n)
{
    int k;
    for (k = 0; k < 3 && ep->lkwkp && (BH_PWORK*)ep->lkwkp >= ene0 && (BH_PWORK*)ep->lkwkp < ene0 + n; k++) ep = (BH_PWORK*)ep->lkwkp;
    return ep;
}

/* ep is the holder `by`'s family: its part or child, or the leader of by's type (an unlinked enemy of
   the same id the type made itself, owner: slot -> creator type + 1) */
static inline int pc_family_in(BH_PWORK* ep, BH_PWORK* by, BH_PWORK* ene0, int n, const unsigned char* owner)
{
    BH_PWORK* r = pc_root_in(by, ene0, n);
    return pc_root_in(ep, ene0, n) == r || (!ep->lkwkp && ep >= ene0 && ep < ene0 + n && ep->id == r->id && owner[ep - ene0] == r->id + 1);
}

/* the function a step of enemy id runs in a room with added enemies: a part or child (ids 31..38; o:
   its creator type + 1, 0 none) made before the room got them (retail) the shared one as it was then
   (snap), another its creator type's own (helper: that type's functions), else f */
static inline PcEnemyProc pc_enemy_proc(PcEnemyProc f, int id, int o, int retail, const PcEnemyProc* snap, const PcEnemyProc* helper)
{
    if (id < 31 || id > 38 || !o) return f;
    if (retail) return snap[id - 31] ? snap[id - 31] : f;
    return helper[id - 31] ? helper[id - 31] : f;
}

/* ---------------------------------------------------------------- dual weapons (tps_glue.c) */
#define PC_DUAL_YAW (45.0f * (65536.0f / 360.0f))   /* (as tps_glue.c DEG) */
#define PC_DUAL_PITCH (15.0f * (65536.0f / 360.0f))

/* enemy i for the other arm, by its yaw dy off the crosshair direction (> 0: left) and its pitch dp
   off the aim's (65536 per turn): 0 not (the crosshair's own enemy aim_e, or outside the cone), 1 the
   right arm, 2 the left. Last tick's (prev) keeps it in a cone 15 % wider. */
static inline int pc_dual_arm(int i, int aim_e, int prev, float dy, float dp)
{
    float w = i == prev ? 1.15f : 1.0f;
    if (i == aim_e || fabsf(dy) > PC_DUAL_YAW * w || fabsf(dp) > PC_DUAL_PITCH * w) return 0;
    return dy > 0 ? 2 : 1;
}

/* ---------------------------------------------------------------- music on a warp (cheat_glue.c) */
/* after the entry script of a room reached without a door, with the music that played before it
   (was, was_vol) stopped as a door does: the new music request. The same music asked for again plays
   on (as PlayBgm2; its volume eased if that changed), -1: none asked, the old one fades out (StopBgm
   400) as at a door; otherwise the request as it is (other music starts as usual). */
static inline int pc_warp_bgm(int req, int cur, int vol, int was, int was_vol, int playing)
{
    if ((req & 0x8) && cur == was) return vol != was_vol ? 0x2 : 0;
    if (cur < 0 && was >= 0 && playing) return -1;
    return req;
}

/* the same for an ambience slot (as CallBackGroundSe2; -1: StopBackGroundSeEx 400) */
static inline int pc_warp_bgse(int req, int cur, int was)
{
    if ((req & 0x2) && cur == was) return 0;
    if (cur < 0 && was >= 0) return -1;
    return req;
}

/* ---------------------------------------------------------------- "Unlock all" (cheat_glue.c) */
/* the part of a room's event block t (u32 offsets of scd0, scd1, the events; the first code offset
   ends the table) at offset ofs: 0 scd0, 1 scd1, 2 an event (they follow scd0 and scd1) */
static inline int pc_script_part(const unsigned int* t, unsigned int ofs)
{
    unsigned int first = t[0], lo = ~0u, i;
    for (i = 0; i * 4 < first; i++)
        if (t[i] && t[i] < first) first = t[i];
    for (i = 2; i * 4 < first; i++)
        if (t[i] && t[i] < lo) lo = t[i];
    if (ofs >= lo) return 2;
    return t[1] > t[0] ? ofs >= t[1] : ofs < t[0];
}

/* a PlItemCheck of a key item (key) in script part `part`, followed by op `next`, in room stg/rom with
   a use spot for the key (spot), passes for the cheat: in scd0/scd1 only (event tasks check items to
   give or take them), not when it guards a model change (36 InitModelSet: RM_3160 hides its zombies
   once the Emblem Card is carried), never in 0/9 (the metal detector checks every metal item there) */
static inline int pc_unlock_rule(int key, int part, int next, int stg, int rom, int spot)
{
    return key && part < 2 && next != 36 && !(stg == 0 && rom == 9) && spot;
}

/* ---------------------------------------------------------------- hor+ margins (native_glue.c) */
/* is a hidden room part drawn in the margins (view_glue.c pc_margin_part) left out? v: its n vertices
   in view space; kx * x + ox * z is the GS screen x times z, so each test is against a plane through
   the camera; zn: the near plane; pl: the player's view x, z.
   Mode 3 (its sphere reaches the camera or the line to the player): unless all vertices lie on one
   side of the 4:3 view, widened to take in the player (PC_PLAYER_HALF either side of its spot), or
   behind the near plane. Their hull then misses that view at every depth, so the part neither ends in
   a straight edge at the 4:3 border nor crosses the line to the player.
   Mode 2 (in front of the player): when a vertex lies in front of the camera inside the 4:3 screen;
   cut there, it would end in a straight edge at the 4:3 border. */
#define PC_PLAYER_HALF 8.0f   /* the player's collision radius (O_WRK ar) is 3.5 */
static inline int pc_margin_rule(int mode, const VU1_STRIP_BUF* v, int n, float kx, float ox, float zn, const float* pl)
{
    if (mode == 3) {
        float x0 = NATIVE_43_X0, x1 = NATIVE_43_X1, pz = pl[1];
        int left = 1, right = 1, behind = 1;
        if (pz > zn) {
            float xp = ox + kx * pl[0] / pz, pad = kx * PC_PLAYER_HALF / pz;
            if (xp - pad < x0) x0 = xp - pad;
            if (xp + pad > x1) x1 = xp + pad;
        }
        for (int i = 0; i < n; i++) {
            float x = ox * v[i].fVz + kx * v[i].fVx;
            if (x > x0 * v[i].fVz) left = 0;
            if (x < x1 * v[i].fVz) right = 0;
            if (v[i].fVz > zn) behind = 0;
        }
        return !(left || right || behind);
    }
    if (mode != 2) return 0;
    for (int i = 0; i < n; i++) {
        float x = ox * v[i].fVz + kx * v[i].fVx;   /* screen x times Vz */
        if (v[i].fVz > zn && x > NATIVE_43_X0 * v[i].fVz && x < NATIVE_43_X1 * v[i].fVz) return 1;
    }
    return 0;
}
