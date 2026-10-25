/* The cutscene cast randomizer in the game (docs/ACTORS.md "Cutscene cast"; the choices: pc/src/host/cast.c).
 *
 * At the end of every room load (enemyopt_glue.c enemy_room_finished, after the enemy randomizer) each room
 * record of the story cast (cast_character: Alfred, Steve, Rodrigo, ...; not the character played, whose
 * doubles are "You appear as"'s) and, with Enemies, each zombie, anatomist or Hunter record the room script
 * drives or shows (the enemy options' scan_script) is given another model (cast_model), loaded into a display
 * slot of the appearance system (ACTOR_CAST + k, one per model the room shows; the retargeting of
 * actor_glue.c). Then, only while a record has one (cast_n):
 *  - eneset.c bhDrawEnemy -> actor_put_enemy -> cast_draw: the model is drawn in the record's place, posed
 *    from its skeleton (a zombie's linked parts hidden with it, a blown-off head hidden on the model);
 *  - coop_glue.c coop_control_remote -> actor_double_tick -> cast_tick, every tick before the objects follow
 *    their links: a character's face takes its lip sync, and the hand pieces and props the script links to
 *    its hands go to the model's hands (as for the cutscene double).
 * Nothing else changes: the record keeps its type, its motions, hits, collision and script.
 */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../src/host/actors.h"
#include "../src/host/cast.h"
#include "../src/host/enemies.h"
#include <string.h>

void host_log(const char* fmt, ...);
extern int coop_on;
extern int cast_n;                      /* actor_glue.c: hooks run while > 0 */
unsigned coop_partner_import_id(void);  /* coop_glue.c: the partner's cast_import_id, 0 none */
BH_PWORK* pc_enemy_root(BH_PWORK* ep);  /* variant_glue.c */
int pc_enemy_draw_as(BH_PWORK* ep, int who);

#define CSLOTS (ACTOR_SLOTS - ACTOR_CAST)
#define RECS 128
static signed char rec_slot[RECS];      /* record -> cast slot, -1: drawn as itself */
static unsigned char rec_id[RECS], rec_ver[RECS], rec_chr[RECS];   /* what it was when given its model; a character */
static short slot_model[CSLOTS] = { -1, -1, -1, -1, -1, -1 };
_Static_assert(CSLOTS == 6, "slot_model");

/* records whose scene needs their own look */
static const struct { unsigned short room; unsigned char rec; const char* why; } kept[] = {
    { 7030, 1, "Alexia asleep in her ice capsule" },
};

/* the imported models count for the pool: ours, and in co-op only while the partner has the same list */
static int import_count(const EnemyOpts* o)
{
    int n = __atomic_load_n(&actor_import_n, __ATOMIC_ACQUIRE);
    if (!o->cast_imports || !n) return 0;
    if (coop_on && coop_partner_import_id() != cast_import_id()) return 0;
    return n;
}

static int shown(int slot) { return slot >= 0 && slot < CSLOTS && slot_model[slot] >= 0 && actor_shown(ACTOR_CAST + slot); }

/* record k still holds what was given a model (an enemy that died and a spawn took its slot: not) */
static int still(const BH_PWORK* ep, int k) { return (ep->flg & 0x1) && ep->id == rec_id[k] && ep->mdlver == rec_ver[k] && shown(rec_slot[k]); }

void cast_release(void)
{
    int k;
    for (k = 0; k < CSLOTS; k++)
        if (slot_model[k] >= 0) { actor_set(ACTOR_CAST + k, -1); slot_model[k] = -1; }
    memset(rec_slot, -1, sizeof(rec_slot));
    cast_n = 0;
}

