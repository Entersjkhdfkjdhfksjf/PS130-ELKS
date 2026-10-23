// Host services the 8086 core needs; implemented in elks_host.c on top of the launcher callbacks.
#ifndef EMU_HOST_H
#define EMU_HOST_H
int  emu_main(void);                          // runs until host_tick() says quit; <0 if boot files missing
int  host_open(const char *name, int rw);     // returns handle > 0, or 0 on failure (rw: 0=read, 1=read/write)
int  host_seek(int h, unsigned off);          // absolute seek; returns offset or -1
int  host_rw(int write, int h, void *buf, int n);
int  host_kbd_pop(unsigned char *out);        // 1 + byte if a key byte is queued
void host_get_rtc(unsigned char *dst);        // fills a 36-byte struct tm + 16-bit ms at +36
int  host_tick(void);                         // called every ~20000 emulated instructions; nonzero = quit
extern unsigned char mem[];                   // emulated 8086 memory (defined in emu86.c)
extern unsigned char io_ports[];
#endif
