/* Subtitles (docs/USER_GUIDE.md "Subtitles"): movies' <mods>/subtitles/MV_nnn.srt, read when the
   movie opens (pc/glue/movie_glue.c initAll, game thread), and voice clips' <mods>/subtitles/
   <archive>.srt (VOICE1.srt: cue number = clip number, times from the clip's start), read at the
   archive's first clip (pc/src/audio/adx.c ADXT_StartAfs). A language's <code>.json replaces their
   text by key. The overlay draws the lines of the movie frame on screen (gs_gl.c gs_movie_frame) or
   of the clips playing (overlay.cpp, render thread). A cue with a speaker ("<v Claire>" in the SRT)
   starts with \x01<id>\x1f<name shown>\x02; the overlay shows or drops it. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int id, start_ms, end_ms;   /* id: the block's number, -1 none */
    char* text;                 /* UTF-8, lines split by '\n' */
    char* who;                  /* the speaker ("<v Claire>"), NULL none */
} SubCue;
typedef struct { char* key; char* val; } SubPair;

/* SRT text to cues sorted by start; the count, *out for subs_free. A block it cannot read is
   skipped and reported to log (NULL: not reported) with its first line. */
int  subs_parse(const char* text, SubCue** out, void (*log)(int line, const char* msg));
void subs_free(SubCue* c, int n);
/* a JSON object's string members, sorted by key (others skipped); the count, -1 if not an object */
int  subs_json_parse(const char* text, SubPair** out);
void subs_pairs_free(SubPair* v, int n);
const char* subs_pairs_get(const SubPair* v, int n, const char* key);
/* any thread: the language, <mods>/subtitles/<code>.json ("" English: the SRTs' own text) */
void subs_set_language(const char* code);
/* the languages there are (<mods>/subtitles/<code>.json): codes and menu names ("_name", ASCII), sorted by name */
int  subs_languages(char (*codes)[16], char (*names)[64], int max);
/* movies, game thread */
int  subs_movie_open(const char* disc_name);           /* "MV_016.PSS;1": <mods>/subtitles/MV_016.srt; its cues, 0 without */
int  subs_load_text(const char* movie, const char* text);   /* the cues of SRT text (NULL: none) for movie (subs_movie_open, tests) */
void subs_movie_close(void);
/* render thread: the lines shown at movie frame `frame` (29.97 Hz) into buf; their length, 0 none */
int  subs_text_at(int frame, char* buf, int n);
/* voice clips, game thread: clip fid of archive ("VOICE1.AFS") started on ADXT slot `slot` at now_ns
   (SDL_GetTicksNS), dur_ms long; stopped (or ended early) */
void subs_voice_start(int slot, const char* archive, int fid, int dur_ms, uint64_t now_ns);
void subs_voice_stop(int slot);
int  subs_voice_load_text(const char* archive, const char* text);   /* an archive's cues from SRT text (subs_voice_start, tests) */
/* render thread: the lines of the clips playing at now_ns into buf; their length, 0 none */
int  subs_voice_text(uint64_t now_ns, char* buf, int n);

#ifdef __cplusplus
}
#endif
