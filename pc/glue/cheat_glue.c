/* F2 cheat menu: applied on the game thread once per frame (at the frame hand-off), so the
   game never sees its state change mid-frame. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/room.h"
#include "../../decomp/include/ps2/veronica/prog/player.h"
#include "../../decomp/include/ps2/veronica/prog/effect.h"
#include "../../decomp/include/ps2/veronica/prog/sub1.h"
#include "../../decomp/include/ps2/veronica/prog/pwksub.h"
#include "../../decomp/include/ps2/veronica/prog/item.h"
#include "../../decomp/include/ps2/veronica/prog/hitchk.h"
#include "../src/host/cheat.h"
#include "../src/host/coop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CheatState g_cheat = { .give_count = 1 };

/* infinite ammo: every shot goes through bhCheckBullet/bhCountBullet (weapon.c), which then take
   the path of the game's own unlimited weapons (no count check, no decrement) */
int pc_inf_ammo(void) { return g_cheat.inf_ammo; }

/* noclip: player.c bhControlPlayer skips the enemy push (flg 0x8) and the wall check (0x10,
   which also does slopes) for the returned flags. Walking never changes the height otherwise
   (no gravity; height comes from the room's floor levels), so the player stays at its height.
   Mode 2 also moves up/down (pad_noclip_vertical) and keeps the floor number (triggers, walls
   per floor) in step. Turning noclip off after flying puts the player back on the floor level
   it took off from (the height levels say nothing about where there is ground, so a level
   between is no safer); walls push it out as usual - in the room it took off in (another room's
   level means nothing here: there it stays where it is). Only our player: co-op runs the partner
   through plp too. */
int pad_noclip_vertical(void);
unsigned int pc_noclip(void)
{
    static int flew, base, room;   /* base: floor number at take-off, in that room */
    int mode = g_cheat.noclip, v;
    if (plp != &ply) return 0;
    if (mode == 2 && plp->mode0 == 1 && (v = pad_noclip_vertical())) {
        if (!flew) { flew = 1; base = plp->flr_no; room = coop_room_key(); }
        plp->py += v;   /* a unit per frame; the player is about 17 tall */
        bhSetFloorNum(plp);
    }
    if (!mode && flew) {
        if (room == coop_room_key()) {
            plp->py = rom->grand[base + 2];
            bhSetFloorNum(plp);
        }
        flew = 0;
    }
    return mode ? 0x18 : 0;
}

/* Unlock all: the game opens a lock in one of two ways, both kept, with the key taken as carried only there.
   - A use spot is a floor record of type 0 listing up to four items (prm0..3): standing in it, the item screen's
     Use (sub1.c ItemUse/Use_05) sets sb_id and cb_flg 0x400, and the room script, which runs on in the item
     screen (st_flg 0x8), tests "on floor N, item X used" and plays the unlock. Here the examine press in such a
     spot uses the spot's key item instead (pc_unlock_examine): the next scd1 sees exactly that state. When the
     script does nothing with it (a spot already done), the press examines as usual, one frame later.
   - A door with a question ("Will you use the Silver Key?") is chosen by the script's PlItemCheck: scd0 sets
     the door's message by it, scd1 opens the door on "Yes" when it still holds. pc_unlock_held passes such a
     check for a key item (itemdata type 0x40: no weapon, ammo, herb, ribbon or lighter) that has a use spot in
     this room, in scd0/scd1 only (event tasks check items to give or take them, RM_9160's jewels), not when the
     check guards a model change (RM_3160 hides its zombies once the Emblem Card is carried), and never in 0/9,
     where the metal detector checks every metal item (keys and emblems among them) with the same op.
   No item is added; the scripts' PlItemLost/PlItemLostEx of a key the player does not have find nothing. */
static int key_item(int id) { return id > 0 && id < 156 && (itemdata[id].type & 0x40); }
static int carried(int id)
{
    for (int c = sys->ply_id * 16 + 1; c < sys->ply_id * 16 + 12; c++)   /* as bhPlItemCheck */
        if ((unsigned char)(sys->itm[c] >> 16) == id) return 1;
    return 0;
}

