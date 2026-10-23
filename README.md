# ELKS (Linux for the 8086) on the Canon PowerShot SX130 IS

An 8086 PC emulator running [ELKS](https://github.com/ghaerr/elks) (Embeddable Linux Kernel Subset, "Linux for 8086")
on the camera, as a CHDK module. Same method as the sibling [DOOM port](https://github.com/Entersjkhdfkjdhfksjf/PS130-doom): a small CHDK launcher loads a flat blob
into the idle raw buffer and jumps into it, so no Canon firmware is touched.

**Status: milestone 1.** It boots to a `login:` prompt and takes input in a PC test harness. **Not yet tested on the
camera hardware.** Things that may not work yet are listed below.

## What's in it
* `src/emu86.c`: [8086tiny](https://github.com/adriancable/8086tiny) (MIT) ported to the blob. Disk, keyboard, clock
  and quit go through `emu_host.h`. All 16-bit/32-bit memory accesses are unaligned-safe (the camera's ARM9 can't do
  unaligned loads). Build flag `-fsigned-char` is required.
* `src/console.c`: renders the guest's 80x25 text screen (read from B800:0) as 9x8 cells in the top 200 rows of the
  720x240 viewport, plus the on-screen keyboard in the bottom 40 rows. Font: public-domain 8x8 VGA (dhepper/font8x8).
* `src/elks_host.c`: loads `BIOS.BIN` and `FD.IMG` into RAM; fake clock.
* `src/gui_elks.c`: the CHDK launcher module (`elks.flt`).
* `test/`: PC harness that runs the same emulator, console and host code (`test/run.sh`).

## Install (prebuilt)
Needs CHDK 1.6.x for `sx130is` firmware `101f` (tested with 1.6.1 rev 6315).

SD card root:
* `ELKS.BIN`, `BIOS.BIN` (from `prebuilt/`)
* `FD.IMG`: the ELKS 1.44MB floppy image, [fd1440.img from ELKS v0.9.2](https://github.com/ghaerr/elks/releases/tag/v0.9.2)
  (rename it to `FD.IMG`)

`CHDK/MODULES/elks.flt` (from `prebuilt/`).

Boot the camera in **PLAYBACK mode**, enter ALT mode, then **MENU > Games > ELKS**. Login is `root` (no password).
The guest boots in roughly 5-15 seconds (estimate).

## Controls (on-screen keyboard)
| Control | Action |
|---|---|
| Rear wheel | Move the highlight along the keys (wraps) |
| D-pad | Move the highlight (up/down = rows, left/right = keys) |
| SET | Type the highlighted key |
| Shutter (full press) | Enter |
| MENU | Backspace (repeats when held) |
| Zoom lever | Switch layer: `abc` > `123/symbols` > `ABC` |
| DISP | Ctrl latch: the next key is a Ctrl character (the keyboard bar turns red) |
| Face button | Quit back to CHDK |

## Known limits
* The disk is a **RAM copy**: nothing is ever written back to the card, so changes are lost on exit and the card image
  can't be corrupted.
* The rear wheel is read with CHDK's `get_jogdial_direction()`; it may not report events on every camera/firmware.
  The d-pad does the same job.
* The guest's date is fake (ELKS prints an old date).
* No sound, no network, no hard disk. No arrow keys for the guest yet.

## Build
Toolchain: [xPack arm-none-eabi-gcc 11.3.1](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v11.3.1-1.1).
* Blob: `XPACK=/path/to/xpack-arm-none-eabi-gcc-11.3.1-1.1 ./build-elks.sh` -> `ELKS.BIN`
* Launcher: build it inside a CHDK trunk checkout: copy `src/gui_elks.c` and `src/elks_chdk_abi.h` to
  `modules/games/` in a CHDK trunk checkout, use `chdk-tree/makefile_usermodules.inc`, run
  `make PLATFORM=sx130is PLATFORMSUB=101f fir` (serial), take `modules/.o/elks.flt`.
* The blob links at `0x01F40000` (inside the SX130 IS raw buffer, cached alias). `BLOB_ADDR` in `gui_elks.c` and the address in `src/elks_blob.ld` must match.

## Credits and license
8086tiny by Adrian Cable (MIT, `third_party/8086tiny-LICENSE.txt`; `BIOS.BIN` is its BIOS), ELKS (GPL-2.0), CHDK,
font8x8 (public domain). Everything else GPL-2.0 like the rest of this repo.
