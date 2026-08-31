;-----------------------------------------------------------------------------
; c64-life.asm
;
; Conway's Game of Life for Commodore 64, text mode (40x25 @ $0400).
; Converted from x16-life.asm (Commander X16 / VERA display).
; Uses the same neighbor-count approach with wrapped edge counts.
;
; Instead of VERA register writes, characters go directly into screen RAM.
; Color RAM is filled once at startup; the game loop never touches it.
;
; Stefan Wessels, 2019 (X16 original) / 2026 (C64 port)
; This is free and unencumbered software released into the public domain.

;-----------------------------------------------------------------------------
NUMBER_PART   = %00001111           ; neighbor count mask
ALIVE_NOW     = %01000000           ; cell is alive this generation
ALIVE_NEXT    = %10000000           ; cell will be alive next generation
THIS_GEN      = %01111111           ; mask to clear ALIVE_NEXT bit
COLUMNS       = 42                  ; grid width  (40 visible + 2 border)
ROWS          = 27                  ; grid height (25 visible + 2 border)
CELL_ALIVE    = $51                 ; screen code: reversed space (solid block)
CELL_DEAD     = $20                 ; screen code: space
CELL_COLOR    = $0D                 ; color: light green

SCREEN_RAM    = $0400               ; C64 text screen base
COLOR_RAM     = $D800               ; C64 color RAM base
BORDER_COL    = $D020               ; VIC border color register
BG_COL        = $D021               ; VIC background color register

GETIN         = $FFE4

;-----------------------------------------------------------------------------
; This is for the c64m assembler - for cc65 use a .cfg file
.segdef "ZEROPAGE", $02, noemit
.segdef "BASIC", $0801
.segdef "CODE", $080d
.segdef "DATA", $0D17

;-----------------------------------------------------------------------------
.segment "ZEROPAGE"

zCellPtr:      .res 2               ; pointer into life grid
zTempPtr:      .res 2               ; temporary pointer (used in alive/die)
zRows:         .res 1               ; current row being processed
zTempY:        .res 1               ; Y register save slot
zAliveNowBit:  .res 1               ; ALIVE_NOW pattern for BIT instruction
zAliveNextBit: .res 1               ; ALIVE_NEXT pattern for BIT instruction
zScreenPtr:    .res 2               ; pointer into screen RAM for current row
zWrapSrcPtr:   .res 2               ; source pointer for wrapped-edge maintenance
zWrapDstPtr:   .res 2               ; destination pointer for wrapped-edge maintenance

;-----------------------------------------------------------------------------
; BASIC stub: 10 SYS 2061  — code begins at $080D = 2061
.segment "BASIC"

    .word $080B                     ; pointer to next BASIC line
    .word 10                        ; BASIC line number
    .byte $9E                       ; SYS token
    .byte "2061"                    ; target address as ASCII digits
    .byte 0                         ; end of BASIC line
    .word 0                         ; end of BASIC program

;-----------------------------------------------------------------------------
.segment "CODE"

jsr setup
jmp runLife

;-----------------------------------------------------------------------------
.proc setup

    lda #ALIVE_NOW
    sta zAliveNowBit

    lda #ALIVE_NEXT
    sta zAliveNextBit

    lda #0                          ; black border and background
    sta BORDER_COL
    sta BG_COL

    jsr initColorRam
    jsr clearData
    jsr setupPattern
    jsr showCanvas
    jmp addWrapEdges

.endproc

;-----------------------------------------------------------------------------
; Fill all 1000 bytes of color RAM with CELL_COLOR.
; 1000 = 3*256 + 232: three full pages then the 232-byte remainder.
.proc initColorRam

    lda #CELL_COLOR
    ldx #0
:
    sta COLOR_RAM + $000, x
    sta COLOR_RAM + $100, x
    sta COLOR_RAM + $200, x
    inx
    bne :-
    ldx #(40 * 25 - 768)       ; 231 — indices 0..231 give 232 bytes
:
    sta COLOR_RAM + $300 - 1, x
    dex
    bne :-
    rts

.endproc

;-----------------------------------------------------------------------------
.proc clearData

    lda #<patternData
    sta zCellPtr
    lda #>patternData
    sta zCellPtr + 1

    ldy #ROWS
    sty zRows

    clc

