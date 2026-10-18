/* Enemy variants in the game (docs/ENEMIES.md "Enemy variants"; the table: pc/src/host/variants.c).
 *
 * A variant is spawned as its base type (spawn_glue.c spawn_at, the note's `variant`) and remembered by
 * its slot. Then, only while a room has one (pc_variant_n), hooks in the decomp apply it:
 *  - eneset.c bhControlEnemy around its step (pc_variant_pre/post): the weapon damage it is about to
 *    take scaled (ep->dam[], which bhEne_CalcDamage sums into total_dam: armour / hp), the player's hit
 *    points the step took scaled (damage), and the steps per frame (speed);
 *  - eneset.c bhDrawEnemy (pc_variant_draw): another model ("You appear as" catalogue) posed from the
 *    base's skeleton every time it is drawn (actor_glue.c retargeting, display slots ACTOR_VARIANT + k,
 *    one per model in the room), with sub-models on its nodes (attach), and the drawn size (scale);
 *  - sdfunc.c RequestEnemySeBasic (pc_variant_sound): sound programs of the base played from another
 *    room's enemy sound bank;
 *  - its motion set: a copy of the base's with motions swapped (motions), set at the spawn.
 * Its parts and children (linked to it) count as the variant. Everything is keyed by the variant id,
 * which co-op spawn notes carry, so a guest makes the same enemy.
 */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/MdlPut.h"
#include "../src/host/actors.h"
#include "../src/host/enemies.h"
#include "../src/host/variants.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void host_log(const char* fmt, ...);
void RequestEnemySe(int EnemyNo, NJS_POINT3* pPos, int SeNo);   /* sdfunc.c */
int snd_xbank_load(int key);
extern int enemy_room_gen;              /* enemyopt_glue.c */

#define VSLOTS (ACTOR_SLOTS - ACTOR_VARIANT)
#define VSETS 4                         /* variants with their own motion set in one room */

int pc_variant_n;                       /* variant enemies in this room */
static unsigned short slot_var[128];    /* slot -> variant id (the variant's body) */
static float steps[128], dam_left[128];
static short vmodel[VSLOTS] = { -1, -1 };   /* catalogue model shown in variant display slot k */
static struct { unsigned short id; MN_WORK set[512]; } mset[VSETS];
static struct { unsigned short id; signed char bank; } vbank[VARIANT_MAX];
static int nbank;
static int pre_hp, hurt_now;
static BH_PWORK* pre_pl;

static BH_PWORK* root(BH_PWORK* ep)
{
    int k;
    for (k = 0; k < 3 && ep->lkwkp && (BH_PWORK*)ep->lkwkp >= ene && (BH_PWORK*)ep->lkwkp < ene + 128; k++) ep = (BH_PWORK*)ep->lkwkp;
    return ep;
}

/* the variant ep belongs to (its own, or its parent's), NULL */
static const EnemyVariant* of(BH_PWORK* ep)
{
    const EnemyVariant* v;
    BH_PWORK* r;
    if (ep < ene || ep >= ene + 128) return NULL;
    r = root(ep);
    if (!slot_var[r - ene] || !(r->flg & 0x1)) return NULL;   /* (a body keeps its slot until the room is left) */
    v = variant_by_id(slot_var[r - ene]);
    return v && v->type == r->id ? v : NULL;
}

static int model_slot(int model)
{
    int k, at = -1;
    for (k = 0; k < VSLOTS; k++) {
        if (vmodel[k] >= 0 && !actor_shown(ACTOR_VARIANT + k)) vmodel[k] = -1;   /* njReleaseTextureAll unloaded it (game over, the title) */
        if (vmodel[k] == model && model >= 0) at = k;
    }
    return at;
}

/* room load (enemyopt_glue.c enemy_room_finished, before the randomizer) */
void variant_room(void)
{
    memset(slot_var, 0, sizeof(slot_var));
    memset(mset, 0, sizeof(mset));
    nbank = 0;
    pc_variant_n = 0;
}

/* the models go with the room's textures (spawn_glue.c spawn_release: bhReleaseMainTexture) */
void variant_release(void)
{
    int k;
    for (k = 0; k < VSLOTS; k++)
        if (vmodel[k] >= 0) { actor_set(ACTOR_VARIANT + k, -1); vmodel[k] = -1; }
}

