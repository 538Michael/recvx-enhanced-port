/*
 * --log[=file]: everything written to stdout/stderr (the game's own printf diagnostics and
 * the port's messages) is copied to a log file, each line prefixed with the time since start.
 * A fatal signal (segfault, abort) appends a backtrace to the log before the process dies.
 * Always, also without --log: a crash writes recvx-crash.log (the current directory, where --log puts
 * recvx.log): the version, the signal or exception, the backtrace and the port's last messages
 * (host_log keeps the last LAST_N in memory), a few KB players can attach to a bug report.
 */
#ifdef _WIN32
#define GetThreadId win_GetThreadId   /* the PS2 kernel shim declares its own */
#define CreateThread win_CreateThread
#include <windows.h>
#include <io.h>
#undef GetThreadId
#undef CreateThread
#endif
#include "../ps2_platform.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAST_N 48                       /* host_log lines kept for the crash report */
#define LAST_W 200
static char last[LAST_N][LAST_W];
static SDL_AtomicInt last_n;
static const char crash_path[] = "recvx-crash.log";
static char crash_head[128];

/* host.c host_log: every message, also with --quiet (best effort across threads: each takes a slot of its own);
   IPv4 addresses masked, so the report can be posted publicly (pc/src/net/net.c) */
const char* net_mask_addrs(const char* text, const char* const* hosts, char* out, int cap);
void log_keep(const char* fmt, va_list ap)
{
    char t[LAST_W];
    int i = SDL_AddAtomicInt(&last_n, 1) % LAST_N;
    SDL_vsnprintf(t, sizeof(t), fmt, ap);
    net_mask_addrs(t, NULL, last[i], LAST_W);
}

/* the kept lines, oldest first, through `out` (async-signal-safe on Linux: write) */
static void last_dump(void (*out)(void*, const char*, size_t), void* ud)
{
    int n = SDL_GetAtomicInt(&last_n), k;
    out(ud, "\nlast messages:\n", 16);
    for (k = n > LAST_N ? n - LAST_N : 0; k < n; k++) {
        const char* l = last[k % LAST_N];
        size_t len = strlen(l);
        out(ud, l, len);
        if (!len || l[len - 1] != '\n') out(ud, "\n", 1);
    }
}
#ifndef _WIN32
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

static int log_fd = -1, console_fd = 2, pipe_rd = -1;
static uint64_t t0;

static void write_all(int fd, const char* p, size_t n)
{
    while (n) { ssize_t k = write(fd, p, n); if (k <= 0) return; p += k; n -= (size_t)k; }
}

static int SDLCALL tee_thread(void* ud)
{
    char buf[4096], stamp[32];
    int at_line_start = 1;
    (void)ud;
    for (;;) {
        ssize_t n = read(pipe_rd, buf, sizeof(buf));
        if (n <= 0) break;
        write_all(console_fd, buf, (size_t)n);
        for (ssize_t i = 0; i < n;) {
            ssize_t j = i;
            if (at_line_start) {
                double t = (SDL_GetTicksNS() - t0) / 1e9;
                int len = SDL_snprintf(stamp, sizeof(stamp), "[%9.3f] ", t);
                write_all(log_fd, stamp, (size_t)len);
                at_line_start = 0;
            }
            while (j < n && buf[j] != '\n') j++;
            if (j < n) { j++; at_line_start = 1; }
            write_all(log_fd, buf + i, (size_t)(j - i));
            i = j;
        }
    }
    return 0;
}

static void out_fd(void* fd, const char* p, size_t n) { write_all(*(int*)fd, p, n); }