extern unsigned char* bhScePtr;   /* event.c */
extern BH_SCEWORK bhEtask[16];
int pc_unlock_held(int item)
{
    const unsigned int* t = (const unsigned int*)rom->evtp;
    unsigned int ofs, first, lo = ~0u;
    if (!g_cheat.unlock || !key_item(item) || (sys->stg_no == 0 && sys->rom_no == 9) || *bhScePtr == 36 /* InitModelSet */) return 0;
    first = t[0];   /* the offset table (scd0, scd1, events) ends where the first script begins */
    for (unsigned int i = 0; i * 4 < first; i++)
        if (t[i] && t[i] < first) first = t[i];
    for (unsigned int i = 2; i * 4 < first; i++)
        if (t[i] && t[i] < lo) lo = t[i];
    ofs = (unsigned int)(bhScePtr - (const unsigned char*)t);
    if (ofs >= lo) return 0;   /* an event (they follow scd0 and scd1) */
    for (int i = 0; i < rom->flr_n; i++) {
        const ATR_WORK* z = &rom->flrp[i];
        if (z->type == 0 && (z->prm0 == item || z->prm1 == item || z->prm2 == item || z->prm3 == item)) return 1;
    }
    return 0;
}

static int unlock_item, unlock_sb;
int pc_unlock_examine(void)
{
    static unsigned char turn;   /* a spot for several items (the proofs, the jewels): each press the next */
    unsigned char ids[4];
    int n = 0;
    if (!g_cheat.unlock || plp != &ply || !(sys->cb_flg & 0x200) || sys->flr_idx >= (unsigned int)rom->flr_n) return 0;
    {
        const ATR_WORK* z = &rom->flrp[sys->flr_idx];
        const unsigned char p[4] = { z->prm0, z->prm1, z->prm2, z->prm3 };
        if (z->type != 0) return 0;
        for (int i = 0; i < 4; i++)
            if (key_item(p[i]) && !carried(p[i])) ids[n++] = p[i];
    }
    if (!n) return 0;   /* carried: the item screen uses it as usual */
    unlock_item = ids[turn++ % n];
    return 1;
}

void pc_unlock_scd1(int after)
{
    static struct { unsigned int st, cb, rm, sp, ev[32], ky[16]; unsigned char task[16]; } was, now;
    static int st8;
    int taken;
    if (!unlock_item) return;
    if (!after) {   /* the frame after the item screen's Use */
        unlock_sb = sys->sb_id;
        sys->sb_id = unlock_item;
        sys->cb_flg |= 0x400;
        st8 = !(sys->st_flg & 0x8);
        sys->st_flg |= 0x8;
    }
    now.st = sys->st_flg; now.cb = sys->cb_flg; now.rm = sys->rm_flg; now.sp = sys->sp_flg;
    memcpy(now.ev, sys->ev_flg, sizeof(now.ev));
    memcpy(now.ky, sys->ky_flg, sizeof(now.ky));
    for (int i = 0; i < 16; i++) now.task[i] = bhEtask[i].status;
    if (!after) { was = now; return; }
    taken = memcmp(&was, &now, sizeof(now)) != 0;
    if (st8) sys->st_flg &= ~0x8;
    sys->cb_flg &= ~0x400;   /* StatusMain clears it on its next frame */
    printf("unlock: item %d used in %d/%d, %s\n", unlock_item, sys->stg_no, sys->rom_no, taken ? "taken" : "nothing reacts");
    unlock_item = 0;
    if (!taken) {   /* nothing here wants it: the press examines */
        sys->sb_id = unlock_sb;
        if (plp == &ply) bhCheckExmAtari(plp);
    }
}

/* item and room names from the game's own message data: glyph = ASCII - 32, 0xFF01 = space. Built when the data
   appears or moves (the system load sets sys->mes_ip twice: the file, then its table), not every frame:
   the overlay (render thread) reads them. A name that changes is published whole: its first character
   last, so a reader sees the old name, an empty one (skipped) or the new one. */
