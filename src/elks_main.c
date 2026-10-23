// Blob entry: runs the 8086 emulator (ELKS) with the camera console.
#include "elks_chdk_abi.h"
#include "emu_host.h"

elks_cb_t *g_cb = 0;
void console_init(void);
extern char __bss_start[], __bss_end[];

int elks_main(elks_cb_t *cb) {
    char *p;
    for (p = __bss_start; p < __bss_end; p++) *p = 0;   // zero BSS first, before touching any global
    g_cb = cb;
    if (cb->magic != ELKS_ABI_MAGIC) return -1;
    cb->log("ELKS BLOB ENTERED");
    console_init();
    int r = emu_main();
    cb->log(r < 0 ? "emu: BIOS.BIN / FD.IMG missing" : "emu exit");
    return r;
}
