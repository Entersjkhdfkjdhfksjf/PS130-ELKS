/* ELKS/8086 blob, linked at the same cached raw-buffer address as the DOOM blob. word0 = entry, word1 = end of blob (arena start). */
ENTRY(elks_main)
SECTIONS {
  . = 0x01F40000;
  .text : {
    LONG(elks_main);     /* word0 = entry address; launcher reads + jumps it */
    LONG(_blob_end);     /* word1 = end of blob (incl bss) = start of the zone arena */
    *(.text.entry)
    *(.text*)
    *(.glue_7) *(.glue_7t)
    *(.rodata*)
  }
  . = ALIGN(8);
  .data : { *(.data*) *(.got) *(.got.plt) }
  . = ALIGN(8);
  __bss_start = .;
  .bss : { *(.bss*) *(COMMON) }
  . = ALIGN(8);
  __bss_end = .;
  _blob_end = .;
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) }
}
