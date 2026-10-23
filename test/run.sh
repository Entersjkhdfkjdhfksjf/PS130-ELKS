#!/bin/bash
# Boot ELKS in the PC build of the emulator (same emu86.c / console.c / elks_host.c as the blob).
# usage: ./run.sh [ms] [t:hexmask ...]   -> prints the guest text screen, writes fb.ppm (camera framebuffer)
set -e
cd "$(dirname "$0")"
[ -f FD.IMG ] || curl -sL -o FD.IMG https://github.com/ghaerr/elks/releases/download/v0.9.2/fd1440.img
cp ../prebuilt/BIOS.BIN BIOS.BIN
gcc -O2 -fsigned-char -fno-strict-aliasing -w -o host_test test_host.c ../src/emu86.c ../src/console.c ../src/elks_host.c
./host_test "${@:-10000}"
