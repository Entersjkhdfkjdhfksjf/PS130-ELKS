#!/bin/bash
# Build ELKS.BIN: the 8086-emulator blob for the Canon PowerShot SX130 IS (fw 101f).
# Same toolchain/approach as ../build-blob.sh (xPack arm-none-eabi-gcc 11.3.1, flat binary linked
# at the cached raw-buffer address 0x01F40000, loaded + jumped to by the CHDK launcher elks.flt).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -n "$XPACK" ]; then export PATH="${XPACK%/}/bin:$PATH"; fi
command -v arm-none-eabi-gcc >/dev/null || { echo "arm-none-eabi-gcc not found (set XPACK= or add to PATH)"; exit 1; }
TC="$(dirname "$(command -v arm-none-eabi-gcc)")/.."
OUT="$HERE/build"; rm -rf "$OUT"; mkdir -p "$OUT"
CF="-c -march=armv5te -mtune=arm946e-s -O2 -fno-strict-aliasing -fsigned-char -ffunction-sections -w"
for c in emu86 console elks_host elks_main elks_syscalls; do
  arm-none-eabi-gcc $CF -I"$HERE/src" -o "$OUT/$c.o" "$HERE/src/$c.c"
done
LIBC="$TC/arm-none-eabi/lib/arm/v5te/softfp/libc.a"
LIBGCC="$(ls "$TC"/lib/gcc/arm-none-eabi/*/arm/v5te/softfp/libgcc.a | head -1)"
arm-none-eabi-gcc -nostartfiles -nostdlib -Wl,-T,"$HERE/src/elks_blob.ld" -Wl,-Map,"$OUT/elks_blob.map" \
  -o "$OUT/elks_blob.elf" -Wl,--start-group "$OUT"/*.o "$LIBC" "$LIBGCC" -Wl,--end-group
arm-none-eabi-objcopy -O binary "$OUT/elks_blob.elf" "$HERE/ELKS.BIN"
echo "built ELKS.BIN:"; ls -la "$HERE/ELKS.BIN"
arm-none-eabi-nm "$OUT/elks_blob.elf" | grep -iE " elks_main$|__bss_start|__bss_end|_blob_end"
