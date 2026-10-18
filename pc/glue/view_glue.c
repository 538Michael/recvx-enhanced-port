/* Re-derive the game's culling volumes when the hor+ widescreen factor changes. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_NaView.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_NaGraphics3D.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_Vu1Strip.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_NaMatrix.h"
#include <math.h>

extern float fVu1Projection;
extern float _fNaViwClipNear, _fNaViwClipFar;
extern sceVu0FMATRIX ClipMatrix2;
extern NJS_SCREEN _nj_screen_;

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
#include <string.h>

uint32_t pc_cur_mesh, pc_cur_strip;   /* identity of what is being drawn (native_glue.c) */
int pc_margin_mesh;   /* the mesh being drawn shows only in the hor+ margins (native_glue.c); 2: in front of the player */
static int margin_next;
void pc_margin_next(int front) { margin_next = front ? 2 : 1; }
static void take_view(void);
static int view_taken;
void pc_mesh_begin(const void* model)
{
    if (!view_taken) take_view();   /* the first mesh of a frame: the camera it is drawn with */
    pc_cur_mesh = (uint32_t)(uintptr_t)model; pc_cur_strip = 0; stream_tag(pc_cur_mesh);
    pc_margin_mesh = margin_next; margin_next = 0;
}
void pc_mesh_end(void) { pc_cur_mesh = 0; pc_margin_mesh = 0; stream_tag(0); }
void pc_mesh_strip(const void* strip) { pc_cur_strip = (uint32_t)(uintptr_t)strip; stream_strip(pc_cur_strip); }

/* effect.c draw loops: the following output is this effect (O_WRK; NULL: none). The tag is odd
   (meshes are aligned pointers): its eff[] slot and that slot's life, which starts again when
   the slot holds another effect or skipped a frame, so the renderer pairs an effect only with
   itself in the previous frame. Its polygons are numbered as strips (native_glue.c), or carry
   the tag after their vertices when drawn from the ordering table (ps2_dummy.c Ps2AddOT). */
void pc_effect_tag(const void* op)
{
    static uint32_t life[512], sig[512];
    static uint64_t seen[512];
    const O_WRK* o = (const O_WRK*)op;
    uint32_t slot, s;
    uint64_t now = stream_game_frames();
    if (!o || o < eff || o >= eff + 512) { pc_mesh_end(); return; }
    slot = (uint32_t)(o - eff);
    s = ((uint32_t)o->id | (uint32_t)o->type << 16 | (uint32_t)o->lkono << 24) ^ (uint32_t)(uintptr_t)o->lkwkp;
    if (sig[slot] != s || seen[slot] + 1 < now) life[slot]++;   /* seen == now: drawn twice this frame */
    sig[slot] = s; seen[slot] = now;
    pc_cur_mesh = 1u | slot << 1 | life[slot] << 10; pc_cur_strip = 0;
    stream_tag(pc_cur_mesh);
}
uint64_t pc_effect_poly(void) { return pc_cur_mesh & 1 ? pc_cur_mesh | (uint64_t)pc_cur_strip++ << 32 : 0; }

int pc_is_room_object(const void* obj)
{
    const NJS_CNK_OBJECT* o = (const NJS_CNK_OBJECT*)obj;
    return rom->mdl.objP && o >= rom->mdl.objP && o < rom->mdl.objP + rom->mdl.obj_num;
}

/* hor+ widescreen with a fixed camera (ps2_NinjaCnk.c pc_show_hidden): is a room part the cut
   hides drawn anyway? With the native renderer it is, cut to the margins (pc_margin_next,
   stream.c native_draw), unless it would cover the view: its bounding sphere reaches the
   camera or the line from the camera to the player (a wall the cut looks through), or it lies
   in front of the player and reaches into the 4:3 view (native_glue.c pc_margin_reaches_43: it
   would end in a straight edge at the 4:3 border). Without it, only a part entirely outside
   the 4:3 view. Called with the part's model matrix current. */
int pc_margin_part(const void* obj)
{
    const NJS_CNK_MODEL* m = ((const NJS_CNK_OBJECT*)obj)->model;
    NJS_POINT3 ctr = m->center, c, pw, p;
    float z, r, sx, half, t, pl, dx, dy, dz;
    if (m->r <= 0) return 0;
    njCalcPoint(NULL, &ctr, &c);
    if (c.z - m->r < _fNaViwClipNear) return 0;
    if (pc_native_3d()) {
        pw.x = plp->px; pw.y = plp->py; pw.z = plp->pz;
        njCalcPoint(cam.mtx, &pw, &p);
        pl = p.x * p.x + p.y * p.y + p.z * p.z;
        t = pl > 0 ? (c.x * p.x + c.y * p.y + c.z * p.z) / pl : 0;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        dx = c.x - t * p.x; dy = c.y - t * p.y; dz = c.z - t * p.z;
        if (dx * dx + dy * dy + dz * dz < m->r * m->r) return 0;
        pc_margin_next(c.z < p.z);
        return 1;
    }
    z = fabsf(_nj_screen_.dist / c.z);
    r = m->r * z * fNaViwAspectW;
    sx = c.x * z;
    half = _nj_screen_.w / 2.0f;
    return sx - r > half || sx + r < -half;
}

/* Frames of different scenes or camera cuts are never blended. Taken when the frame's first mesh
   is drawn (else at its end), which is the cut the picture shows: game.c bhCheckCut changes the
   cut after drawing, so at the frame's end it is the next frame's. The scene: stage, room and,
   unless the third-person camera is on, the cut (ncut, camver), the event or fixed camera mode
   (cam.flg 0x46) with its event camera, and first person / scope. The view: the eye and view
   direction of cam.mtx, which bhControlCamera computed for the drawing. */
static uint64_t scene_now;
static float view_now[6];
static void take_view(void)
{
    uint32_t cut = 0xFFFFFFFFu;
    if (!tps_cam_active)
        cut = (uint32_t)(cam.ncut & 0xFF) | (uint32_t)(cam.camver & 1) << 8 | (cam.flg & 0x46) << 9 | (sys->gm_flg & 0x40) << 10 |
              (cam.flg & 0x2 ? (uint32_t)(cam.evc_no & 0xFF) << 24 : 0);
    scene_now = (uint64_t)(unsigned char)sys->stg_no << 40 | (uint64_t)(unsigned char)sys->rom_no << 32 | cut;
    memset(view_now, 0, sizeof(view_now));   /* none: not compared (the third-person camera moves fast but never jumps) */
    if (cam.mtx && !tps_cam_active) {   /* row vectors: view = world * R + T, so the eye is -T * R^T, the view axis R's third column */
        const float* m = &(*cam.mtx)[0];
        for (int r = 0; r < 3; r++) {
            view_now[r] = -(m[12] * m[r * 4] + m[13] * m[r * 4 + 1] + m[14] * m[r * 4 + 2]);
            view_now[3 + r] = m[r * 4 + 2];
        }
    }
    view_taken = 1;
}
/* stream_submit: the scene and view (eye, direction) of the frame being submitted */
uint64_t pc_frame_view(float view[6])
{
    if (!view_taken) take_view();
    view_taken = 0;
    memcpy(view, view_now, sizeof(view_now));
    return scene_now;
}
