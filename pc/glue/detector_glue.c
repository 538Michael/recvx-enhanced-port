/* Co-op game rule "Metal detector: Skipped" (the host's OPT_SKIP_DETECTOR, docs/COOP.md "Metal detector"): the
   prison's metal detector room (0/9) as if every pass were clean. Its script: entering the zone before a gate (floor 3,
   the north side with security box 192 at trigger 11; floor 4, the south-west side with box 224 at trigger 12) runs
   event 0, whose PlItemCheck list of metal items sets rm_flg bit 2; with that bit a gate (floor 5, 6) sounds the alarm
   (ev_flg bit 53), drops the shutters and clears ky_flg bit 15 until the release switch. A clean pass
   sets no flag: the shutters are open and ky_flg bit 15 set from the room's entry. With the rule, in a session:
   - event.c bhPlItemCheck finds no metal in 0/9 (every PlItemCheck there is event 0's);
   - every frame before scd1 (event.c bhControlEvent): an empty security box has its trigger off, as the script's
     EtcAtariSet would; a box still holding items stays usable until it is empty, so nothing left there is lost. With
     box 192 empty, coming back to its side does not play the reminder "Take the metal items you placed into the
     security box with you" (rm_flg bit 10 on floor 3: events 30, 32).
   An alarm that went off before (a save) is released at its switch as in the original. Game thread. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../src/host/coop.h"

static int detector_room(void) { return coop_on && sys->stg_no == 0 && sys->rom_no == 9; }

static int box_empty(int first)
{
    int i;
    for (i = first; i < first + 32; i++)
        if (sys->itm[i] >> 16 & 0xFF) return 0;
    return 1;
}

int pc_detector_skipped(void) { return detector_room() && coop_skip_detector(); }

void pc_detector_scd1(void)
{
    int k, skip = pc_detector_skipped();
    if (!detector_room()) return;
    for (k = 0; k < 2; k++) {   /* the script never turns these two off: on is the room's own state */
        ATR_WORK* t = &rom->etcp[11 + k];
        if (skip && box_empty(192 + 32 * k)) t->flg &= ~1;
        else t->flg |= 1;
    }
    if (skip && box_empty(192) && sys->flr_idx == 3) sys->rm_flg &= ~0x200000;   /* rm_flg bit 10 */
}
