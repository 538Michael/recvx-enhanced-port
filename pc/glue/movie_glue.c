/*
 * Replacement for ps2_MovieFunc.c (libmpeg + IPU + IOP audio streaming). Compiled with the
 * decomp's types.
 *
 * The PSS is decoded in software (pc/src/movie) into the IPU's output layout; the frame is
 * then uploaded and drawn by the game's own setImageTag()/vbrank_draw() (copied verbatim
 * from ps2_MovieFunc.c), so size and position follow mwPlySetDispSize/Pos as on the PS2.
 * Sofdec status goes PREP -> PLAYING -> PLAYEND; frames are chosen from the elapsed
 * vblanks (29.97 Hz = every second NTSC field) so video keeps pace with the audio.
 */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_MovieFunc.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_MovieWork.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_dummy.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_loadtim2.h"

int  movie_open(uint32_t lsn, uint32_t size);
void movie_close(void);
void movie_pause(int paused);
const uint8_t* movie_frame(int index, int* width, int* height);
void audio_movie_volume(float gain);
uint64_t ps2_vblank_count(void);

#define MOVIE_STAT_PREP    1
#define MOVIE_STAT_PLAYING 2
#define MOVIE_STAT_PLAYEND 3

int movie_draw;
StrFile infile;
VoBuf voBuf;
u_long128 test_tag[1400] __attribute__((aligned(64)));
RMI_WORK rmi;
MDSIZE_WORK mdSize;
u_long128 new_tags[64] __attribute__((aligned(64)));
static int __image_w__;
static int __image_h__;

static uint64_t start_vblank, paused_at, paused_total;
static int skip_movies, skip_frames, frame_shown = -1;

void initAll(void)
{
    memset(&rmi, 0, sizeof(rmi));
    rmi.uiContFlag = 0x1;
    rmi.iMovieState = MOVIE_STAT_PREP;
    skip_movies = getenv("RECVX_SKIP_MOVIES") != NULL;   /* testing aid: one second per movie */
    frame_shown = -1;
    paused_total = 0; paused_at = 0;
    if (!skip_movies && !movie_open(infile.fp.lsn, infile.fp.size)) printf("movie: cannot decode %u\n", infile.fp.lsn);
}

void readMpeg(void)
{
    uint64_t now = ps2_vblank_count();
    int index, w, h;
    const uint8_t* img;

    if (rmi.iMovieState == MOVIE_STAT_PREP) {
        rmi.iMovieState = MOVIE_STAT_PLAYING;
        rmi.iMovieFrame = Ps2_vcount;
        start_vblank = now;
        skip_frames = 0;
        movie_pause(0);
    }
    if (rmi.uiContFlag & 2) {   /* paused by the game */
        if (!paused_at) { paused_at = now; movie_pause(1); }
        movie_draw = 1;
        return;
    }
    if (paused_at) { paused_total += now - paused_at; paused_at = 0; movie_pause(0); }

    movie_draw = 1;
    if (skip_movies) {
        /* counted in game frames, not vblanks: a test run then reaches the same game frame
           after the movie however its frames were timed */
        if (++skip_frames >= 30) rmi.iMovieState = MOVIE_STAT_PLAYEND;
        return;
    }
    index = (int)((now - start_vblank - paused_total) / 2);
    img = movie_frame(index, &w, &h);
    if (!img) { rmi.iMovieState = MOVIE_STAT_PLAYEND; return; }
    voBuf.data = (VoData*)img;
    __image_w__ = w;
    __image_h__ = h;
    frame_shown = index;
}

void termAll(void)
{
    movie_close();
    rmi.iMovieState = 0;
}

void changeInputVolume(u_int val) { audio_movie_volume((val & 0xFFFF) / 16383.0f); }
int sendToIOP(int dst, u_char* src, int size) { (void)dst; (void)src; (void)size; return 0; }

/* ---- from ps2_MovieFunc.c ---- */