outerLoop:
    lda #0
    ldy #COLUMNS - 1

loop:
    sta (zCellPtr), y
    dey
    bpl loop
    dec zRows
    beq done

    lda zCellPtr
    adc #COLUMNS
    sta zCellPtr
    bcc outerLoop
    inc zCellPtr + 1
    clc
    bcc outerLoop

done:
    rts

.endproc

;-----------------------------------------------------------------------------
; Seed a shape
; Visible origin is patternData + COLUMNS + 1 (row 1, col 1 in the border grid).
; Visible cell (r,c) lives at patternData + (r+1)*COLUMNS + (c+1).
.proc setupPattern

    lda #ALIVE_NOW

    sta patternData + (11 * COLUMNS) + 17  ; visible row 10, col 16
    sta patternData + (11 * COLUMNS) + 21
    sta patternData + (11 * COLUMNS) + 22
    sta patternData + (11 * COLUMNS) + 23
    sta patternData + (12 * COLUMNS) + 17  ; visible row 11
    sta patternData + (12 * COLUMNS) + 18
    sta patternData + (12 * COLUMNS) + 19
    sta patternData + (12 * COLUMNS) + 22
    sta patternData + (13 * COLUMNS) + 18  ; visible row 12

    sta patternData + (11 * COLUMNS) + 6 ; glider
    sta patternData + (12 * COLUMNS) + 5
    sta patternData + (13 * COLUMNS) + 5
    sta patternData + (13 * COLUMNS) + 6
    sta patternData + (13 * COLUMNS) + 7

    rts

.endproc

;-----------------------------------------------------------------------------
; Draw the initial grid to screen RAM and initialise neighbor counts.
; zScreenPtr tracks the current screen row base (advances +40 per row).
.proc showCanvas

    lda #<SCREEN_RAM
    sta zScreenPtr
    lda #>SCREEN_RAM
    sta zScreenPtr + 1

    lda #<(patternData + COLUMNS + 1)
    sta zCellPtr
    lda #>(patternData + COLUMNS + 1)
    sta zCellPtr + 1

    lda #ROWS - 2
    sta zRows

outerLoop:
    ldy #0

loop:
    lda (zCellPtr), y
    bit zAliveNowBit
    beq notAlive

    jsr cellAlive
    lda #CELL_ALIVE
    sta (zScreenPtr), y
    bne nextCol                     ; CELL_ALIVE is non-zero, so always branches

notAlive:
    lda #CELL_DEAD
    sta (zScreenPtr), y

nextCol:
    iny
    cpy #COLUMNS - 2
    bne loop

nextRow:
    dec zRows
    beq done

    clc
    lda zScreenPtr
    adc #COLUMNS - 2                ; screen rows are 40 bytes apart
    sta zScreenPtr
    bcc :+
    inc zScreenPtr + 1
:
    clc
    lda zCellPtr
    adc #COLUMNS                    ; grid rows are COLUMNS bytes apart
    sta zCellPtr
    bcc outerLoop
    inc zCellPtr + 1
    bne outerLoop

done:
    rts

.endproc

;-----------------------------------------------------------------------------
; Increment the neighbor count in all 8 neighbors of a newly-alive cell.
.proc cellAlive

    sty zTempY

    lda zCellPtr + 1
    sta zTempPtr + 1

    clc
    tya
    adc zCellPtr
    bcc :+
    inc zTempPtr + 1
:
    sec
    sbc #(COLUMNS + 1)              ; point at upper-left neighbor
    sta zTempPtr
    bcs :+
    dec zTempPtr + 1

:
    clc
    ldy #0
    lda (zTempPtr), y               ; upper-left
    adc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y               ; above
    adc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y               ; upper-right
    adc #1
    sta (zTempPtr), y

    ldy #COLUMNS

    lda (zTempPtr), y               ; left
    adc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y               ; the cell itself — set alive flag
    and #NUMBER_PART
    ora #ALIVE_NOW
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y               ; right
    adc #1
    sta (zTempPtr), y

    ldy #(2 * COLUMNS)

    lda (zTempPtr), y               ; lower-left
    adc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y               ; below
    adc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y               ; lower-right
    adc #1
    sta (zTempPtr), y

    ldy zTempY
    rts

.endproc