/* enemyopt_glue.c, the end of the room load: scripted[k] & 3: the script drives (1) or shows (2) record k */
void cast_room(const EnemyOpts* o, const unsigned char* scripted)
{
    int want[RECS], k, j, n = 0, room = room_file_now(), pid = sys->ply_id & 3, imports;
    unsigned scene;
    memset(rec_slot, -1, sizeof(rec_slot));
    cast_n = 0;
    if (!o || !o->cast || !rom) { cast_release(); return; }
    imports = import_count(o);
    scene = o->cast_assign ? (unsigned)room + 1 : 0;
    for (k = 0; k < rom->ene_n && k < RECS; k++) {
        BH_PWORK* ep = &ene[k];
        int c, kind = -1, key = -1;
        want[k] = -1;
        if (!(ep->flg & 0x1) || !ep->mlwP) continue;
        c = cast_character(ep->id, ep->mdlver);
        if (c >= 0 && (o->cast & CAST_CHARS) && !(c == pid && c < 4)) kind = CASTK_CHAR, key = c;   /* (the character played: "You appear as") */
        else if (c < 0 && (o->cast & CAST_ENEMIES) && (scripted[k] & 3) && !(ep->flg & 0x80) && (kind = cast_enemy_kind(ep->id)) >= 0) key = cast_look(ep->id, ep->mdlver);
        if (kind < 0 || key < 0) continue;
        for (j = 0; j < (int)(sizeof(kept) / sizeof(kept[0])) && !(kept[j].room == room && kept[j].rec == k); j++);
        if (j < (int)(sizeof(kept) / sizeof(kept[0]))) { host_log("cast: room %04d record %d (type %d) stays: %s\n", room, k, ep->id, kept[j].why); continue; }
        want[k] = cast_model((unsigned)o->seed, scene, kind, key, kind == CASTK_CHAR && o->cast_all, imports);
        rec_chr[k] = kind == CASTK_CHAR;
    }
    for (j = 0; j < CSLOTS; j++) {   /* models no record wants go (normally all went with the last room's textures) */
        for (k = 0; k < rom->ene_n && k < RECS && want[k] != slot_model[j]; k++);
        if (slot_model[j] >= 0 && !(k < rom->ene_n && k < RECS)) { actor_set(ACTOR_CAST + j, -1); slot_model[j] = -1; }
        if (slot_model[j] >= 0 && !actor_shown(ACTOR_CAST + j)) slot_model[j] = -1;   /* (njReleaseTextureAll took it) */
    }
    for (k = 0; k < rom->ene_n && k < RECS; k++) {
        const ActorModel* am;
        const char* name;
        int s = -1;
        if (want[k] < 0) continue;
        am = want[k] >= ACTOR_PART ? NULL : actor_model(want[k]);
        name = am ? am->name : "a player model";
        for (j = 0; j < CSLOTS && s < 0; j++) if (slot_model[j] == want[k]) s = j;
        for (j = 0; j < CSLOTS && s < 0; j++)
            if (slot_model[j] < 0) {
                if (actor_set(ACTOR_CAST + j, want[k]) && actor_rigged(ACTOR_CAST + j)) { slot_model[j] = (short)want[k]; s = j; }
                else { actor_set(ACTOR_CAST + j, -1); break; }
            }
        if (s < 0) {
            host_log("cast: room %04d record %d (type %d model %d): %s %s, drawn as itself\n", room, k, ene[k].id, ene[k].mdlver, name,
                     j >= CSLOTS ? "has no display slot left" : "cannot be loaded or mapped");
            continue;
        }
        rec_slot[k] = (signed char)s;
        rec_id[k] = (unsigned char)ene[k].id;
        rec_ver[k] = ene[k].mdlver;
        cast_n++;
        n++;
        host_log("cast: room %04d record %d (type %d model %d) -> %s (%s, slot %d)\n", room, k, ene[k].id, ene[k].mdlver, name,
                 rec_chr[k] ? "character" : "enemy", s);
    }
    host_log("cast: room %04d: %d records drawn as other models (seed %d, %s, %s%s)\n", room, n, o->seed, o->cast_assign ? "random per cutscene" : "fixed",
             o->cast_all ? "all models" : "cutscene models", imports ? ", imported models" : "");
}

/* actor_double_tick: every character shown, posed with its face and hands */
void cast_tick(void)
{
    int k;
    for (k = 0; k < (int)sys->ewk_n && k < RECS; k++) {
        BH_PWORK* ep = &ene[k];
        if (rec_slot[k] < 0 || !rec_chr[k] || !still(ep, k) || (ep->stflg & 0x41000000) || !ep->mlwP) continue;
        actor_act(ACTOR_CAST + rec_slot[k], ep);
    }
}

/* actor_put_enemy: 1 = drawn (or left out: an enemy's part the model stands in for) */
int cast_draw(BH_PWORK* ep)
{
    BH_PWORK* r;
    int k = (int)(ep - ene);
    if (k >= 0 && k < RECS && rec_slot[k] >= 0 && rec_chr[k] && still(ep, k)) return actor_put_as(ACTOR_CAST + rec_slot[k], ep);
    r = pc_enemy_root(ep);
    k = (int)(r - ene);
    if (k >= 0 && k < RECS && rec_slot[k] >= 0 && !rec_chr[k] && still(r, k)) return pc_enemy_draw_as(ep, ACTOR_CAST + rec_slot[k]);
    return 0;
}
