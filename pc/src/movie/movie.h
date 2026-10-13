/* PSS movie playback (platform side); driven by pc/glue/movie_glue.c. */
#pragma once
#include <stdint.h>
int  movie_open(uint32_t lsn, uint32_t size);   /* 1 if at least one frame decodes */
void movie_close(void);
void movie_pause(int paused);
/* Frame `index` (display order, 29.97 Hz) in IPU layout (RGBA32 macroblocks, column-major);
   NULL past the end. Earlier frames are released. */
const uint8_t* movie_frame(int index, int* width, int* height);
