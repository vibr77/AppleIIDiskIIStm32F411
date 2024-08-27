


.global  sectorBuf

//.p2align 2
.syntax unified
.cpu cortex-m3
.thumb

.type    myadd,%function
.equ     PERIPHBASE,        0x40000000UL
.equ     APB2PERIPH_BASE,   0x00010000UL
.equ     GPIOC_BASE,        0x00001000UL 
.equ     PINOUT,            0x00002000UL
.equ     PIN_BSRR,PERIPHBASE+APB2PERIPH_BASE+GPIOC_BASE+0x10              ///
.equ     PIN_ODR,PERIPHBASE+APB2PERIPH_BASE+GPIOC_BASE+0x0C

.equ     DWTCYCNT,               0xE0001000UL+0x004

.global wait_1us
wait_1us:
    .fnstart
    push {lr}
    nop                         ;// 1   1
    nop                         ;// 1   2
    mov r2,#20                  ;// 1   3
wait_1us_1:
    subs    r2,r2,#1            ;// 1   1
    bne     wait_1us_1          ;// 1   2
    pop     {lr}
    bx      lr                          // return from function call 
    .fnend

.global wait_3us
wait_3us:
    .fnstart
    push {lr}                   ;//      2   2       push to stack the LR register return pointer
    nop                         ;//      1   3       do nothing, syncing clock cycle
    nop                         ;//      1   4       do nothing, syncing clock cycle
wait_3us_1:
    subs    r2,r2,#1            ;//                     decrement r2 by 1
    bne     wait_3us_1          ;//      1   2       check if Zero flag is set     
    pop     {lr}                ;//      2   4       restoring LR register from the stack
    bx      lr                  ;//      1   5       return from function call 
    .fnend

.global  AppleIIDataPulse
.type    AppleIIDataPulse,%function
AppleIIDataPulse:
;//                                                  r0 holds the sectorbuf memory address
;//                                                  r1 current memory offset (bytes)
;//
    .fnstart
    push    {lr}                ;//      2   2       Branch to the function is 1
    push    {r0,r1}             ;//      3   5       push to the stack
    push    {r2,r3}             ;//      3   8       push to the stack
    push    {r4,r5}             ;//      3  11       push to the stack
    push    {r6}                ;//      2  12       push to the stack
    ldr     r0, =sectorBuf      ;//    111 123       memory address of sectorBuf
    mov     r1,#0               ;//      1 124       set r1 to 0
    ldr     r6,=PIN_BSRR        ;//      2   2
    b       process             ;//      1 125       jump to process section
    
process:
    cmp     r1,#402             ;//      1   1       402 Bytes to be sent 
    beq     end                 ;//      1   2       End of the sector
    add     r1,r1,#1            ;//      1   3
    ldr     r3,[r0]             ;//      2   5
    mov     r4,#9               ;//      1   6       we want to send 32 byte (Start counting at 33)
    add     r0,r0,1             ;//      1   7
    b       sendByte            ;//      1   8       129

sendByte:
    and     r5,r3,0x80000000    ;//      1   1     
    lsl     r3,r3,#1            ;//      1   2       Right shift r3 by 1
    subs    r4,r4,#1            ;//      1   3       dec r4 bit counter
    //mov     r6,#0             ;//                  Reset the DWT Cycle counter for debug cycle counting
    //ldr     r6,=DWTCYCNT
    //mov     r2,#0
    //str     r2,[r6]           ;//                  end
    bne     sendBit             ;//      1   4       2nd Cycle brkpoint 426 => 715 =289
    beq     process             ;//      1   5
                                ;//                  Clk 15, Readpulse 14, Enable 13
sendBit:
    
    ldr	    r2,[r6]             ;//      3   5     
    cmp     r5,#0               ;//      1   6   
    ite     EQ  
    moveq   r2,  #0x80000000
    movne   r2,  #0x00008000    ;//      1   7
    //orreq r2,r2,  #0x80000000 ;//      1   8       set bit 31 to 1, GPIO Pin 15 LOW
    //orrne r2,r2,  #0x00008000 ;//      1   9       set bit 15 to 1, GPIO Pin 15 HIGH     
    orr     r2,r2,  #0x00004000 ;//      1  10       set bit 14 to 1, GPIO Pin 14 HIGH (clock signal for the logic analyzer)
    str	    r2,  [r6]           ;//      1  11       set the GPIO port -> from here POINT A to POINT B we need 1us, 72 CPU cycles
    b       il_wait_1us
sendBitAfter_w1us:              ;//     70  81       
    mov     r2,  #0xC0000000
    //orr     r2,r2,  #0xC0000000 ;//      1  82       set bit 30 & 30 to 1, GPIO PIN 15 & 14 LOW after 1us
    str	    r2,[r6]             ;//      1  83       set the GPIO port, POINT B
                                ;//                  We need to adjust the duration of the remaining 3us 
                                ;//                  function if it is the first bit (coming from process less 10 cycle)
    cmp     r4,#1               ;//      1  78 
    ite     eq                  ;//      1  79 
    moveq   r2,#53             ;//      1  80    
    movne   r2,#64              ;//      1  81
    b       il_wait_3us         ;//    186 267       wait for 3 us in total
sendBitAfter_w3us:     
    b       sendByte            ;//    187 268       loop
il_wait_1us:
    nop                         ;//      1   1
    nop                         ;//      1   2
    mov r2,#21                  ;//      1   3
il_wait_1us_1:
    subs    r2,r2,#1            ;//      1   1
    bne     il_wait_1us_1       ;//          1   
    b       sendBitAfter_w1us      

il_wait_3us:               
                                ;//      2   2       push to stack the LR register return pointer
    nop                         ;//      1   3       do nothing, syncing clock cycle
    nop                         ;//      1   4       do nothing, syncing clock cycle
il_wait_3us_1:
    subs    r2,r2,#1            ;//                  decrement r2 by 1
    bne     il_wait_3us_1       ;//      1   2       check if Zero flag is set
    b sendBitAfter_w3us

end:
    pop     {r6}                ;//      1   1       pop from the stack
    pop     {r4,r5}             ;//      1   2       pop from the stack
    pop     {r2,r3}             ;//      1   3       pop from the stack
    pop     {r0,r1}             ;//      1   4       pop from the stack
    pop     {lr}                ;//      1   5
    bx  lr
    .fnend
