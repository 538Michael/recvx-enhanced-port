/* Co-op lobby screen (pc/glue/lobby_glue.c, game thread) and its host side
   (pc/src/host/lobby_io.c, main thread): text entry from the keyboard, the clipboard, the
   memory card's save files. docs/COOP.md "Lobby". */
#ifndef RECVX_LOBBY_H
#define RECVX_LOBBY_H
#ifdef __cplusplus
extern "C" {
#endif

/* keys for a text field, besides the characters 32..126 */
enum { LK_ENTER = 256, LK_ESC, LK_BACK, LK_DEL, LK_LEFT, LK_RIGHT, LK_HOME, LK_END };

typedef struct LobbyIO {
    volatile int text_on;           /* game thread: a text field is edited (the keyboard types, it is no pad) */
    volatile unsigned int head, tail;   /* key ring: the main thread writes at head, the game thread reads at tail */
    volatile int keys[64];
    char clip[128];                 /* game thread: text for the clipboard ... */
    volatile int clip_req;          /* ... taken by the main thread */
} LobbyIO;
extern LobbyIO g_lobby_io;

/* main thread */
void lobby_io_event(const void* sdl_event);
void lobby_io_frame(void* window);      /* per presented frame: text input on/off, clipboard, test script */

/* any thread: memory card save slot 0..14 (<save_dir>/card0/BASLUS-20184/SAVEDATA-nn, the game's
   SAVEFILE: the save region sys->version..save_end, then a byte-sum checksum). 1 = valid. */
#define LOBBY_SAVE_BYTES 0x838
int  lobby_read_save(int slot, unsigned char* out);
int  lobby_write_save(int slot, const void* region, int size);   /* test aid: region + checksum; 1 = written */
/* co-op, any thread: the host's saved partner inventories that go with save slot 0..14
   (<save_dir>/coop-partners-nn.bin). read: bytes read (0 none); write: n 0 removes the file; 1 = done */
int  lobby_read_partners(int slot, void* buf, int cap);
int  lobby_write_partners(int slot, const void* buf, int n);
void lobby_frame(void);                 /* game thread, frame hand-off: test aids */
void lobby_settings_changed(void);      /* game thread: write recvx.ini */

#ifdef __cplusplus
}
#endif
#endif