/* spawn_glue.c spawn_at: slot `slot` now holds variant `id` (its base type realized there) */
void variant_spawned(int slot, int id)
{
    const EnemyVariant* v = variant_by_id(id);
    BH_PWORK* ep = &ene[slot];
    const char* look = "its base's model";
    int k, i;
    if (!v || ep->id != v->type) {
        host_log("variant: slot %d: %s %d, it plays as its base type %d\n", slot, v ? "not the base type of variant" : "unknown variant", id, ep->id);
        return;
    }
    slot_var[slot] = (unsigned short)id;
    steps[slot] = dam_left[slot] = 0;
    pc_variant_n++;
    if (v->model >= 0) {
        if ((k = model_slot(v->model)) < 0)
            for (k = 0; k < VSLOTS && vmodel[k] >= 0; k++);
        if (k == VSLOTS) look = "no display slot left: its base's model";
        else if (vmodel[k] == v->model) look = actor_models[v->model].name;
        else if (actor_set(ACTOR_VARIANT + k, v->model) && actor_rigged(ACTOR_VARIANT + k)) { vmodel[k] = v->model; look = actor_models[v->model].name; }
        else { actor_set(ACTOR_VARIANT + k, -1); look = "its model could not be loaded or mapped: its base's"; }
    }
    if (v->motion_n && ep->mnwPb) {   /* its own copy of the set, motions swapped */
        for (k = 0; k < VSETS && mset[k].id && mset[k].id != id; k++);
        if (k < VSETS) {
            if (!mset[k].id) {
                memcpy(mset[k].set, ep->mnwPb, sizeof(mset[k].set));
                for (i = 0; i < v->motion_n; i++) mset[k].set[v->motion[i][0]] = ep->mnwPb[v->motion[i][1]];
                mset[k].id = (unsigned short)id;
            }
            ep->mnwP = ep->mnwPb = mset[k].set;
        } else host_log("variant: %s: no motion set left, the base's motions\n", v->name);
    }
    if ((v->sound_n || v->hurt) && v->sound_room) {
        for (k = 0; k < nbank && vbank[k].id != id; k++);
        if (k == nbank && nbank < VARIANT_MAX) {
            vbank[nbank].id = (unsigned short)id;
            vbank[nbank++].bank = (signed char)snd_xbank_load(v->sound_room);
        }
    }
    host_log("variant: slot %d is %s (variant %d, type %d model %d): drawn as %s, hp x%.2f, armour x%.2f, damage x%.2f, speed x%.2f, scale x%.2f\n",
             slot, v->name, id, ep->id, ep->mdlver, look, v->hp, v->armour, v->damage, v->speed, v->scale);
}

/* ---------------------------------------------------------------- around its step */
int pc_variant_pre(BH_PWORK* ep, int slot)
{
    const EnemyVariant* v = of(ep);
    int runs = 1, i;
    pre_pl = NULL;
    if (!v) return 1;
    if ((ep->flg & 0x4) && (v->armour != 1.0f || v->hp != 1.0f) && ep->mlwP) {   /* the hit it is about to take */
        float f = v->armour / v->hp;
        for (i = 1; i < (int)ep->mlwP->obj_num && i < 64; i++)
            if (ep->dam[i] > 0) {
                float x = ep->dam[i] * f + dam_left[slot];
                int d = (int)x;
                dam_left[slot] = x - d;
                ep->dam[i] = d > 0 || f == 0 ? d : 1;   /* (a joint hit stays a hit for the combo counts) */
            }
    }
    if ((ep->flg & 0x4) && v->hurt && ep->hp >= 0 && ep == root(ep)) {   /* its own hurt cry (a bank 3 voice, as en17.c's) */
        for (i = 0; i < nbank && vbank[i].id != v->id; i++);
        if (i < nbank && vbank[i].bank > 0) {
            hurt_now = vbank[i].bank;
            RequestEnemySe(slot, (NJS_POINT3*)&ep->px, 0x1002300 | v->hurt);
            hurt_now = 0;
        }
    }
    if (v->speed != 1.0f) {
        steps[slot] += v->speed;
        runs = (int)steps[slot];
        if (!runs && (ep->flg & 0x4)) runs = 1;   /* a hit is not skipped */
        steps[slot] -= runs;
    }
    if (v->damage != 1.0f && plp) { pre_pl = plp; pre_hp = plp->hp; }
    return runs;
}

void pc_variant_post(BH_PWORK* ep, int slot)
{
    const EnemyVariant* v;
    (void)slot;
    if (!pre_pl || pre_pl != plp || plp->hp >= pre_hp || !(v = of(ep))) return;
    {
        int hp = pre_hp - (int)lroundf((pre_hp - plp->hp) * v->damage);
        if (plp->hp >= 0) plp->hp = hp < 0 ? -1 : hp;   /* (a blow the game made fatal stays fatal) */
    }
    pre_pl = NULL;
}

