.syntax unified
.cpu cortex-m3
.thumb

.extern hook_key_down
.extern hook_key_up

.section .hook_down,"ax",%progbits
.global patch_hook_down
.thumb_func
patch_hook_down:
    bl hook_key_down

.section .hook_up,"ax",%progbits
.global patch_hook_up
.thumb_func
patch_hook_up:
    bl hook_key_up
