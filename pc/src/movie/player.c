/*
 * Movie player: streams a PSS file from the disc image, decodes video on demand (frames are
 * requested by display index) and queues the PCM audio to the output device.
 * Frames are delivered in the IPU's output format, RGBA32 macroblocks (16x16) ordered
 * column by column, which is what the game's own upload code (setImageTag) expects.
 */
#include "../ps2_platform.h"
#include "../audio/audio.h"
#include "movie.h"
#include "mpeg2.h"
#include "pss.h"
#include <stdlib.h>
#include <string.h>

#define QCAP 64
static Pss* pss;
static Mpeg2* dec;
static uint8_t* es; static size_t es_len, es_cap;
static uint8_t* queue[QCAP];
static int q_head, q_count, head_index, eof, width, height;
static uint32_t file_lsn, file_size, file_off;
static int audio_open;
/* the frame handed to the game: static so it is inside the emulated EE address window (DMA) */
static uint8_t display[(720 / 16) * (576 / 16) * 1024] __attribute__((aligned(64)));

static void on_frame(const Mpeg2Frame* f, void* user)
{
    int mbw = (f->width + 15) / 16, mbh = (f->height + 15) / 16;
    uint8_t* out;
    (void)user;
    if (q_count == QCAP || (size_t)mbw * mbh * 1024 > sizeof(display)) { host_log("movie: frame dropped\n"); return; }
    width = f->width; height = f->height;
    out = malloc((size_t)mbw * mbh * 1024);
    for (int mx = 0; mx < mbw; mx++)
        for (int my = 0; my < mbh; my++) {
            uint8_t* o = out + ((size_t)mx * mbh + my) * 1024;
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++, o += 4) {
                    int px = mx * 16 + x, py = my * 16 + y;
                    int Y = f->y[py * f->y_stride + px] - 16;
                    int U = f->cb[(py / 2) * f->c_stride + px / 2] - 128, V = f->cr[(py / 2) * f->c_stride + px / 2] - 128;
                    int r = (298 * Y + 409 * V + 128) >> 8, g = (298 * Y - 100 * U - 208 * V + 128) >> 8, b = (298 * Y + 516 * U + 128) >> 8;
                    o[0] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
                    o[1] = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
                    o[2] = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
                    o[3] = 0x80;
                }
        }
    queue[(q_head + q_count++) % QCAP] = out;
}

static void on_video(void* user, const uint8_t* d, size_t n)
{
    (void)user;
    if (es_len + n > es_cap) { es_cap = (es_len + n) * 2; es = realloc(es, es_cap); }
    memcpy(es + es_len, d, n);
    es_len += n;
}

static void on_audio(void* user, const int16_t* s, size_t frames, int rate)
{
    (void)user;
    if (!audio_open) { audio_movie_open(rate); audio_movie_pause(1); audio_open = 1; }
    audio_movie_put(s, frames);
}

/* demux + decode the next chunk of the file; 0 when everything has been decoded */
static int pump(void)
{
    static uint8_t chunk[16 * 2048];
    uint32_t n;
    size_t used;
    if (eof) return 0;
    if (file_off >= file_size) {
        mpeg2_decode(dec, es, es_len, 1);
        es_len = 0;
        eof = 1;
        return 1;
    }
    n = file_size - file_off < sizeof(chunk) ? file_size - file_off : (uint32_t)sizeof(chunk);
    if (!cdvd_read_sectors(file_lsn + file_off / 2048, (n + 2047) / 2048, chunk)) { file_off = file_size; return 1; }
    file_off += n;
    if (!pss_feed(pss, chunk, n)) file_off = file_size;
    used = mpeg2_decode(dec, es, es_len, 0);
    memmove(es, es + used, es_len - used);
    es_len -= used;
    return 1;
}

int movie_open(uint32_t lsn, uint32_t size)
{
    PssSink sink = { on_video, on_audio, NULL };
    movie_close();
    file_lsn = lsn; file_size = size; file_off = 0;
    pss = pss_create(&sink);
    dec = mpeg2_create(on_frame, NULL);
    while (!q_count && pump()) {}
    return q_count > 0;
}

void movie_close(void)
{
    while (q_count) { free(queue[q_head]); q_head = (q_head + 1) % QCAP; q_count--; }
    q_head = 0; head_index = 0; eof = 0; es_len = 0;
    mpeg2_destroy(dec); dec = NULL;
    pss_destroy(pss); pss = NULL;
    if (audio_open) audio_movie_close();
    audio_open = 0;
}

void movie_pause(int paused) { if (audio_open) audio_movie_pause(paused); }

const uint8_t* movie_frame(int index, int* w, int* h)
{
    if (!dec) return NULL;
    while (head_index + q_count <= index && pump()) {}
    while (q_count > 1 && head_index < index) { free(queue[q_head]); q_head = (q_head + 1) % QCAP; q_count--; head_index++; }
    if (!q_count || index >= head_index + q_count) return NULL;   /* past the end */
    *w = width; *h = height;
    memcpy(display, queue[q_head], (size_t)((width + 15) / 16) * ((height + 15) / 16) * 1024);
    return display;
}
