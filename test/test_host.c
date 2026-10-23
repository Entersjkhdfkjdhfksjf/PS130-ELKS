// PC test harness: runs the exact blob sources (emu86.c, console.c, elks_host.c) on the host with
// stdio-backed callbacks. Dumps the guest's text screen and a PPM of the camera framebuffer.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "../src/elks_chdk_abi.h"

elks_cb_t *g_cb;
extern unsigned char mem[];
int emu_main(void);
void console_init(void);

static unsigned int limit_ms = 20000;
static unsigned int t0;
static unsigned int now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000u + ts.tv_nsec / 1000000u; }
static unsigned int c_ticks(void) { return now_ms() - t0; }
static unsigned char pal[768]; static unsigned char lastfb[720*240]; static int frames = 0;
static void c_draw(const unsigned char *fb, int w, int h) { memcpy(lastfb, fb, w*h); frames++; }
static void c_pal(const unsigned char *p) { memcpy(pal, p, 768); }
static int c_open(const char *p, int m) { return open(p, m == 2 ? O_RDWR : O_RDONLY); }
static int c_read(int fd, void *b, int n) { return read(fd, b, n); }
static int c_write(int fd, const void *b, int n) { return write(fd, b, n); }
static int c_seek(int fd, int o, int w) { return lseek(fd, o, w); }
static int c_close(int fd) { return close(fd); }
static void c_log(const char *s) { fprintf(stderr, "[log] %s\n", s); }
static int c_quit(void) { return c_ticks() > limit_ms; }
static unsigned int script_buttons = 0;
// scripted input: (time_ms, button mask held)
static struct { unsigned t, m; } script[64]; static int nscript = 0;
static unsigned int c_buttons(void) { unsigned t = c_ticks(), m = 0; for (int i = 0; i < nscript; i++) if (t >= script[i].t) m = script[i].m; return m; }
static int c_jog(void) { return 0; }
static void c_sleep(unsigned ms) { (void)ms; }

static void dump_text(FILE *f) {
    for (int r = 0; r < 25; r++) {
        char line[81]; int last = -1;
        for (int c = 0; c < 80; c++) { unsigned char ch = mem[0xB8000 + (r*80+c)*2]; line[c] = (ch >= 32 && ch < 127) ? ch : (ch ? '?' : ' '); if (line[c] != ' ') last = c; }
        line[last + 1] = 0; fprintf(f, "|%s\n", line);
    }
}
int main(int argc, char **argv) {
    static elks_cb_t cb;
    cb.magic = ELKS_ABI_MAGIC; cb.get_ticks_ms = c_ticks; cb.sleep_ms = c_sleep; cb.draw_frame = c_draw; cb.set_palette = c_pal;
    cb.f_open = c_open; cb.f_read = c_read; cb.f_write = c_write; cb.f_lseek = c_seek; cb.f_close = c_close; cb.log = c_log;
    cb.should_quit = c_quit; cb.get_buttons = c_buttons; cb.get_jog = c_jog;
    g_cb = &cb;
    if (argc > 1) limit_ms = atoi(argv[1]);
    // script: "t:mask" pairs on argv[2..]
    for (int i = 2; i < argc; i++) { unsigned t, m; if (sscanf(argv[i], "%u:%x", &t, &m) == 2) { script[nscript].t = t; script[nscript].m = m; nscript++; } }
    t0 = now_ms();
    console_init();
    int r = emu_main();
    fprintf(stderr, "emu_main -> %d, frames drawn %d\n", r, frames);
    dump_text(stdout);
    FILE *f = fopen("fb.ppm", "wb"); fprintf(f, "P6\n720 240\n255\n");
    for (int i = 0; i < 720*240; i++) fwrite(pal + lastfb[i]*3, 1, 3, f); fclose(f);
    return 0;
}
