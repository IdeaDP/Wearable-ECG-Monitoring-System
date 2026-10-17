ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Heap_Size = 0;
_Min_Stack_Size = 0x400;
PHDRS
{
  flash PT_LOAD FLAGS(5);
  ram PT_LOAD FLAGS(6);
}
SECTIONS
{
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } >FLASH :flash
  .text :
  {
    . = ALIGN(4);
    *(.text .text.*) *(.glue_7) *(.glue_7t)
    KEEP(*(.init)) KEEP(*(.fini))
    . = ALIGN(4); _etext = .;
  } >FLASH :flash
  .rodata : { . = ALIGN(4); *(.rodata .rodata.*) . = ALIGN(4); } >FLASH :flash
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH :flash
  .ARM.exidx :
  {
    __exidx_start = .; *(.ARM.exidx*) __exidx_end = .;
  } >FLASH :flash
  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH :flash
  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH :flash
  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH :flash
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4); _sdata = .;
    *(.data .data.*) . = ALIGN(4); _edata = .;
  } >RAM AT>FLASH :ram
  .bss (NOLOAD) : AT(LOADADDR(.data) + ADDR(.bss) - ADDR(.data))
  {
    . = ALIGN(4); _sbss = .; __bss_start__ = .;
    *(.bss .bss.*) *(COMMON)
    . = ALIGN(4); _ebss = .; __bss_end__ = .;
  } >RAM :ram
  PROVIDE(end = ALIGN(_ebss, 8));
  PROVIDE(_end = ALIGN(_ebss, 8));
  ASSERT(ALIGN(_ebss, 8) + _Min_Heap_Size + _Min_Stack_Size <= _estack,
         "RAM overflow: insufficient stack space")
  .ARM.attributes 0 : { *(.ARM.attributes) } :NONE
}
