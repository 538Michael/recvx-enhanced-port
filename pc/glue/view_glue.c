/* Re-derive the game's culling volumes when the hor+ widescreen factor changes. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_NaView.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_NaGraphics3D.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_Vu1Strip.h"

extern float fVu1Projection;
extern float _fNaViwClipNear, _fNaViwClipFar;
extern sceVu0FMATRIX ClipMatrix2;

void pc_view_changed(void)
{
    extern float fNaViwHalfW;
    extern NJS_SCREEN _nj_screen_;
    _Make_ClipMatrix(ClipMatrix2, fVu1Projection, _fNaViwClipNear, _fNaViwClipFar);
    fNaViwHalfW = _nj_screen_.w / 2.0f * pc_hor_scale();   /* model culling (njCnkDrawModelLocal) */
    Ps2CalcScreenCone();
}

/* enhanced frame interpolation: tags for the renderer (gs/stream.c) */
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../src/gs/stream.h"

uint32_t pc_cur_mesh, pc_cur_strip;   /* identity of what is being drawn (native_glue.c) */
void pc_mesh_begin(const void* model) { pc_cur_mesh = (uint32_t)(uintptr_t)model; pc_cur_strip = 0; stream_tag(pc_cur_mesh); }
void pc_mesh_end(void) { pc_cur_mesh = 0; stream_tag(0); }
void pc_mesh_strip(const void* strip) { pc_cur_strip = (uint32_t)(uintptr_t)strip; stream_strip(pc_cur_strip); }

int pc_is_room_object(const void* obj)
{
    const NJS_CNK_OBJECT* o = (const NJS_CNK_OBJECT*)obj;
    return rom->mdl.objP && o >= rom->mdl.objP && o < rom->mdl.objP + rom->mdl.obj_num;
}

/* frames of different scenes or camera cuts are never blended */
uint32_t pc_scene_id(void)
{
    return (uint32_t)(unsigned char)sys->stg_no << 24 | (uint32_t)(unsigned char)sys->rom_no << 16 | (tps_cam_active ? 0 : cam.ncut & 0xFFFF);
}