;-----------------------------------------------------------------------------
; Decrement the neighbor count in all 8 neighbors of a newly-dead cell.
; Exact inverse of cellAlive.
.proc cellDie

    sty zTempY

    lda zCellPtr + 1
    sta zTempPtr + 1

    clc
    tya
    adc zCellPtr
    bcc :+
    inc zTempPtr + 1
:
    sec
    sbc #(COLUMNS + 1)
    sta zTempPtr
    bcs :+
    dec zTempPtr + 1

:
    sec
    ldy #0
    lda (zTempPtr), y
    sbc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y
    sbc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y
    sbc #1
    sta (zTempPtr), y

    ldy #COLUMNS

    lda (zTempPtr), y
    sbc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y               ; the cell itself — clear all state flags
    and #NUMBER_PART
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y
    sbc #1
    sta (zTempPtr), y

    ldy #(2 * COLUMNS)

    lda (zTempPtr), y
    sbc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y
    sbc #1
    sta (zTempPtr), y

    iny
    lda (zTempPtr), y
    sbc #1
    sta (zTempPtr), y

    ldy zTempY
    rts

.endproc

;-----------------------------------------------------------------------------
; Wrapped Life keeps the fast dead-border trick for the main 40x25 scan.  The
; border remains dead; these edge-only routines add/remove the extra neighbor
; counts that would have arrived through the opposite side of the torus.
.proc addWrapEdges

    jsr addWrapVertical
    jsr addWrapHorizontal
    jmp addWrapCorners

.endproc

;-----------------------------------------------------------------------------
.proc removeWrapEdges

    jsr removeWrapVertical
    jsr removeWrapHorizontal
    jmp removeWrapCorners

.endproc

;-----------------------------------------------------------------------------
; Add/remove one horizontal triplet at (zCellPtr + Y), +1, +2.
.proc addWrapTriplet

    sty zTempY
    clc
    lda (zCellPtr), y
    adc #1
    sta (zCellPtr), y
    iny
    lda (zCellPtr), y
    adc #1
    sta (zCellPtr), y
    iny
    lda (zCellPtr), y
    adc #1
    sta (zCellPtr), y
    ldy zTempY
    rts

.endproc

;-----------------------------------------------------------------------------
.proc removeWrapTriplet

    sty zTempY
    sec
    lda (zCellPtr), y
    sbc #1
    sta (zCellPtr), y
    iny
    lda (zCellPtr), y
    sbc #1
    sta (zCellPtr), y
    iny
    lda (zCellPtr), y
    sbc #1
    sta (zCellPtr), y
    ldy zTempY
    rts

.endproc

;-----------------------------------------------------------------------------
; Add/remove one vertical triplet at (zCellPtr + Y), +COLUMNS, +2*COLUMNS.
.proc addWrapColumn

    sty zTempY
    clc
    lda (zCellPtr), y
    adc #1
    sta (zCellPtr), y
    tya
    clc
    adc #COLUMNS
    tay
    lda (zCellPtr), y
    adc #1
    sta (zCellPtr), y
    tya
    clc
    adc #COLUMNS
    tay
    lda (zCellPtr), y
    adc #1
    sta (zCellPtr), y
    ldy zTempY
    rts

.endproc

;-----------------------------------------------------------------------------
.proc removeWrapColumn

    sty zTempY
    sec
    lda (zCellPtr), y
    sbc #1
    sta (zCellPtr), y
    tya
    clc
    adc #COLUMNS
    tay
    sec
    lda (zCellPtr), y
    sbc #1
    sta (zCellPtr), y
    tya
    clc
    adc #COLUMNS
    tay
    sec
    lda (zCellPtr), y
    sbc #1
    sta (zCellPtr), y
    ldy zTempY
    rts

.endproc

;-----------------------------------------------------------------------------
.proc addWrapOne

    clc
    lda (zCellPtr), y
    adc #1
    sta (zCellPtr), y
    rts

.endproc

;-----------------------------------------------------------------------------
.proc removeWrapOne

    sec
    lda (zCellPtr), y
    sbc #1
    sta (zCellPtr), y
    rts

.endproc

;-----------------------------------------------------------------------------
; Top visible row contributes to the bottom visible row; bottom visible row
; contributes to the top visible row.
.proc addWrapVertical

    lda #<(patternData + COLUMNS + 1)
    sta zWrapSrcPtr
    lda #>(patternData + COLUMNS + 1)
    sta zWrapSrcPtr + 1

    lda #<(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr
    lda #>(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr + 1

    ldy #COLUMNS - 3
