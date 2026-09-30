SECTIONS
{
  .hook_down 0x08006AB4 : { KEEP(*(.hook_down)) }
  .hook_up   0x08006B0C : { KEEP(*(.hook_up)) }

  .payload   0x0802700C : {
    *(.text*)
    *(.rodata*)
  }

  /DISCARD/ : {
    *(.comment*)
    *(.ARM.attributes*)
    *(.llvm_addrsig*)
  }
}
