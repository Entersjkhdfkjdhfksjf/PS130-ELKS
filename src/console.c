// Text console + on-screen keyboard for the 8086 blob.
// Screen: 720x240 8-bit indexed. Top 200 rows = the PC's 80x25 text screen (read straight from
// emulated B800:0, 9x8 cells). Bottom 40 rows = on-screen keyboard (3 rows x 13 keys).
//
// Input scheme (milestone 1):
//   wheel          move the highlight along the keys (wraps)     d-pad  move highlight (U/D rows, L/R keys)
//   SET            type the highlighted key                      shutter (full)  Enter
//   zoom lever     switch layer: abc -> 123/symbols -> ABC       MENU   Backspace (repeats)
//   DISP           Ctrl latch (next key is a Ctrl-char)          PRINT  quit (handled by the launcher)
#include "elks_chdk_abi.h"
#include "emu_host.h"
#include "font8x8.h"

extern elks_cb_t *g_cb;

#define FB_W 720
#define FB_H 240
#define CELL_W 9
#define CELL_H 8
#define TEXT_ROWS 25
#define TEXT_COLS 80
#define OSK_Y 200
#define KEY_COLS 13
#define KEY_ROWS 3
#define KEY_W 55
#define KEY_H 13

static unsigned char fb[FB_W * FB_H];
static unsigned char shadow[TEXT_ROWS * TEXT_COLS * 2];
static int force_text = 1, osk_dirty = 1, cur_shown = 0;
static unsigned int last_draw = 0;

// palette indices
#define C_OSK_BG   16
#define C_KEY_BG   17
#define C_KEY_FG   18
#define C_HI_BG    19
#define C_HI_FG    20
#define C_CTRL_BG  21
#define C_SPEC_BG  22