static void publish(char* out, int size, const unsigned int* tab, int i)
{
    unsigned int ofs = tab[i + 1];
    const unsigned short* dp = (const unsigned short*)((const char*)tab + ofs);
    char name[32] = { 0 };
    int n = 0;
    if (ofs < 0x40000)   /* sysmes.ald is 87 KB */
        for (; n < size - 1 && *dp != 0xFFFF; dp++) name[n++] = *dp == 0xFF01 ? ' ' : *dp < 95 ? (char)(*dp + 32) : '?';
    while (n && name[n - 1] == ' ') name[--n] = 0;   /* "Starting Part 2 " */
    if (!memcmp(name, out, size)) return;
    __atomic_store_n(&out[0], 0, __ATOMIC_RELEASE);
    memcpy(out + 1, name + 1, size - 1);
    __atomic_store_n(&out[0], name[0], __ATOMIC_RELEASE);
}

static void read_names(void)
{
    static const unsigned int* built;
    if (sys->mes_ip == built) return;
    built = sys->mes_ip;
    for (int id = 0; id < CHEAT_ITEMS; id++) publish(g_cheat.names[id], sizeof(g_cheat.names[id]), sys->mes_ip, id);
    /* the save menu's room names (the teleport lists, pc/src/host/teleport.c): message 215 + RoomNameSwitch of the second table */
    for (int k = 1; k < 19 && sys->mes_sp && sys->mes_sp[0] > 233; k++) publish(g_cheat.rooms[k], sizeof(g_cheat.rooms[k]), sys->mes_sp, 215 + k);
}

/* Music on a room change without a door. The room scripts turn their music and ambience off as
   the player uses a door to a room without them (BgmOff2, BGSeOff2), and the next room's entry
   script (scd0) asks for its own (BgmOn2, BGSeOn2), which plays on when it is already playing.
   With no door the first half is missing, and the music went on in rooms without their own.
   So around the next room's entry script (sdfunc.c Com_StartInitScript, Com_FinishInitScript):
   before it, the music and ambience are off as such a door leaves them; after it, what the
   script asked for again plays on, and the rest fades out as at a door (Com_ExecRoomFadeOut). */
extern int CurrentBgmNo, CurrentBgmVolume, ReqFadeBgmNo, CurrentBgSeNo[2], ReqFadeBgSe[2];
extern unsigned int AdxPlayFlag[2];
void StopBgm(int FadeOutRate);   /* sdfunc.h (its macros.h clashes here) */
void StopBgm2();
void StopBackGroundSeEx(unsigned int SlotNo, short Timer);
void StopBackGroundSe2(unsigned int SlotNo);
static int warp_sound, warp_bgm, warp_vol, warp_bgse[2];

void cheat_warp_sound(int after)
{
    int i;
    if (!warp_sound) return;
    if (!after) {
        warp_bgm = CurrentBgmNo; warp_vol = CurrentBgmVolume;
        StopBgm2();
        for (i = 0; i < 2; i++) { warp_bgse[i] = CurrentBgSeNo[i]; StopBackGroundSe2(i); }
        return;
    }
    warp_sound = 0;
    if ((ReqFadeBgmNo & 0x8) && CurrentBgmNo == warp_bgm) ReqFadeBgmNo = CurrentBgmVolume != warp_vol ? 0x2 : 0;   /* as PlayBgm2 */
    else if (CurrentBgmNo < 0 && warp_bgm >= 0 && AdxPlayFlag[0]) StopBgm(400);
    for (i = 0; i < 2; i++)
        if ((ReqFadeBgSe[i] & 0x2) && CurrentBgSeNo[i] == warp_bgse[i]) ReqFadeBgSe[i] = 0;   /* as CallBackGroundSe2 */
        else if (CurrentBgSeNo[i] < 0 && warp_bgse[i] >= 0) StopBackGroundSeEx(i, 400);
}