topLoop:
    lda (zWrapSrcPtr), y
    bit zAliveNowBit
    beq :+
    jsr addWrapTriplet
:
    dey
    bpl topLoop

    lda #<(patternData + ((ROWS - 2) * COLUMNS) + 1)
    sta zWrapSrcPtr
    lda #>(patternData + ((ROWS - 2) * COLUMNS) + 1)
    sta zWrapSrcPtr + 1

    lda #<(patternData + COLUMNS)
    sta zCellPtr
    lda #>(patternData + COLUMNS)
    sta zCellPtr + 1

    ldy #COLUMNS - 3
bottomLoop:
    lda (zWrapSrcPtr), y
    bit zAliveNowBit
    beq :+
    jsr addWrapTriplet
:
    dey
    bpl bottomLoop

    rts

.endproc

;-----------------------------------------------------------------------------
.proc removeWrapVertical

    lda #<(patternData + COLUMNS + 1)
    sta zWrapSrcPtr
    lda #>(patternData + COLUMNS + 1)
    sta zWrapSrcPtr + 1

    lda #<(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr
    lda #>(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr + 1

    ldy #COLUMNS - 3
topLoop:
    lda (zWrapSrcPtr), y
    bit zAliveNowBit
    beq :+
    jsr removeWrapTriplet
:
    dey
    bpl topLoop

    lda #<(patternData + ((ROWS - 2) * COLUMNS) + 1)
    sta zWrapSrcPtr
    lda #>(patternData + ((ROWS - 2) * COLUMNS) + 1)
    sta zWrapSrcPtr + 1

    lda #<(patternData + COLUMNS)
    sta zCellPtr
    lda #>(patternData + COLUMNS)
    sta zCellPtr + 1

    ldy #COLUMNS - 3
bottomLoop:
    lda (zWrapSrcPtr), y
    bit zAliveNowBit
    beq :+
    jsr removeWrapTriplet
:
    dey
    bpl bottomLoop

    rts

.endproc

;-----------------------------------------------------------------------------
; Left visible column contributes to the right visible column; right visible
; column contributes to the left visible column.
.proc addWrapHorizontal

    lda #<(patternData + COLUMNS + 1)
    sta zWrapSrcPtr
    lda #>(patternData + COLUMNS + 1)
    sta zWrapSrcPtr + 1

    lda #<patternData
    sta zWrapDstPtr
    lda #>patternData
    sta zWrapDstPtr + 1

    lda #ROWS - 2
    sta zRows
leftLoop:
    ldy #0
    lda (zWrapSrcPtr), y
    bit zAliveNowBit
    beq :+
    lda zWrapDstPtr
    sta zCellPtr
    lda zWrapDstPtr + 1
    sta zCellPtr + 1
    ldy #COLUMNS - 2
    jsr addWrapColumn
:
    clc
    lda zWrapSrcPtr
    adc #COLUMNS
    sta zWrapSrcPtr
    bcc :+
    inc zWrapSrcPtr + 1
:
    clc
    lda zWrapDstPtr
    adc #COLUMNS
    sta zWrapDstPtr
    bcc :+
    inc zWrapDstPtr + 1
:
    dec zRows
    bne leftLoop

    lda #<(patternData + COLUMNS + 1)
    sta zWrapSrcPtr
    lda #>(patternData + COLUMNS + 1)
    sta zWrapSrcPtr + 1

    lda #<patternData
    sta zWrapDstPtr
    lda #>patternData
    sta zWrapDstPtr + 1

    lda #ROWS - 2
    sta zRows
rightLoop:
    ldy #COLUMNS - 3
    lda (zWrapSrcPtr), y
    bit zAliveNowBit
    beq :+
    lda zWrapDstPtr
    sta zCellPtr
    lda zWrapDstPtr + 1
    sta zCellPtr + 1
    ldy #1
    jsr addWrapColumn
:
    clc
    lda zWrapSrcPtr
    adc #COLUMNS
    sta zWrapSrcPtr
    bcc :+
    inc zWrapSrcPtr + 1