static void on_fatal(int sig)
{
    static int entered;
    void* frames[64];
    int n, fd;
    char msg[96];
    if (__atomic_exchange_n(&entered, 1, __ATOMIC_ACQ_REL)) for (;;) pause();   /* another thread is writing the report; the process ends with it */
    n = backtrace(frames, 64);
    int len = SDL_snprintf(msg, sizeof(msg), "\nfatal signal %d (%s), backtrace:\n", sig, sig == SIGSEGV ? "segmentation fault" : sig == SIGABRT ? "abort" : "signal");
    write_all(console_fd, msg, (size_t)len);
    backtrace_symbols_fd(frames, n, console_fd);
    if (log_fd >= 0) { write_all(log_fd, msg, (size_t)len); backtrace_symbols_fd(frames, n, log_fd); fsync(log_fd); }
    if ((fd = open(crash_path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0) {
        write_all(fd, crash_head, strlen(crash_head));
        write_all(fd, msg + 1, (size_t)len - 1);
        backtrace_symbols_fd(frames, n, fd);
        last_dump(out_fd, &fd);
        fsync(fd);
        close(fd);
        write_all(console_fd, "crash report written to recvx-crash.log\n", 41);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

/* exit(): what the game printed just before is still in the pipe (the tee thread dies
   with the process) - copy it now and say that the process exited */
static void on_exit_drain(void)
{
    char buf[4096];
    ssize_t n;
    static const char msg[] = "\nprocess exiting (exit() called)\n";
    fflush(stdout); fflush(stderr);
    if (pipe_rd < 0 || log_fd < 0) return;
    fcntl(pipe_rd, F_SETFL, fcntl(pipe_rd, F_GETFL) | O_NONBLOCK);
    while ((n = read(pipe_rd, buf, sizeof(buf))) > 0) { write_all(log_fd, buf, (size_t)n); write_all(console_fd, buf, (size_t)n); }
    write_all(log_fd, msg, sizeof(msg) - 1);
    fsync(log_fd);
}

/* main(), always: the crash report's handler */
void crash_init(const char* version)
{
    static char altstack[64 * 1024];   /* an own stack for the handler, so stack overflows are reported too */
    stack_t ss = { .ss_sp = altstack, .ss_size = sizeof(altstack), .ss_flags = 0 };
    struct sigaction sa;
    static const int sigs[] = { SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL, SIGTRAP };
    SDL_snprintf(crash_head, sizeof(crash_head), "recvx-enhanced-port %s crashed (Linux; addresses: addr2line -f -e recvx)\n", version);
    sigaltstack(&ss, NULL);
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_fatal;
    sa.sa_flags = SA_ONSTACK;
    for (unsigned i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) sigaction(sigs[i], &sa, NULL);
}

void log_start(const char* path)
{
    int p[2];
    t0 = SDL_GetTicksNS();
    log_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) { fprintf(stderr, "log: cannot write %s\n", path); return; }
    if (pipe(p) != 0) { close(log_fd); log_fd = -1; return; }
    fflush(stdout); fflush(stderr);
    console_fd = dup(2);
    pipe_rd = p[0];
    dup2(p[1], 1);
    dup2(p[1], 2);
    close(p[1]);
    SDL_DetachThread(SDL_CreateThread(tee_thread, "log", NULL));
    atexit(on_exit_drain);
    host_log("log: writing %s\n", path);
}
#else
/* fixed image base (no ASLR): addresses map to functions with i686-w64-mingw32-addr2line -f -e recvx.exe;
   the Windows build keeps frame pointers (pc/CMakeLists.txt), so EBP is the frame chain */
static void out_file(void* f, const char* p, size_t n) { fwrite(p, 1, n, (FILE*)f); }

static LONG WINAPI on_fatal(EXCEPTION_POINTERS* ep)
{
    static LONG entered;
    const CONTEXT* c = ep->ContextRecord;
    uint32_t* fp = (uint32_t*)c->Ebp;
    char msg[512];
    int len;
    if (InterlockedExchange(&entered, 1)) for (;;) Sleep(INFINITE);   /* another thread is writing the report */
    len = snprintf(msg, sizeof(msg), "\nfatal exception 0x%08lX at 0x%08lX (esp 0x%08lX), frame chain:",
                       ep->ExceptionRecord->ExceptionCode, (unsigned long)c->Eip, (unsigned long)c->Esp);
    FILE* f;
    for (int i = 0; i < 32 && fp && !IsBadReadPtr(fp, 8) && len < (int)sizeof(msg) - 16; i++, fp = (uint32_t*)fp[0])
        len += snprintf(msg + len, sizeof(msg) - len, " 0x%08X", fp[1]);
    fprintf(stderr, "%s\n", msg);
    fflush(stderr);
    if ((f = fopen(crash_path, "w"))) {
        fprintf(f, "%s%s\n", crash_head, msg + 1);
        last_dump(out_file, f);
        fclose(f);
        fprintf(stderr, "crash report written to %s\n", crash_path);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* main(), always: the crash report's handler */
void crash_init(const char* version)
{
    snprintf(crash_head, sizeof(crash_head), "recvx-enhanced-port %s crashed (Windows; addresses: i686-w64-mingw32-addr2line -f -e recvx.exe)\n", version);
    SetUnhandledExceptionFilter(on_fatal);
}

void log_start(const char* path)
{
    wchar_t w[1024];   /* the path is UTF-8 (fileio.h) */
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, w, 1024) || !_wfreopen(w, L"w", stdout)) return;
    setvbuf(stdout, NULL, _IONBF, 0);
    _dup2(_fileno(stdout), 2);
    host_log("log: writing %s\n", path);
}
#endif