/* the game's own door transition (door type 255: no animation), for the teleport, the co-op
   follow and test aids. An enemy grab plays the player on the enemy's motion set (en01sub.c);
   the new room frees it, and the room change would animate from it (crash in bhSetMotion), so
   take the player's own set back first, as the game does when a grab ends. */
void cheat_room_warp(int stg, int rom, int pos)
{
    plp->mnwP = plp->mnwPb;
    warp_sound = 1;
    bhSetDoorDemo(0, stg, rom, pos, 255);
}

static void teleport(int stg, int rom, int pos)
{
    static const char* pl = (const char*)-1;
    if (pl == (const char*)-1) pl = getenv("RECVX_PLAYER");
    if (pl) {   /* test aid: the first teleport switches the character as script op bhPlayerChangeSet does */
        sys->cng_pid = atoi(pl) & 3;
        sys->cb_flg |= 0x80;
        pl = NULL;
    }
    cheat_room_warp(stg, rom, pos);
}

/* Testing aid for headless runs, keyed to game frames since start (frames in which the game
   task advanced sys->gfrm_ct, which restarts per room):
   RECVX_TELEPORT="frame:stage:room:pos,delay:stage:room:pos,..." (later entries: frames after
   the previous teleport) and RECVX_EQUIP="frame:item:count[,item:count...]" (puts a weapon item
   in the first slot and equips it with the inventory's WeaponSet; the item may carry its ammo
   kind, e.g. 4102 = 0x1006 grenade launcher with acid rounds; further entries fill the next
   slots; after a ';' another "frame:item:count" equips again later: a weapon switch).
   RECVX_WARP="stage:room:frame:x:z:angle,..." puts the player at x/z facing `angle` once it has
   been `frame` game frames in that room (independent of how the room was reached, e.g. a
   co-op join).
   RECVX_INF_AMMO=1 turns the infinite ammo cheat on, RECVX_UNLOCK=1 "Unlock all"; RECVX_NOCLIP="frame:mode,..." sets the
   noclip mode (0/1/2) at that game frame; it also logs the position every 15 game frames and any jump
   of more than 2 units in one tick ("test: JUMP").
   RECVX_EFFECT="frame:effno:type:mode1" spawns effect effno at the player at that game frame
   and sets its mode1 two frames later (e.g. "60:110:0:1": the full-screen distortion of
   bhEff110, which reads the frame buffer back every frame - StoreRenderTex).
   RECVX_PLAYER=n: the first RECVX_TELEPORT also switches the character played to n (0 Claire,
   1 Chris, 2 Steve, 3 Wesker) as the story's character switch does.
   RECVX_APPEAR="frame:chr,..." sets "You appear as" (g_coop.chr, the ini's coop_char) at that
   game frame.
   RECVX_SKIP_CINE=n skips each cutscene (as its skip button: cb_flg 0x10000000) after n game frames.
   RECVX_FLAGS="set:bit,..." sets these story flags at the first game frame (set as in the room
   scripts: 1 ev_flg, 2 ky_flg, 3 ed_flg, 7 it_flg, 8 mp_flg; e.g. "1:183,1:184"). */