:
    clc
    lda zWrapDstPtr
    adc #COLUMNS
    sta zWrapDstPtr
    bcc :+
    inc zWrapDstPtr + 1
:
    dec zRows
    bne rightLoop

    rts

.endproc

;-----------------------------------------------------------------------------
.proc removeWrapHorizontal

    lda #<(patternData + COLUMNS + 1)
    sta zWrapSrcPtr
    lda #>(patternData + COLUMNS + 1)
    sta zWrapSrcPtr + 1

    lda #<patternData
    sta zWrapDstPtr
    lda #>patternData
    sta zWrapDstPtr + 1

    lda #ROWS - 2
    sta zRows
leftLoop:
    ldy #0
    lda (zWrapSrcPtr), y
    bit zAliveNowBit
    beq :+
    lda zWrapDstPtr
    sta zCellPtr
    lda zWrapDstPtr + 1
    sta zCellPtr + 1
    ldy #COLUMNS - 2
    jsr removeWrapColumn
:
    clc
    lda zWrapSrcPtr
    adc #COLUMNS
    sta zWrapSrcPtr
    bcc :+
    inc zWrapSrcPtr + 1
:
    clc
    lda zWrapDstPtr
    adc #COLUMNS
    sta zWrapDstPtr
    bcc :+
    inc zWrapDstPtr + 1
:
    dec zRows
    bne leftLoop

    lda #<(patternData + COLUMNS + 1)
    sta zWrapSrcPtr
    lda #>(patternData + COLUMNS + 1)
    sta zWrapSrcPtr + 1

    lda #<patternData
    sta zWrapDstPtr
    lda #>patternData
    sta zWrapDstPtr + 1

    lda #ROWS - 2
    sta zRows
rightLoop:
    ldy #COLUMNS - 3
    lda (zWrapSrcPtr), y
    bit zAliveNowBit
    beq :+
    lda zWrapDstPtr
    sta zCellPtr
    lda zWrapDstPtr + 1
    sta zCellPtr + 1
    ldy #1
    jsr removeWrapColumn
:
    clc
    lda zWrapSrcPtr
    adc #COLUMNS
    sta zWrapSrcPtr
    bcc :+
    inc zWrapSrcPtr + 1
:
    clc
    lda zWrapDstPtr
    adc #COLUMNS
    sta zWrapDstPtr
    bcc :+
    inc zWrapDstPtr + 1
:
    dec zRows
    bne rightLoop

    rts

.endproc

;-----------------------------------------------------------------------------
; Corner cells need the one diagonal contribution in addition to their
; horizontal and vertical wrapped-edge contributions.
.proc addWrapCorners

    lda patternData + COLUMNS + 1
    bit zAliveNowBit
    beq :+
    lda #<(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr
    lda #>(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr + 1
    ldy #COLUMNS - 2
    jsr addWrapOne
:
    lda patternData + COLUMNS + (COLUMNS - 2)
    bit zAliveNowBit
    beq :+
    lda #<(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr
    lda #>(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr + 1
    ldy #1
    jsr addWrapOne
:
    lda patternData + ((ROWS - 2) * COLUMNS) + 1
    bit zAliveNowBit
    beq :+
    lda #<(patternData + COLUMNS)
    sta zCellPtr
    lda #>(patternData + COLUMNS)
    sta zCellPtr + 1
    ldy #COLUMNS - 2
    jsr addWrapOne
:
    lda patternData + ((ROWS - 2) * COLUMNS) + (COLUMNS - 2)
    bit zAliveNowBit
    beq :+
    lda #<(patternData + COLUMNS)
    sta zCellPtr
    lda #>(patternData + COLUMNS)
    sta zCellPtr + 1
    ldy #1
    jsr addWrapOne
:
    rts

.endproc

;-----------------------------------------------------------------------------
.proc removeWrapCorners

    lda patternData + COLUMNS + 1
    bit zAliveNowBit
    beq :+
    lda #<(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr
    lda #>(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr + 1
    ldy #COLUMNS - 2
    jsr removeWrapOne
:
    lda patternData + COLUMNS + (COLUMNS - 2)
    bit zAliveNowBit
    beq :+
    lda #<(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr
    lda #>(patternData + ((ROWS - 2) * COLUMNS))
    sta zCellPtr + 1
    ldy #1
    jsr removeWrapOne