void setImageTag(u_int *tags, void *image)
{
    int mbx = __image_w__ / 16;
    int mby = __image_h__ / 16;
    int i;
    u_long* tag;
    const u_long giftag[2] = { SCE_GIF_SET_TAG(0, 0, 0, 0, 0, 1), 0x000000000000000EL };

    if (!image || skip_movies) return;
    for (i = 0; i < mbx; i++)
    {
        D2_SyncTag();

        tag = (void*)((u_int)tags | UNCBASE);

        *tag++ = DMAcnt | 0x3;
        *tag++ = 0;

        *tag++ = giftag[0] | 0x2;
        *tag++ = giftag[1];

        *tag++ = SCE_GS_SET_BITBLTBUF(0, 0, 0, 909312 / 64, 5, 0);
        *tag++ = SCE_GS_BITBLTBUF;

        *tag++ = SCE_GS_SET_TRXREG(1 * 16, mby * 16);
        *tag++ = SCE_GS_TRXREG;

        *tag++ = DMAcnt | 0x4;
        *tag++ = 0;

        *tag++ = giftag[0] | 0x2;
        *tag++ = giftag[1];

        *tag++ = SCE_GS_SET_TRXPOS(0, 0, i * 16, 0, 0);
        *tag++ = SCE_GS_TRXPOS;

        *tag++ = SCE_GS_SET_TRXDIR(0);
        *tag++ = SCE_GS_TRXDIR;

        *tag++ = SCE_GIF_SET_TAG((mby * 1024) / 16, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_IMAGE, 0);
        *tag++ = 0;

        /* PC: the image lives above 16 MB of the address window; keep 28 address bits (was 0xFFFFFF) */
        *tag++ = (unsigned long)((unsigned int)image & 0xFFFFFFF) << 32 | 0x30000000L | ((mby * 1024) / 16);
        *tag++ = 0;

        image = (unsigned char*)image + (mby * 1024);

        SyncPath();

        loadImage(tags);
    }

    D2_SyncTag();
}

void vbrank_draw(void)
{
    unsigned long* tag;
    const unsigned long giftag_eop[2] = { SCE_GIF_SET_TAG(0, 1, 0, 0, 0, 1), 0x000000000000000EL };
    int u, v;
    int y;
    int w, h;

    if (skip_movies || frame_shown < 0) return;
    D2_SyncTag();

    tag = (void*)((u_int)new_tags | UNCBASE);

    u = (__image_w__ - 1) * 16;
    v = (__image_h__ - 1) * 16;

    y = (mdSize.sDispY + 1824) * 16;

    w = mdSize.sWidth * 16;
    h = mdSize.sHeight * 16;

    *tag++ = DMAend | 0x9;
    *tag++ = 0;

    *tag++ = giftag_eop[0] | 0x8;
    *tag++ = giftag_eop[1];

    *tag++ = 0;
    *tag++ = SCE_GS_TEXFLUSH;

    *tag++ = SCE_GS_SET_TEX1_1(0, 0, 1, 1, 0, 0, 0);
    *tag++ = SCE_GS_TEX1_2;

    *tag++ = SCE_GS_SET_TEX0_2(14208, 5, SCE_GS_PSMCT32, 9, 9, 0, SCE_GS_DECAL, 0, SCE_GS_PSMCT32, 0, 0, 0);
    *tag++ = SCE_GS_TEX0_2;

    *tag++ = SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 0, 0, 1, 1, 0);
    *tag++ = SCE_GS_PRIM;

    *tag++ = SCE_GS_SET_UV(8, 8);
    *tag++ = SCE_GS_UV;

    *tag++ = SCE_GS_SET_XYZ2(27648, y, 0);
    *tag++ = SCE_GS_XYZ2;

    *tag++ = SCE_GS_SET_UV(u + 8, v + 8);
    *tag++ = SCE_GS_UV;

    *tag++ = SCE_GS_SET_XYZ2(w + 27648, y + h, 0);
    *tag++ = SCE_GS_XYZ2;

    SyncPath();

    loadImage(new_tags);

    D2_SyncTag();
}
