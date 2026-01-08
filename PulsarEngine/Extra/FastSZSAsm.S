# DecodeSZS logic developed by Rambo.

.text
.global decodeSZSAsm
.align 0x20
decodeSZSAsm:
    lwz r5, 4(r4)
    li r11, 0x20
    li r6, 0
    mr r0, r5
    addi r4, r4, 0xF
    addi r3, r3, -1
    cmpwi r5, 0x132
    ble decode_small_block_loop
    addi r5, r5, -0x132
    nop # Align
    nop
    nop
    nop
    nop
    nop
    nop
decode_main_loop:
    srwi. r6, r6, 1
    bne fetch_bit_byte_main
    lbzu r7, 1(r4)
    li r6, 0x80
fetch_bit_byte_main:
    and. r8, r6, r7
    lbzu r8, 1(r4)
    beq decode_repeat_sequence_main
    andi. r9, r3, 0x1F
    bne emit_literal_and_continue_main
    dcbz r11, r3
emit_literal_and_continue_main:
    addic.    r5, r5, -1
    stbu      r8, 1(r3)
    bne       decode_main_loop
    b         check_block_reset_main
decode_repeat_sequence_main:
    lbzu    r9, 1(r4)
    srwi.   r10, r8, 4
    bne     prepare_copy_main
    lbzu    r10, 1(r4)
    addi    r10, r10, 0x10
prepare_copy_main:
    addi    r10, r10, 2
    insrwi  r9, r8, 4, 20
    subf    r5, r10, r5
    subf    r8, r9, r3
    mtctr   r10
    addi    r8, r8, 1
copy_repeated_bytes_main:
    andi.   r9, r3, 0x1F
    lbz     r9, -1(r8)
    addi    r8, r8, 1
    bne     copy_repeated_bytes_main_continue
    dcbz    r11, r3
copy_repeated_bytes_main_continue:
    stbu    r9, 1(r3)
    bdnz    copy_repeated_bytes_main
    cmpwi   r5, 0
    bgt     decode_main_loop
check_block_reset_main:
    addi    r5, r5, 0x132
    cmpwi   r5, 0
    ble     return_decompressed_size
decode_small_block_loop:
    srwi.   r6, r6, 1
    bne     fetch_bit_byte_small
    lbzu    r7, 1(r4)
    li      r6, 0x80
fetch_bit_byte_small:
    and.    r8, r6, r7
    lbzu    r8, 1(r4)
    beq     decode_repeat_sequence_small
    addic.  r5, r5, -1
    stbu    r8, 1(r3)
    bne     decode_small_block_loop
    b       return_decompressed_size
decode_repeat_sequence_small:
    lbzu    r9, 1(r4)
    srwi.   r10, r8, 4
    bne     prepare_copy_small
    lbzu    r10, 1(r4)
    addi    r10, r10, 0x10
prepare_copy_small:
    addi    r10, r10, 2
    insrwi  r9, r8, 4, 20
    subf.   r5, r10, r5
    blt     return_decompressed_size
    subf    r8, r9, r3
    mtctr   r10
    addi    r8, r8, 1
copy_repeated_bytes_small:
    lbz     r9, -1(r8)
    addi    r8, r8, 1
    stbu    r9, 1(r3)
    bdnz    copy_repeated_bytes_small
    cmpwi   r5, 0
    bgt     decode_small_block_loop
return_decompressed_size:
    mr      r3, r0
    blr