static void scripted(unsigned int* inv)
{
    static const char *tp = (const char*)-1, *eq, *wp, *ef, *nc, *ap;
    static int ef_slot = -1, ef_frame, nclog;
    float x, z;
    static int last, clock, prev, room, in_room;
    int f, a, b, c, n;
    if (tp == (const char*)-1) {
        tp = getenv("RECVX_TELEPORT"); eq = getenv("RECVX_EQUIP"); wp = getenv("RECVX_WARP"); ef = getenv("RECVX_EFFECT");
        if (getenv("RECVX_INF_AMMO")) g_cheat.inf_ammo = 1;
        if (getenv("RECVX_UNLOCK")) g_cheat.unlock = 1;
        nc = getenv("RECVX_NOCLIP");
        nclog = nc != NULL;
        ap = getenv("RECVX_APPEAR");
    }
    if (nclog && sys->gfrm_ct != prev && clock % 15 == 0)   /* with RECVX_NOCLIP: the path taken */
        printf("test: game frame %d room %d/%d pos %.1f %.1f %.1f floor %d noclip %d\n", clock, sys->stg_no, sys->rom_no, plp->px, plp->py,
               plp->pz, plp->flr_no, g_cheat.noclip);
    if (nclog) {   /* and any jump of more than 2 units in one tick (the player moved by code, not by walking) */
        static float lx, lz;
        static int lroom = -1;
        if (lroom == coop_room_key() && (plp->px - lx) * (plp->px - lx) + (plp->pz - lz) * (plp->pz - lz) > 4.0f)
            printf("test: JUMP at game frame %d room %d/%d from %.1f %.1f to %.1f %.1f %.1f floor %d\n", clock, sys->stg_no, sys->rom_no, lx, lz,
                   plp->px, plp->py, plp->pz, plp->flr_no);
        lx = plp->px; lz = plp->pz; lroom = coop_room_key();
    }
    if (nc && sscanf(nc, "%d:%d%n", &f, &a, &n) == 2 && clock >= f) {
        printf("test: noclip %d at game frame %d (%.1f %.1f %.1f)\n", a, clock, plp->px, plp->py, plp->pz);
        g_cheat.noclip = a;
        nc = nc[n] == ',' ? nc + n + 1 : NULL;
    }
    if (wp && sscanf(wp, "%d:%d:%d:%f:%f:%d%n", &a, &b, &f, &x, &z, &c, &n) == 6 && sys->stg_no == a && sys->rom_no == b &&
        in_room >= f && !(sys->gm_flg & 0x9) && !(sys->cb_flg & 0x1)) {
        printf("test: warp to (%.1f %.1f) in %d/%d\n", x, z, a, b);
        {   /* the player and its collision points move together: no wall check across the jump */
            EXP_WORK* e = (EXP_WORK*)plp->exp0;
            float dx = x - plp->px, dz = z - plp->pz;
            plp->px += dx; plp->pxb += dx; plp->pz += dz; plp->pzb += dz; plp->ay = c;
            plp->flg &= ~0xC0000;   /* not placed by the legs of the last pose (bhFixPosition) */
            if (e) { e->fpx += dx; e->bpx += dx; e->bpxb += dx; e->spx += dx; e->plx += dx; e->fpz += dz; e->bpz += dz; e->bpzb += dz; e->spz += dz; e->plz += dz; }
        }
        wp = wp[n] == ',' ? wp + n + 1 : NULL;
    }
    {
        static const char* fl = (const char*)-1;
        if (fl == (const char*)-1) fl = getenv("RECVX_FLAGS");
        for (; fl && sscanf(fl, "%d:%d%n", &a, &b, &n) == 2; fl = fl[n] == ',' ? fl + n + 1 : NULL) {
            unsigned int* w = a == 1 ? sys->ev_flg : a == 2 ? sys->ky_flg : a == 3 ? sys->ed_flg : a == 7 ? sys->it_flg : a == 8 ? sys->mp_flg : NULL;
            if (w && b >= 0 && b < 256 * (a == 8 ? 1 : a == 2 || a == 7 ? 2 : 4)) { w[b >> 5] |= 0x80000000u >> (b & 31); printf("test: flag %d:%d set\n", a, b); }
        }
    }
    {
        static const char* sk = (const char*)-1;
        static int cine;
        if (sk == (const char*)-1) sk = getenv("RECVX_SKIP_CINE");
        if (sk && (sys->cb_flg & 0x4) && !(sys->st_flg & 0x1C040008)) {
            if (sys->gfrm_ct != prev && ++cine == atoi(sk)) { sys->cb_flg |= 0x10000000; printf("test: cutscene skipped in %d/%d\n", sys->stg_no, sys->rom_no); }
        } else cine = 0;
    }
    if (coop_room_key() != room) { room = coop_room_key(); in_room = 0; }
    if (sys->gfrm_ct != prev) { prev = sys->gfrm_ct; clock++; in_room++; }
    if (tp && sscanf(tp, "%d:%d:%d:%d%n", &f, &a, &b, &c, &n) == 4 && clock >= f + last &&
        !(sys->gm_flg & 0x9) && !(sys->cb_flg & 0x1)) {
        printf("test: teleport to %d/%d/%d at game frame %d\n", a, b, c, clock);
        teleport(a, b, c);
        last = clock;
        tp = tp[n] == ',' ? tp + n + 1 : NULL;
    }
    {   /* RECVX_TP_NAME="frame:entry:room name,...": the teleport lists' room of that name (as the menus show it,
           e.g. "Prison: Obs. R"), then the menus' Teleport; frames as RECVX_TELEPORT. RECVX_TP_LIST=1 logs the lists. */
        static const char *tn = (const char*)-1, *tl;
        char want[64], nm[96], en[96];
        if (tn == (const char*)-1) { tn = getenv("RECVX_TP_NAME"); tl = getenv("RECVX_TP_LIST"); }
        if (tl && tp_count() && g_cheat.rooms[1][0]) {   /* with the game's names */
            for (int i = 0; i < tp_count(); i++) {
                const TpRoom* r = tp_room(i);
                printf("rooms: %d/%02d %s | %s | %s", r->stg, r->rom, r->file, tp_stage_name(r->stg), tp_room_name(i, 0, nm, sizeof(nm)));
                for (int p = 0; p < tp_entries(i); p++) printf(" | %s", tp_entry_name(i, p, en, sizeof(en)));
                printf("\n");
            }
            tl = NULL;
        }
        if (tn && tp_count() && sscanf(tn, "%d:%d:%n", &f, &c, &n) == 2 && clock >= f + last && !(sys->gm_flg & 0x9) && !(sys->cb_flg & 0x1)) {
            int i, len = (int)strcspn(tn + n, ",");
            snprintf(want, sizeof(want), "%.*s", len, tn + n);
            for (i = 0; i < tp_count() && strcmp(tp_room_name(i, 0, nm, sizeof(nm)), want); i++) {}
            if (i < tp_count()) {
                g_cheat.tp_stg = tp_room(i)->stg; g_cheat.tp_rom = tp_room(i)->rom; g_cheat.tp_pos = c;
                __atomic_or_fetch(&g_cheat.request, CHEAT_TELEPORT, __ATOMIC_ACQ_REL);
                printf("test: teleport to '%s' (%d/%d, %s) at game frame %d\n", want, g_cheat.tp_stg, g_cheat.tp_rom, tp_entry_name(i, c, en, sizeof(en)), clock);
            } else printf("test: no room named '%s' in the teleport list\n", want);
            last = clock;
            tn = tn[n + len] == ',' ? tn + n + len + 1 : NULL;
        }
    }
    if (ef && sscanf(ef, "%d:%d:%d:%d", &f, &a, &b, &c) == 4) {
        if (ef_slot < 0 && clock >= f) {
            POINT pt = { plp->px, plp->py, plp->pz, 0, 0, 0 };
            ef_slot = bhSetEffectEvt(a, &pt, (unsigned short)b, 0, 0);
            ef_frame = clock;
            printf("test: effect %d type %d -> slot %d at game frame %d\n", a, b, ef_slot, clock);
            if (ef_slot < 0) ef = NULL;
        } else if (ef_slot >= 0 && clock >= ef_frame + 2) {
            eff[ef_slot].mode1 = (unsigned char)c;
            ef = NULL;
        }
    }
    {   /* RECVX_RETRY="frame,...": a retry point here (as the scripts' ReTryPointSet): the game over's continue resumes in this room */
        static const char* rt = (const char*)-1;
        if (rt == (const char*)-1) rt = getenv("RECVX_RETRY");
        if (rt && *rt && clock >= atoi(rt) && !(sys->cb_flg & 0x1)) {
            bhPushGameData();
            printf("test: retry point in %d/%d at game frame %d\n", sys->stg_no, sys->rom_no, clock);
            rt = strchr(rt, ',') ? strchr(rt, ',') + 1 : NULL;
        }
    }
    {   /* RECVX_TEST_TYPEWRITER="frame,...": the typewriter's save screen at that game frame (as its ink ribbon question's yes) */
        static const char* tw = (const char*)-1;
        if (tw == (const char*)-1) tw = getenv("RECVX_TEST_TYPEWRITER");
        if (tw && *tw && clock >= atoi(tw) && !(sys->cb_flg & 0x1)) {
            sys->cb_flg |= 0x200000;
            printf("test: typewriter in %d/%d at game frame %d\n", sys->stg_no, sys->rom_no, clock);
            tw = strchr(tw, ',') ? strchr(tw, ',') + 1 : NULL;
        }
    }
    {   /* RECVX_TEST_COMPUTER="frame:n,...": computer terminal n at that game frame (as the room scripts' EvtComSet n) */
        static const char* ct = (const char*)-1;
        if (ct == (const char*)-1) ct = getenv("RECVX_TEST_COMPUTER");
        if (ct && sscanf(ct, "%d:%d%n", &f, &a, &n) == 2 && clock >= f && !(sys->cb_flg & 0x1)) {
            sys->com_num = a;
            sys->cb_flg |= 0x400000;
            printf("test: computer %d in %d/%d at game frame %d\n", a, sys->stg_no, sys->rom_no, clock);
            ct = ct[n] == ',' ? ct + n + 1 : NULL;
        }
    }
    {   /* RECVX_HP="frame:hp,...": our health set at that game frame (a quick game over) */
        static const char* hp = (const char*)-1;
        if (hp == (const char*)-1) hp = getenv("RECVX_HP");
        if (hp && sscanf(hp, "%d:%d%n", &f, &a, &n) == 2 && clock >= f) {
            plp->hp = a;
            printf("test: health %d at game frame %d\n", a, clock);
            hp = hp[n] == ',' ? hp + n + 1 : NULL;
        }
    }
    if (ap && sscanf(ap, "%d:%d%n", &f, &a, &n) == 2 && clock >= f) {
        printf("test: appear as %d at game frame %d\n", a, clock);
        g_coop.chr = a;
        ap = ap[n] == ',' ? ap + n + 1 : NULL;
    }
    spawn_frame(clock);   /* RECVX_SPAWN (pc/glue/spawn_glue.c) */
    if (eq && sscanf(eq, "%d:%d:%d%n", &f, &a, &b, &n) == 3 && clock >= f && sys->mn_mode0 == 0 && plp->mnwP) {
        printf("test: equip item %d at game frame %d\n", a, clock);
        inv[2] = (unsigned int)a << 16 | b;
        inv[0] = 0;
        plp->wpnr_no = 0;
        swork.pip = inv;
        WeaponSet(2, 2);   /* weapon number from the item (incl. its ammo kind), slot, model load */
        for (eq += n, c = 3; c < 12 && sscanf(eq, ",%d:%d%n", &a, &b, &n) == 2; eq += n) inv[c++] = (unsigned int)a << 16 | b;
        eq = *eq == ';' ? eq + 1 : NULL;   /* "...;frame:item:count": equip another later (a weapon switch) */
    }
}

