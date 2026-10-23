// Host services for the 8086 core, built on the launcher's callback table.
#include "elks_chdk_abi.h"
#include "emu_host.h"

extern elks_cb_t *g_cb;
void console_tick(void);

// Disk/BIOS images are loaded whole into RAM (the arena). Reads AND writes hit the RAM copy only,
// so the card image is never modified (an abrupt power-off can't corrupt it) and sector access is
// fast. Writes are lost on exit; write-back to the card is a later milestone.
#define MAXH 4
static struct { unsigned char *data; unsigned size, pos; } H[MAXH];
void *malloc(unsigned long n);
void *memcpy(void *d, const void *s, unsigned long n);

int host_open(const char *name, int rw) {
    int i; (void)rw;
    for (i = 0; i < MAXH && H[i].data; i++) ;
    if (i == MAXH) return 0;
    int fd = g_cb->f_open(name, 0);
    if (fd < 0) return 0;
    int size = g_cb->f_lseek(fd, 0, 2);
    g_cb->f_lseek(fd, 0, 0);
    if (size <= 0) { g_cb->f_close(fd); return 0; }
    unsigned char *buf = malloc((unsigned long)size);
    if (!buf) { g_cb->f_close(fd); return 0; }
    int got = 0, r;
    while (got < size && (r = g_cb->f_read(fd, buf + got, size - got > 65536 ? 65536 : size - got)) > 0) got += r;
    g_cb->f_close(fd);
    if (got != size) return 0;
    H[i].data = buf; H[i].size = (unsigned)size; H[i].pos = 0;
    return i + 1;
}
int host_seek(int h, unsigned off) {
    if (h < 1 || h > MAXH || !H[h-1].data || off > H[h-1].size) return -1;
    H[h-1].pos = off; return (int)off;
}
int host_rw(int write, int h, void *buf, int n) {
    if (h < 1 || h > MAXH || !H[h-1].data || n < 0) return -1;
    unsigned avail = H[h-1].size - H[h-1].pos;
    unsigned cnt = (unsigned)n < avail ? (unsigned)n : avail;
    if (write) memcpy(H[h-1].data + H[h-1].pos, buf, cnt); else memcpy(buf, H[h-1].data + H[h-1].pos, cnt);
    H[h-1].pos += cnt;
    return (int)cnt;
}

// Wall-clock for the guest: the camera RTC isn't wired up yet, so ELKS gets a fixed date plus
// uptime. Layout = 9 ints of struct tm (sec,min,hour,mday,mon,year,wday,yday,isdst), then 16-bit ms.
void host_get_rtc(unsigned char *dst) {
    unsigned int ms = g_cb->get_ticks_ms();
    unsigned int t = 12 * 3600 + ms / 1000;                 // start at 12:00:00
    int day = 22 + t / 86400;                                // Oct 23, 2026 + day rollover (month rollover ignored)
    t %= 86400;
    int v[9] = { (int)(t % 60), (int)((t / 60) % 60), (int)(t / 3600), day + 1, 9, 126, (4 + (day - 22)) % 7, 295 + (day - 22), 0 };
    int i, k;
    for (i = 0; i < 9; i++) for (k = 0; k < 4; k++) dst[i * 4 + k] = (unsigned char)(v[i] >> (8 * k));
    dst[36] = (unsigned char)((ms % 1000) & 0xFF); dst[37] = (unsigned char)((ms % 1000) >> 8);
}

int host_tick(void) {
    console_tick();
    return g_cb->should_quit && g_cb->should_quit();
}