/* ---------------------------------------------------------------- drawing */
/* its node matrices scaled by s about (x, y, z), or back (1 / s) */
static void scale_nodes(ML_WORK* m, float s, float x, float y, float z)
{
    int i, j;
    for (i = 0; m && m->owP && i < (int)m->obj_num; i++) {
        float* w = m->owP[i].mtx;
        for (j = 0; j < 12; j++) if ((j & 3) != 3) w[j] *= s;
        w[12] = x + (w[12] - x) * s; w[13] = y + (w[13] - y) * s; w[14] = z + (w[14] - z) * s;
    }
}

/* sub-model `sub` of display work w drawn on its node `node` (as Monster Steve's arms, linked enemies) */
static void attach_draw(BH_PWORK* w, int sub, int node)
{
    static BH_PWORK a;
    ML_WORK* m = w->mlwP;
    if (sub >= w->mdl_n || !w->mdl[sub].objP || !w->mdl[sub].owP || node >= (int)m->obj_num) return;
    memset(&a, 0, sizeof(a));
    a.flg = 0x1;
    a.mdl[0] = w->mdl[sub];
    a.mlwP = &a.mdl[0];
    a.mtx = (float(*)[16])a.mtxbuf;
    a.draw_tp = w->draw_tp;
    memcpy(a.mtxbuf, m->owP[node].mtx, sizeof(float) * 16);
    bhCalcTree((NJS_MATRIX*)a.mtx, a.mlwP);
    bhPutModel(&a);
}

int pc_variant_draw(BH_PWORK* ep)
{
    const EnemyVariant* v = of(ep);
    BH_PWORK* r;
    int k, i;
    if (!v) return 0;
    r = root(ep);
    if (v->model >= 0 && (k = model_slot(v->model)) >= 0) {
        BH_PWORK* w;
        if (ep != r) return (ep->flg & 0x80) != 0;   /* its body's parts: the model stands in for all of them */
        actor_pose(ACTOR_VARIANT + k, ep);
        if (!(w = actor_work(ACTOR_VARIANT + k))) return 0;
        w->draw_tp = ep->draw_tp;
        if (v->scale != 1.0f) scale_nodes(w->mlwP, v->scale, ep->px, ep->py, ep->pz);
        bhPutModel(w);
        for (i = 0; i < v->attach_n; i++) attach_draw(w, v->attach[i][0], v->attach[i][1]);
        return 1;
    }
    if (v->scale == 1.0f) return 0;
    scale_nodes(ep->mlwP, v->scale, r->px, r->py, r->pz);
    bhPutModel(ep);
    scale_nodes(ep->mlwP, 1.0f / v->scale, r->px, r->py, r->pz);
    return 1;
}

/* ---------------------------------------------------------------- sounds */
int pc_variant_sound(int enemy_no, int se, signed char* bank)
{
    static int trace = -1;
    const EnemyVariant* v;
    int k, i;
    if (enemy_no < 0 || enemy_no >= 127 || !(v = of(&ene[enemy_no]))) return se;
    if (trace < 0) trace = getenv("RECVX_TRACE_SND") != NULL;
    if (trace)   /* for sound_map: which of its base's programs it plays when (RECVX_TRACE_SND) */
        host_log("variant: slot %d (%s) motion %d frame %d requests sound %#x (program %d)\n", enemy_no, v->name, ene[enemy_no].mtn_no,
                 ene[enemy_no].frm_no >> 16, se, se & 0xFF);
    if (hurt_now) { *bank = (signed char)hurt_now; return se; }
    if (!v->sound_n) return se;
    for (k = 0; k < nbank && vbank[k].id != v->id; k++);
    if (k == nbank || vbank[k].bank <= 0) return se;
    for (i = 0; i < v->sound_n; i++)
        if ((se & 0xFF) == v->sound[i][0]) { *bank = vbank[k].bank; return (se & ~0xFF) | v->sound[i][1]; }
    return se;
}

/* ---------------------------------------------------------------- the spawner and the randomizer */
/* the spawner entry of v's base (its type and model version), NULL: none */
const ActorModel* spawn_entry(int i);
int spawn_base_count(void);
const ActorModel* variant_base(const EnemyVariant* v)
{
    int i;
    for (i = 0; i < spawn_base_count(); i++)
        if (spawn_entry(i)->id == v->type && spawn_entry(i)->mdlver == v->mdlver) return spawn_entry(i);
    return NULL;
}

int variant_of_slot(int slot) { return slot >= 0 && slot < 128 && (ene[slot].flg & 0x1) ? slot_var[slot] : 0; }