/* RECVX_ITEM_TRACE=1: the inventory (equipped index, lighter slot, slots) and the two security
   boxes (sys->itm[192], [224]: the metal detector's trays in 0/9) whenever a word changes, with
   the box screen's cursors (listcsr_0/listcsr_2, ips1); by presented frame, as RECVX_INPUT */
uint64_t gs_frame_count(void);
static void item_trace(const unsigned int* inv)
{
    static int on = -1;
    static unsigned int seen[48];
    unsigned int now[48];
    int c;
    if (on < 0) on = getenv("RECVX_ITEM_TRACE") != NULL;
    if (!on) return;
    if (sys->stg_no == 0 && sys->rom_no == 9) {   /* the detector's script: floor zone, rm_flg bit 2 (metal carried), ev_flg bit 53 (gone off) */
        static unsigned int det = ~0u;
        unsigned int d = sys->flr_idx << 2 | !!(sys->rm_flg & 0x20000000) << 1 | !!(sys->ev_flg[1] & 0x400);
        if (d != det)
            printf("detector: frame %u floor %u metal %u alarm %u\n", (unsigned)gs_frame_count(), d >> 2, d >> 1 & 1, d & 1);
        det = d;
    }
    for (c = 0; c < 16; c++) { now[c] = inv[c]; now[16 + c] = sys->itm[192 + c]; now[32 + c] = sys->itm[224 + c]; }
    if (!memcmp(now, seen, sizeof(now))) return;
    memcpy(seen, now, sizeof(now));
    printf("items: frame %u room %d/%d cursor %u/%u box cursor %d; equipped %u", (unsigned)gs_frame_count(), sys->stg_no, sys->rom_no,
           swork.listcsr_0, swork.listcsr_2, (int)swork.ips1, now[0]);
    for (c = 1; c < 48; c++) {
        if (!(c & 15)) printf("\n  box %d:", c == 16 ? 192 : 224);
        else if (c == 1) printf("\n  inventory:");
        if (now[c] >> 16) printf(" [%d] %s(%u) %u", c & 15, g_cheat.names[(now[c] >> 16) & 0xFF], (now[c] >> 16) & 0xFF, now[c] & 0xFFFF);
        else if (now[c]) printf(" [%d] junk %#x", c & 15, now[c]);
        else if (c < 16) printf(" [%d] -", c);
    }
    printf("\n");
}

