/* RTD2556 startup hook.
 *
 * _sdcc_external_startup() runs before static initialization.  Return 0 to
 * let normal init (including XDATA clearing) proceed.
 *
 * Watchdog: three independent enables arm it - WATCHDOG_TIMER (0xFFEA)
 * bit7 wdt_en, MCU_CLK_CONTROL_1 (0xFFE9) bit7 wdt_en_3 and PWM_CK_SEL
 * (0xFF3A) bit6 wdt_en_2 (rtd2556_regs.h).  board_early_init() in
 * boards/intouch156b/board_init.c clears all three, so nothing is needed
 * here.  It is armed at power-up, so whatever runs before board_early_init()
 * must be short (see scaler_soft_reset()); a watchdog that stays armed shows
 * as a reset loop - a heartbeat that keeps restarting from 0.
 */
#include "ob_types.h"
#include "rtd2556_regs.h"
#include "delay.h"
#include "flash_rw.h"

unsigned char __sdcc_external_startup(void)
{
    return 0;
}

/* Initialized XDATA variables (XISEG) from their image (XINIT) - redone by
 * hand, with DPTR, because SDCC's own crtxinit does NOT work on this core.
 *
 * crtxinit writes with `movx @r0`, taking the high address byte from P2
 * (__XPAGE = 0xA0).  This 8051 does not page `movx @Ri` through P2, so the
 * initial values land somewhere else and every `static x = value` in XDATA
 * would start with whatever the RAM held.
 *
 * crtxinit still runs (its stray writes are harmless in practice); this puts
 * the right values where they belong.  Call it FIRST in main, before anything
 * reads an initialized variable. */
void xinit_copy(void) __naked
{
    __asm
        mov     r6,#l_XINIT
        mov     r7,#(l_XINIT >> 8)
        mov     r2,#s_XINIT
        mov     r3,#(s_XINIT >> 8)
        mov     r4,#s_XISEG
        mov     r5,#(s_XISEG >> 8)
    00001$:
        mov     a,r6
        orl     a,r7
        jz      00003$
        mov     dpl,r2
        mov     dph,r3
        clr     a
        movc    a,@a+dptr
        inc     dptr
        mov     r2,dpl
        mov     r3,dph
        mov     dpl,r4
        mov     dph,r5
        movx    @dptr,a
        inc     dptr
        mov     r4,dpl
        mov     r5,dph
        mov     a,r6
        jnz     00002$
        dec     r7
    00002$:
        dec     r6
        sjmp    00001$
    00003$:
        ret
    __endasm;
}

/* Whole-scaler soft reset: HOST_CTRL bit 0 high for
 * >= 8 ms, then low.
 *
 * An ISP-exit reboot only resets the 8051; without this the scaler keeps
 * whatever the PREVIOUS image left behind, and video after a reflash can be
 * broken until a real power cycle.  Run FIRST in main(), before
 * board_early_init(): everything the recovery path needs (pinshare, DDC, EDID,
 * HPD) is configured after it, so nothing it clears matters.  HOST_CTRL is in
 * the common page (0x0000-0x009E), visible in XDATA before the scaler remap is
 * on.
 *
 * Timing: on the crystal clock one delay_ms unit is ~6.8 ms (delay.h),
 * so 2 is ~14 ms high (HOST_CTRL wants >= 8 ms).  Do NOT lengthen it here: this runs before
 * board_early_init() disables the watchdog, which is ARMED at power-up
 * (MCU_CLK_CONTROL_1 bit 7) - a much longer pulse reset-loops the chip on
 * every power-up. */
void scaler_soft_reset(void)
{
    SR_SET(HOST_CTRL, HOST_CTRL_SOFT_RESET);
    delay_ms(2);
    SR_CLR(HOST_CTRL, HOST_CTRL_SOFT_RESET);
    delay_ms(2);
}

/* ---- Code banking -----------------------------------------------------------
 *
 * CODE_PAGE (XDATA 0xFFFF) maps a whole 64 KB flash page into the code space,
 * so every bank image carries an identical copy of the COMMON region
 * (0x0000 - BANK_SPLIT-1: vectors, startup, the recovery path, the interrupt
 * handler and everything it calls, these trampolines, SDCC's helpers).  Banked
 * code lives in the window above it.
 *
 * SDCC calls a __banked function as  r0:r1:r2 = target lo, hi, bank;
 * lcall __sdcc_banked_call  - and the function returns through
 * ljmp __sdcc_banked_ret.  Same stack protocol as SDCC's own (PSBANK)
 * version: caller's return address, the caller's bank, then the target,
 * which `ret` jumps to.  The bank register is XDATA here, so DPTR (the
 * first argument / return value, with B and A) is saved around it; r3/r4 are
 * free (caller-saved, never argument registers).  The current bank is
 * shadowed in XDATA. */
__xdata u8 code_page_shadow;

void _sdcc_banked_call(void) __naked
{
    __asm
        xch     a,r0                    ; r0 = A (argument), a = target lo
        mov     r3,a
        push    dpl
        push    dph
        mov     dptr,#_code_page_shadow
        movx    a,@dptr
        mov     r4,a                    ; the caller bank
        mov     a,r2
        movx    @dptr,a
        mov     dptr,#0xFFFF            ; CODE_PAGE: switch (this code is common)
        movx    @dptr,a
        pop     dph
        pop     dpl
        mov     a,r4
        push    acc                     ; for __sdcc_banked_ret
        mov     a,r3
        push    acc                     ; target lo
        mov     a,r1
        push    acc                     ; target hi
        mov     a,r0                    ; A back
        ret                             ; into the target
    __endasm;
}

void _sdcc_banked_ret(void) __naked
{
    __asm
        xch     a,r0                    ; keep A (return value)
        pop     acc                     ; the caller bank
        push    dpl
        push    dph
        mov     dptr,#_code_page_shadow
        movx    @dptr,a
        mov     dptr,#0xFFFF
        movx    @dptr,a
        pop     dph
        pop     dpl
        xch     a,r0
        ret
    __endasm;
}

/* Is every code bank from the same build as bank 0?  mkimage stamps each bank
 * image with the build ID (CRC-32 of the common region) at 0xFF10 and bank
 * 0's with the mask of banks the build has (0xFF14).  Read through the flash
 * controller, NOT by mapping the bank: a bank from another build carries a
 * different common region, and mapping it would swap the code running this
 * check.  A partial flash, an old or a blank bank -> 0, and main() then makes
 * no banked call at all (the recovery path stays: EDID, ISP). */
u8 banks_consistent(void)
{
    static __xdata u8 ref[6], id[4];
    u16 mask;
    u8 n, i;

    flash_read(0x00FF10UL, ref, 6);
    mask = ref[4] | ((u16)ref[5] << 8);
    for (n = 1; n < 16; n++) {
        if (!(mask & (1U << n)))
            continue;
        flash_read(((u32)n << 16) | 0xFF10UL, id, 4);
        for (i = 0; i < 4; i++)
            if (id[i] != ref[i])
                return 0;
    }
    return 1;
}
