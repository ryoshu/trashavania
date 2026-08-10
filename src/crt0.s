; crt0.s -- startup/interrupt code for Trashavania (Mapper 2/UxROM, CHR RAM)
;
; Provides:
;   - iNES header (HEADER segment)
;   - reset handler: disable interrupts, init stack/PPU, clear internal RAM,
;     wait for two vblanks (PPU warm-up), init cc65 runtime (sp, BSS, DATA),
;     call _main
;   - nmi handler: bumps _nmi_flag once per vblank (see nes.h / ppu.c)
;   - irq handler: unused, just RTI
;   - _oam / _nmi_flag storage (declared extern in nes.h, defined here since
;     nothing else owns them)
;   - hardware vectors

        .import         _main
        .import         copydata, zerobss
        .import         __PSTACK_START__, __PSTACK_SIZE__
        .importzp       sp

        .export         _oam, _nmi_flag
        .export         __STARTUP__ : absolute = 1     ; force STARTUP inclusion

        .include        "zeropage.inc"

INES_MAPPER     = 2     ; UxROM
INES_MIRROR     = 1     ; vertical mirroring
INES_PRG_BANKS  = 4     ; 4 x 16KB PRG
INES_CHR_BANKS  = 0     ; CHR RAM, no CHR ROM

PPUCTRL         = $2000
PPUMASK         = $2001
PPUSTATUS       = $2002
APUFRAME        = $4017
DMCFREQ         = $4010

; ---------------------------------------------------------------------------
; iNES header

.segment "HEADER"

        .byte   "NES", $1A
        .byte   INES_PRG_BANKS
        .byte   INES_CHR_BANKS
        .byte   (INES_MAPPER & $0F) << 4 | INES_MIRROR
        .byte   INES_MAPPER & $F0
        .res    8, $00

; ---------------------------------------------------------------------------
; OAM shadow buffer -- page-aligned, DMA'd to real OAM by ppu_update().
; Lives in its own segment (OAM_BUF, mapped to $0200 by nes-uxrom.cfg) so
; it's guaranteed page alignment regardless of what else is in BSS.

.segment "OAM_BUF"

_oam:   .res    256

; ---------------------------------------------------------------------------
; Set once per vblank by the NMI handler below; wait_vblank() (ppu.c) polls
; and clears it.

.segment "BSS"

_nmi_flag:
        .res    1

; ---------------------------------------------------------------------------
; Reset handler

.segment "STARTUP"

reset:
        sei                     ; ignore IRQs
        cld                     ; disable decimal mode (no-op on 2A03, cheap insurance)
        ldx     #$40
        stx     APUFRAME        ; disable APU frame IRQ
        ldx     #$FF
        txs                     ; init hardware stack
        inx                     ; x = 0
        stx     PPUCTRL         ; disable NMI
        stx     PPUMASK         ; disable rendering
        stx     DMCFREQ         ; disable DMC IRQ

        ; first vblank wait -- PPU needs ~30k cycles after power-on before
        ; its registers behave; don't touch PPUCTRL/PPUMASK/PPUADDR before this
vblank1:
        bit     PPUSTATUS
        bpl     vblank1

        ; clear all internal RAM ($0000-$07FF) to 0 while waiting for the
        ; second vblank
clear_ram:
        lda     #$00
        sta     $0000,x
        sta     $0100,x
        sta     $0200,x
        sta     $0300,x
        sta     $0400,x
        sta     $0500,x
        sta     $0600,x
        sta     $0700,x
        inx
        bne     clear_ram

        ; second vblank wait -- PPU is now stable
vblank2:
        bit     PPUSTATUS
        bpl     vblank2

        ; init cc65 parameter (software) stack pointer -- grows down from
        ; the top of the PSTACK region defined in nes-uxrom.cfg
        lda     #<(__PSTACK_START__ + __PSTACK_SIZE__)
        sta     sp
        lda     #>(__PSTACK_START__ + __PSTACK_SIZE__)
        sta     sp+1

        ; copy initialized DATA from ROM to RAM, zero BSS (RAM already
        ; cleared above, but zerobss also covers anything the linker placed
        ; after this point -- keep it for correctness w.r.t. cc65 conventions)
        jsr     copydata
        jsr     zerobss

        jsr     _main
        ; _main never returns (infinite loop in main.c), but if it ever did:
forever:
        jmp     forever

; ---------------------------------------------------------------------------
; NMI handler -- runs once per vblank once PPUCTRL_NMI_ENABLE is set.
; Keep this minimal: just flag the event, let the main loop do PPU updates
; during the vblank window it detects via wait_vblank().

.segment "CODE"

nmi:
        pha
        txa
        pha
        tya
        pha

        inc     _nmi_flag

        pla
        tay
        pla
        tax
        pla
        rti

irq:
        rti

; ---------------------------------------------------------------------------
; Hardware vectors

.segment "VECTORS"

        .addr   nmi
        .addr   reset
        .addr   irq