void cheat_frame(void)
{
    unsigned int* inv;
    int max_hp, slots, i, req;

    if (!sys || !plp) return;
    inv = &sys->itm[sys->ply_id * 16];
    max_hp = sys->gm_mode == 2 ? 320 : 160;
    slots = (sys->gm_flg & 0x8000000) ? 10 : 8;
    if (sys->mes_ip) read_names();
    item_trace(inv);
    if (sys->tk_flg && (sys->sp_flg & 0x2)) scripted(inv);   /* in game */

    g_cheat.hp = plp->hp;
    g_cheat.stg = sys->stg_no; g_cheat.rom = sys->rom_no; g_cheat.pos = sys->pos_no;
    g_cheat.player = sys->ply_id;
    g_cheat.slots = slots;
    for (i = 0; i < 12; i++) g_cheat.inv[i] = inv[i];

    if (g_cheat.inf_health && plp->hp < max_hp) plp->hp = max_hp;

    req = __atomic_exchange_n(&g_cheat.request, 0, __ATOMIC_ACQ_REL);
    if (!req) return;
    if (req & CHEAT_HEAL) { plp->hp = max_hp; g_cheat.result = "healed"; }
    if (req & CHEAT_GIVE) {
        g_cheat.result = "inventory full";
        for (i = 2; i < 2 + slots; i++)
            if (!(inv[i] >> 16)) {
                inv[i] = (unsigned int)g_cheat.give_id << 16 | (g_cheat.give_count & 0xFFFF);
                g_cheat.result = "item added";
                break;
            }
    }
    if (req & CHEAT_SPAWN) g_cheat.result = spawn_enemy(g_cheat.spawn_idx);
    if (req & CHEAT_TELEPORT) {
        /* the game's own door transition (door type 255 skips the animation), under the
           conditions the door trigger checks (hitchk.c) */
        if ((sys->gm_flg & 0x9) || (sys->cb_flg & 0x1)) g_cheat.result = "can't teleport now";
        else {
            teleport(g_cheat.tp_stg, g_cheat.tp_rom, g_cheat.tp_pos);
            g_cheat.result = "teleporting";
        }
    }
}