:
    lda patternData + ((ROWS - 2) * COLUMNS) + 1
    bit zAliveNowBit
    beq :+
    lda #<(patternData + COLUMNS)
    sta zCellPtr
    lda #>(patternData + COLUMNS)
    sta zCellPtr + 1
    ldy #COLUMNS - 2
    jsr removeWrapOne
:
    lda patternData + ((ROWS - 2) * COLUMNS) + (COLUMNS - 2)
    bit zAliveNowBit
    beq :+
    lda #<(patternData + COLUMNS)
    sta zCellPtr
    lda #>(patternData + COLUMNS)
    sta zCellPtr + 1
    ldy #1
    jsr removeWrapOne
:
    rts

.endproc

;-----------------------------------------------------------------------------
; Two-pass simulation loop.
; Pass 1: walk every visible cell and set ALIVE_NEXT where the rules are met.
; Pass 2: apply births/deaths, update neighbor counts, write to screen RAM.
.proc runLife

    lda #<(patternData + COLUMNS + 1)
    sta zCellPtr
    lda #>(patternData + COLUMNS + 1)
    sta zCellPtr + 1

    lda #ROWS - 2
    sta zRows

p1_outer:
    ldy #COLUMNS - 3

p1_loop:
    lda (zCellPtr), y
    beq p1_next                     ; empty cell, skip
    bit zAliveNowBit
    beq p1_notAliveNow

    and #NUMBER_PART
    cmp #4
    bcs p1_next                     ; 4+ neighbors — dies
    cmp #2
    bcc p1_next                     ; 0-1 neighbors — dies

    ora #(ALIVE_NEXT | ALIVE_NOW)   ; 2 or 3 neighbors — survives
    sta (zCellPtr), y
    bne p1_next

p1_notAliveNow:
    cmp #3
    bne p1_next
    ora #ALIVE_NEXT                 ; exactly 3 neighbors — born
    sta (zCellPtr), y

p1_next:
    dey
    bpl p1_loop

    dec zRows
    beq p1_done

    lda zCellPtr
    clc
    adc #COLUMNS
    sta zCellPtr
    bcc p1_outer
    inc zCellPtr + 1
    bne p1_outer

p1_done:
    jsr removeWrapEdges

p2_start:
    lda #<(patternData + COLUMNS + 1)
    sta zCellPtr
    lda #>(patternData + COLUMNS + 1)
    sta zCellPtr + 1

    lda #<SCREEN_RAM
    sta zScreenPtr
    lda #>SCREEN_RAM
    sta zScreenPtr + 1

    lda #0
    sta zRows

p2_outer:
    ldy #COLUMNS - 3

p2_loop:
    lda (zCellPtr), y
    beq p2_next
    bit zAliveNextBit
    beq p2_notAliveNext

    bit zAliveNowBit
    beq p2_notAliveNow

    and #THIS_GEN                   ; already alive, stays alive — clear ALIVE_NEXT
    sta (zCellPtr), y
    jmp p2_next                     ; no draw needed

p2_notAliveNow:
    jsr cellAlive                   ; dead cell is born — update neighbors
    ldx #CELL_ALIVE
    bne draw                        ; always branches (CELL_ALIVE is non-zero)

p2_notAliveNext:
    bit zAliveNowBit
    beq p2_next                     ; wasn't alive, ignore
    jsr cellDie                     ; alive cell dies — update neighbors
    ldx #CELL_DEAD

draw:
    txa
    sta (zScreenPtr), y             ; write screen code to current row + column Y

p2_next:
    dey
    bpl p2_loop

    inc zRows
    lda zRows
    cmp #ROWS - 2
    beq done

    lda zCellPtr
    clc
    adc #COLUMNS
    sta zCellPtr
    bcc :+
    inc zCellPtr + 1
:
    lda zScreenPtr
    clc
    adc #COLUMNS - 2                ; advance screen ptr by 40 columns
    sta zScreenPtr
    bcc p2_outer
    inc zScreenPtr + 1
    bne p2_outer

done:
    jsr addWrapEdges
    ; jsr GETIN                     ; uncomment for single-step debugging
    ; beq done
    jmp runLife

.endproc

;-----------------------------------------------------------------------------
.segment "DATA"

patternData:
    .res COLUMNS * ROWS             ; 42 * 27 = 1134 bytes