static void set_palette(void) {
    static const unsigned char vga[16][3] = {
        {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
        {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255} };
    unsigned char pal[256 * 3];
    int i;
    for (i = 0; i < 256 * 3; i++) pal[i] = 0;
    for (i = 0; i < 16; i++) { pal[i*3] = vga[i][0]; pal[i*3+1] = vga[i][1]; pal[i*3+2] = vga[i][2]; }
#define SETC(n,r,g,b) do { pal[(n)*3]=(r); pal[(n)*3+1]=(g); pal[(n)*3+2]=(b); } while (0)
    SETC(C_OSK_BG, 24, 24, 40);   SETC(C_KEY_BG, 64, 64, 92);  SETC(C_KEY_FG, 235, 235, 235);
    SETC(C_HI_BG, 255, 176, 32);  SETC(C_HI_FG, 0, 0, 0);      SETC(C_CTRL_BG, 130, 30, 30);
    SETC(C_SPEC_BG, 40, 96, 60);
    g_cb->set_palette(pal);
}

// ---------------------------------------------------------------- text screen
static void draw_cell(int r, int c, unsigned char ch, unsigned char at, int cursor) {
    int fg = at & 15, bg = (at >> 4) & 7;
    unsigned char *d = fb + (r * CELL_H) * FB_W + c * CELL_W;
    int y, x;
    for (y = 0; y < CELL_H; y++) {
        unsigned char bits = font8x8[ch][y];
        for (x = 0; x < 8; x++) d[y * FB_W + x] = (bits >> x) & 1 ? fg : bg;
        d[y * FB_W + 8] = (ch >= 0xC0 && ch <= 0xDF && (bits & 0x80)) ? fg : bg;   // box-drawing joins
    }
    if (cursor) for (y = 6; y < 8; y++) for (x = 0; x < 9; x++) d[y * FB_W + x] = fg ? fg : 7;
}

// BIOS cursor position (see crt_curpos_* in the 8086tiny BIOS: mem[0x49D]=col, mem[0x49E]=row)
static int cursor_col(void) { return mem[0x49D]; }
static int cursor_row(void) { return mem[0x49E]; }

static int pcol = -1, prow = -1;
static int redraw_text(int blink_on) {
    int r, c, changed = 0;
    unsigned char *vm = mem + 0xB8000;
    int ccol = cursor_col(), crow = cursor_row();
    int show = blink_on && ccol < TEXT_COLS && crow < TEXT_ROWS;
    int cur_changed = (ccol != pcol || crow != prow || show != cur_shown);
    for (r = 0; r < TEXT_ROWS; r++)
        for (c = 0; c < TEXT_COLS; c++) {
            int i = (r * TEXT_COLS + c) * 2;
            int on_cur = (r == crow && c == ccol), on_prev = (r == prow && c == pcol);
            if (force_text || vm[i] != shadow[i] || vm[i+1] != shadow[i+1] || (cur_changed && (on_cur || on_prev))) {
                draw_cell(r, c, vm[i], vm[i+1], show && on_cur);
                shadow[i] = vm[i]; shadow[i+1] = vm[i+1];
                changed = 1;
            }
        }
    force_text = 0; cur_shown = show; pcol = ccol; prow = crow;
    return changed;
}

// ---------------------------------------------------------------- on-screen keyboard
static const char *const LAYER[3][KEY_ROWS][KEY_COLS] = {
  { { "q","w","e","r","t","y","u","i","o","p","-","/","BKSP" },
    { "a","s","d","f","g","h","j","k","l",".","_","~","ENTER" },
    { "z","x","c","v","b","n","m",",",";","*","TAB","ESC","SPACE" } },
  { { "1","2","3","4","5","6","7","8","9","0","-","=","BKSP" },
    { "!","@","#","$","%","^","&","*","(",")","_","+","ENTER" },
    { "[","]","{","}","|","\\",":","\"","'","<",">","?","SPACE" } },
  { { "Q","W","E","R","T","Y","U","I","O","P","-","/","BKSP" },
    { "A","S","D","F","G","H","J","K","L",".","_","~","ENTER" },
    { "Z","X","C","V","B","N","M",",",";","*","TAB","ESC","SPACE" } },
};
static int layer = 0, kr = 0, kc = 0, ctrl_latch = 0;

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int sameq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

static void draw_glyph(int x, int y, unsigned char ch, int fg, int bg) {
    int yy, xx;
    for (yy = 0; yy < 8; yy++) {
        unsigned char bits = font8x8[ch][yy];
        for (xx = 0; xx < 8; xx++) fb[(y + yy) * FB_W + x + xx] = (bits >> xx) & 1 ? fg : bg;
    }
}

static void redraw_osk(void) {
    int r, c, x, y;
    int bgc = ctrl_latch ? C_CTRL_BG : C_OSK_BG;
    for (y = OSK_Y; y < FB_H; y++) for (x = 0; x < FB_W; x++) fb[y * FB_W + x] = bgc;
    for (r = 0; r < KEY_ROWS; r++)
        for (c = 0; c < KEY_COLS; c++) {
            const char *lab = LAYER[layer][r][c];
            int hi = (r == kr && c == kc), n = slen(lab);
            int special = n > 1;
            int kbg = hi ? C_HI_BG : (special ? C_SPEC_BG : C_KEY_BG);
            int kfg = hi ? C_HI_FG : C_KEY_FG;
            int kx = c * KEY_W + 2, ky = OSK_Y + r * KEY_H + 1;
            for (y = 0; y < KEY_H - 2; y++) for (x = 0; x < KEY_W - 4; x++) fb[(ky + y) * FB_W + kx + x] = kbg;
            int tx = kx + ((KEY_W - 4) - n * 8) / 2, i;
            for (i = 0; i < n; i++) draw_glyph(tx + i * 8, ky + 2, (unsigned char)lab[i], kfg, kbg);
        }
    osk_dirty = 0;
}

// ---- keystroke queue (bytes the 8086 BIOS sees as terminal input)
static unsigned char kq[32];
static int kq_r = 0, kq_w = 0;
static void kpush(unsigned char b) { int n = (kq_w + 1) & 31; if (n != kq_r) { kq[kq_w] = b; kq_w = n; } }
int host_kbd_pop(unsigned char *out) {
    if (kq_r == kq_w) return 0;
    *out = kq[kq_r]; kq_r = (kq_r + 1) & 31; return 1;
}

static void type_char(unsigned char c) {
    if (ctrl_latch) { ctrl_latch = 0; osk_dirty = 1; if (c >= 'a' && c <= 'z') c -= 32; if (c >= '@' && c <= '_') c &= 0x1F; else if (c == '?') c = 0x7F; }
    kpush(c);
}
static void activate_key(void) {
    const char *l = LAYER[layer][kr][kc];
    if      (sameq(l, "BKSP"))  { ctrl_latch = 0; kpush(0x08); osk_dirty = 1; }
    else if (sameq(l, "ENTER")) { ctrl_latch = 0; kpush(0x0D); osk_dirty = 1; }
    else if (sameq(l, "SPACE")) { type_char(' '); }
    else if (sameq(l, "TAB"))   { type_char(0x09); }
    else if (sameq(l, "ESC"))   { type_char(0x1B); }
    else                         type_char((unsigned char)l[0]);
}
static void move_lin(int d) {
    int i = kr * KEY_COLS + kc + d, n = KEY_ROWS * KEY_COLS;
    i = ((i % n) + n) % n; kr = i / KEY_COLS; kc = i % KEY_COLS; osk_dirty = 1;
}

// ---- input polling (edge detect + hold-repeat for movement/backspace)
static unsigned int prev_btn = 0, rep_t0[11], rep_last[11];
static void on_press(unsigned int b) {
    switch (b) {
        case BTN_UP:       kr = (kr + KEY_ROWS - 1) % KEY_ROWS; osk_dirty = 1; break;
        case BTN_DOWN:     kr = (kr + 1) % KEY_ROWS; osk_dirty = 1; break;
        case BTN_LEFT:     kc = (kc + KEY_COLS - 1) % KEY_COLS; osk_dirty = 1; break;
        case BTN_RIGHT:    kc = (kc + 1) % KEY_COLS; osk_dirty = 1; break;
        case BTN_SET:      activate_key(); break;
        case BTN_SHOOT:    ctrl_latch = 0; kpush(0x0D); osk_dirty = 1; break;
        case BTN_MENU:     ctrl_latch = 0; kpush(0x08); osk_dirty = 1; break;
        case BTN_DISP:     ctrl_latch = !ctrl_latch; osk_dirty = 1; break;
        case BTN_ZOOM_IN:  layer = (layer + 1) % 3; osk_dirty = 1; break;
        case BTN_ZOOM_OUT: layer = (layer + 2) % 3; osk_dirty = 1; break;
    }
}
static void poll_input(unsigned int now) {
    unsigned int cur = g_cb->get_buttons(), i;
    for (i = 0; i < 11; i++) {
        unsigned int b = 1u << i;
        if ((cur & b) && !(prev_btn & b)) { on_press(b); rep_t0[i] = rep_last[i] = now; }
        else if ((cur & b) && (b & (BTN_UP|BTN_DOWN|BTN_LEFT|BTN_RIGHT|BTN_MENU)) &&
                 now - rep_t0[i] > 400 && now - rep_last[i] > 90) { on_press(b); rep_last[i] = now; }
    }
    prev_btn = cur;
    int d, guard = 8;
    while (guard-- && (d = g_cb->get_jog())) move_lin(d > 0 ? 1 : -1);
}

// ---------------------------------------------------------------- public
void console_init(void) { set_palette(); osk_dirty = 1; force_text = 1; last_draw = 0; }

// called by host_tick(): returns nothing; draws when something changed (<= ~12 fps)
void console_tick(void) {
    unsigned int now = g_cb->get_ticks_ms();
    poll_input(now);
    if (now - last_draw < 80) return;
    int blink_on = ((now / 500) & 1) == 0;
    int changed = 0;
    if (osk_dirty) { redraw_osk(); changed = 1; }
    if (redraw_text(blink_on)) changed = 1;
    if (changed) { g_cb->draw_frame(fb, FB_W, FB_H); }
    last_draw = now;
}
