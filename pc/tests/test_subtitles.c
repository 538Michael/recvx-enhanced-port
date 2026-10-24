/* Subtitles (pc/src/movie/subtitles.c): the SRT reader, the lines of a movie frame and of the voice clips
   playing, the JSON reader and a language over the SRTs (files in ./subs_test_mods, removed after). */
#include "../src/movie/subtitles.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define T0 5000000000ull                  /* a clip's start (SDL_GetTicksNS) */
#define MS(x) ((uint64_t)(x) * 1000000)

void host_log(const char* fmt, ...) { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); }
static int write_file(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    fputs(text, f);
    fclose(f);
    return 1;
}
static int msgs, last_line;
static void log_line(int line, const char* m) { printf("  line %d: %s\n", line, m); msgs++; last_line = line; }

int main(void)
{
    SubCue* c;
    SubPair* v;
    char buf[256], codes[4][16], names[4][64];
    int n;

    /* a byte order mark, CRLF, tags, a cue without its number, '.' before the milliseconds, hours
       left out, a missing blank line, a block that is no cue, an end before its start, overlaps */
    n = subs_parse("\xEF\xBB\xBF" "1\r\n"
                   "00:00:01,000 --> 00:00:03,500\r\n"
                   "<i>Line one...</i>  \r\n"
                   "{\\an8}line two?\r\n"
                   "\r\n"
                   "00:00:05.25 --> 00:00:06.5 X1:10 X2:20\r\n"
                   "Third!\r\n"
                   "3\r\n"
                   "01:02,000 --> 01:04,000\r\n"
                   "1999\r\n"
                   "\r\n\r\n"
                   "just some text\r\n"
                   "\r\n"
                   "5\r\n"
                   "00:00:09,000 --> 00:00:08,000\r\n"
                   "backwards\r\n"
                   "\r\n"
                   "6\n"
                   "00:00:02,000 --> 00:00:04,000\n"
                   "Over it\n"
                   "\n"
                   "7\n"
                   "00:00:20,000 --> 00:00:21,000\n"
                   "<i></i>\n",
                   &c, log_line);
    CHECK(n == 4 && msgs == 2 && last_line == 15);   /* lines 13 (no cue) and 15 (backwards) */
    CHECK(c[0].start_ms == 1000 && c[0].end_ms == 3500 && !strcmp(c[0].text, "Line one...\nline two?"));
    CHECK(c[1].start_ms == 2000 && c[1].end_ms == 4000 && !strcmp(c[1].text, "Over it"));   /* by start */
    CHECK(c[2].start_ms == 5250 && c[2].end_ms == 6500 && !strcmp(c[2].text, "Third!"));
    CHECK(c[3].start_ms == 62000 && c[3].end_ms == 64000 && !strcmp(c[3].text, "1999"));   /* a number that is text */
    CHECK(c[0].id == 1 && c[1].id == 6 && c[2].id == -1 && c[3].id == 3);   /* the blocks' numbers, -1 without */
    subs_free(c, n);
    CHECK(subs_parse("", &c, log_line) == 0 && !c);
    CHECK(subs_parse("1\n00:00:01,000 --> 00:00:02,000\n", &c, NULL) == 0);   /* no text */
    CHECK(subs_parse("x\ny", &c, NULL) == 0);

    /* the lines of a frame (29.97 Hz: frame 30 is 1001 ms); end times exclusive */
    CHECK(subs_text_at(30, buf, sizeof(buf)) == 0);   /* nothing loaded */
    CHECK(subs_load_text("MV_000", "1\n00:00:01,001 --> 00:00:02,002\nfirst\n\n2\n00:00:01,500 --> 00:00:03,000\nsecond\n") == 2);
    CHECK(subs_text_at(29, buf, sizeof(buf)) == 0 && !buf[0]);
    CHECK(subs_text_at(30, buf, sizeof(buf)) == 5 && !strcmp(buf, "first"));
    CHECK(subs_text_at(45, buf, sizeof(buf)) > 0 && !strcmp(buf, "first\nsecond"));   /* 1501 ms */
    CHECK(subs_text_at(60, buf, sizeof(buf)) > 0 && !strcmp(buf, "second"));          /* 2002 ms */
    CHECK(subs_text_at(45, buf, 9) == 5 && !strcmp(buf, "first"));                    /* whole cues only */
    CHECK(subs_text_at(90, buf, sizeof(buf)) == 0);
    subs_movie_close();
    CHECK(subs_text_at(30, buf, sizeof(buf)) == 0);

    /* voice clips: the cue number is the clip, times count from its start (wall clock in ns) */
    CHECK(subs_voice_load_text("VOICE1.AFS", "40\n00:00:00,000 --> 00:00:01,000\nforty\n\n"
                                             "12\n00:00:01,500 --> 00:00:03,000\nsecond half\n\n"
                                             "12\n00:00:00,000 --> 00:00:01,500\nTwelve!\n") == 3);
    subs_voice_start(1, "VOICE1.AFS", 12, 2500, T0);
    CHECK(subs_voice_text(T0 - 1, buf, sizeof(buf)) == 0);                                   /* not yet */
    CHECK(subs_voice_text(T0, buf, sizeof(buf)) > 0 && !strcmp(buf, "Twelve!"));
    CHECK(subs_voice_text(T0 + MS(1600), buf, sizeof(buf)) > 0 && !strcmp(buf, "second half"));
    CHECK(subs_voice_text(T0 + MS(2600), buf, sizeof(buf)) > 0 && !strcmp(buf, "second half"));   /* clip over (2.5 s), the line stays a moment */
    CHECK(subs_voice_text(T0 + MS(3000), buf, sizeof(buf)) == 0);                            /* its end */
    subs_voice_start(3, "VOICE1.AFS", 40, 300, T0);                                          /* a line longer than its clip: */
    CHECK(subs_voice_text(T0 + MS(850), buf, sizeof(buf)) > 0 && !strcmp(buf, "Twelve!\nforty"));   /* (slot 1's clip plays on) */
    CHECK(subs_voice_text(T0 + MS(900), buf, sizeof(buf)) > 0 && !strcmp(buf, "Twelve!"));   /* 0.6 s after the clip at most */
    subs_voice_stop(3);
    subs_voice_start(2, "VOICE1.AFS", 40, 1000, T0 + MS(1000));                              /* another slot, overlapping */
    CHECK(subs_voice_text(T0 + MS(1600), buf, sizeof(buf)) > 0 && !strcmp(buf, "second half\nforty"));
    subs_voice_stop(1);
    CHECK(subs_voice_text(T0 + MS(1600), buf, sizeof(buf)) > 0 && !strcmp(buf, "forty"));
    subs_voice_start(1, "VOICE1.AFS", 99, 1000, T0 + MS(1000));                              /* a clip without subtitles */
    subs_voice_start(0, "VOICE2.AFS", 12, 1000, T0 + MS(1000));                              /* an archive without a file */
    CHECK(subs_voice_text(T0 + MS(1600), buf, sizeof(buf)) > 0 && !strcmp(buf, "forty"));
    CHECK(subs_voice_load_text("VOICE1.AFS", NULL) == 0);                                    /* reloaded: its tracks end */
    CHECK(subs_voice_text(T0 + MS(1600), buf, sizeof(buf)) == 0);

    /* a speaker tag starts a cue's text: <v Name> */
    n = subs_parse("1\n00:00:01,000 --> 00:00:02,000\n<v Claire >A spoken line\nsecond line\n\n"
                   "2\n00:00:03,000 --> 00:00:04,000\nno speaker\n", &c, NULL);
    CHECK(n == 2 && c[0].who && !strcmp(c[0].who, "Claire") && !strcmp(c[0].text, "A spoken line\nsecond line") && !c[1].who);
    subs_free(c, n);

    /* JSON: a flat object of strings (escapes, \u, a surrogate pair); other members skipped */
    n = subs_json_parse("\xEF\xBB\xBF{ \"b\": \"x\\\"y\\\\z\\n\", \"a\": \"\\u00e3o \\ud83d\\ude00\", \"n\": 12, "
                        "\"o\": {\"k\": [1, \"}\"]}, \"c\": \"\" }", &v);
    CHECK(n == 3 && !strcmp(v[0].key, "a") && !strcmp(v[0].val, "\xC3\xA3o \xF0\x9F\x98\x80"));
    CHECK(!strcmp(subs_pairs_get(v, n, "b"), "x\"y\\z\n") && !*subs_pairs_get(v, n, "c") && !subs_pairs_get(v, n, "n"));
    subs_pairs_free(v, n);
    CHECK(subs_json_parse("{\"a\": \"x\" \"b\": \"y\"}", &v) == -1 && !v);   /* no comma */
    CHECK(subs_json_parse("[\"a\"]", &v) == -1 && subs_json_parse("{\"a\": \"\\ud83d\"}", &v) == -1);   /* not an object; a lone surrogate */
    CHECK(subs_json_parse(" {} ", &v) == 0);

    /* a language: <mods>/subtitles/xx.json over the SRTs' text, subtitle by subtitle, and the speakers' names */
    CHECK(SDL_CreateDirectory("subs_test_mods/subtitles"));
#ifdef _WIN32
    _putenv_s("RECVX_MODS", "subs_test_mods");
#else
    setenv("RECVX_MODS", "subs_test_mods", 1);
#endif
    CHECK(write_file("subs_test_mods/subtitles/VOICE9.srt", "1\n00:00:00,000 --> 00:00:01,000\n<v Claire>Hello.\n\n"
                                                            "1\n00:00:01,000 --> 00:00:02,000\n<v Claire>Second.\n\n"
                                                            "2\n00:00:00,000 --> 00:00:01,000\nUntranslated.\n"));
    CHECK(write_file("subs_test_mods/subtitles/MV_100.srt", "1\n00:00:00,000 --> 00:00:02,000\nA movie line.\n"));
    CHECK(write_file("subs_test_mods/subtitles/xx.json", "{ \"_name\": \"Testish\", \"name/Claire\": \"Clara\", "
                                                         "\"VOICE9/1/1\": \"Ol\\u00e1.\", \"MV_100/1\": \"Uma fala.\" }"));
    CHECK(subs_languages(codes, names, 4) == 1 && !strcmp(codes[0], "xx") && !strcmp(names[0], "Testish"));
    subs_set_language("xx");
    subs_voice_start(4, "VOICE9.AFS", 1, 2000, T0);
    CHECK(subs_voice_text(T0, buf, sizeof(buf)) > 0 && !strcmp(buf, "\x01" "Claire" "\x1f" "Clara" "\x02" "Ol\xC3\xA1."));
    CHECK(subs_voice_text(T0 + MS(1500), buf, sizeof(buf)) > 0 && !strcmp(buf, "\x01" "Claire" "\x1f" "Clara" "\x02" "Second."));   /* not in xx */
    subs_voice_start(4, "VOICE9.AFS", 2, 1000, T0);
    CHECK(subs_voice_text(T0, buf, sizeof(buf)) > 0 && !strcmp(buf, "Untranslated."));
    CHECK(subs_movie_open("MV_100.PSS;1") == 1 && subs_text_at(10, buf, sizeof(buf)) > 0 && !strcmp(buf, "Uma fala."));
    subs_set_language("");   /* English again from the next clip */
    subs_voice_start(4, "VOICE9.AFS", 1, 2000, T0);
    CHECK(subs_voice_text(T0, buf, sizeof(buf)) > 0 && !strcmp(buf, "\x01" "Claire" "\x1f" "Claire" "\x02" "Hello."));
    subs_voice_stop(4);
    subs_movie_close();
    SDL_RemovePath("subs_test_mods/subtitles/VOICE9.srt");
    SDL_RemovePath("subs_test_mods/subtitles/MV_100.srt");
    SDL_RemovePath("subs_test_mods/subtitles/xx.json");
    SDL_RemovePath("subs_test_mods/subtitles");
    SDL_RemovePath("subs_test_mods");
    printf("subtitles: ok\n");
    return 0;
}
