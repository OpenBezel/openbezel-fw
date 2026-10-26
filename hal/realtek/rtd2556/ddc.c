/* DDC slave bring-up - the DDC channel configuration (addresses,
 * debounce, external-access enables).  Needed for the EDID/DDC-CI slaves;
 * NOTE it does NOT open the host's live 0x4A scaler-port access - that is
 * gated by ISP mode.  The sequence: a park/config pass early in init, then
 * enable |= 3 on all five channels at the go-live point.
 *
 * The five DDC channels and their registers (DB "EMCU DDC" names; the
 * fifth channel is not in the RTD2537T DB, so it stays numeric):
 *   ch    ENABLE            CTRL1                CTRL2                EDID_IRQ
 *   1     ADC_DDC_ENABLE    ADC_DDC_CONTROL_1    ADC_DDC_CONTROL_2    VGA_EDID_IRQ   (FF1B/1C/1D, FFE7: VGA)
 *   2     DDC0_DDC_ENABLE   DDC0_DDC_CONTROL_1   DDC0_DDC_CONTROL_2   DDC0_EDID_IRQ  (FF1E/1F/20, FFE4)
 *   3     DDC1_DDC_ENABLE   DDC1_DDC_CONTROL_1   DDC1_DDC_CONTROL_2   DDC1_EDID_IRQ  (FF2C/2D/2E, FFE5)
 *   4     DDC2_DDC_ENABLE   DDC2_DDC_CONTROL_1   DDC2_DDC_CONTROL_2   DDC2_EDID_IRQ  (FE64/65/66, FE69)
 *   5     DDC3_DDC_ENABLE   DDC3_DDC_CONTROL_1   DDC3_DDC_CONTROL_2   DDC3_EDID_IRQ  (FE6D/6E/6F, FE72)
 * ENABLE bits: [7:5] channel address LSBs (cleared -> 0xA0 family),
 * [1] debounce enable, [0] external DDC access enable.
 */
#include "ob_types.h"
#include "dbgmon.h"
#include "rtd2556_regs.h"

/* Bit layout is identical on every channel; the DDC0 names stand for all. */
#define DDC_ENABLE_DDC_ADDR DDC0_DDC_ENABLE_DDC0_DDC_ADDR          /* [7:5] */
#define DDC_ENABLE_DBN_EN DDC0_DDC_ENABLE_DDC0_DBN_EN              /* [1] */
#define DDC_ENABLE_DDC_EN DDC0_DDC_ENABLE_DDC0_DDC_EN              /* [0] */
#define DDC_CONTROL_1_DBN_CLK_SEL DDC0_DDC_CONTROL_1_DDC0_DBN_CLK_SEL   /* [7:6] */
#define DDC_CONTROL_1_SYS_CK_SEL DDC0_DDC_CONTROL_1_DDC0_SYS_CK_SEL     /* [3] */
#define DDC_CONTROL_1_DDC2 DDC0_DDC_CONTROL_1_DDC0_DDC2                 /* [2] force DDC2 mode */
#define DDC_CONTROL_2_DDCSEG_EN DDC0_DDC_CONTROL_2_DDC0_DDCSEG_EN       /* [2] */
#define DDC_CONTROL_2_FORCE_SCL_L DDC0_DDC_CONTROL_2_DDC0_FORCE_SCL_L   /* [0] */
#define DDC_EDID_IRQ_FORCE_NACK DDC0_EDID_IRQ_DDC0_FORCE_NACK           /* [5] */
#define DDCRAM_ST_ALL (DDCRAM_ST_ADDRESS_DDCRAM_0_ST | DDCRAM_ST_ADDRESS_DDCRAM_1_ST | DDCRAM_ST_ADDRESS_DDCRAM_2_ST \
                       | DDCRAM_ST_ADDRESS_DDCRAM_3_ST | DDCRAM_ST_ADDRESS_DDCRAM_4_ST | DDCRAM_ST_ADDRESS_DDCRAM_5_ST)

static void rmw(u16 addr, u8 and_mask, u8 or_mask)
{
    SR(addr) = (SR(addr) & and_mask) | or_mask;
}

void rtd_ddc_external_enable(void)
{
    static const __code u16 en[5] = {ADC_DDC_ENABLE, DDC0_DDC_ENABLE, DDC1_DDC_ENABLE, DDC2_DDC_ENABLE, DDC3_DDC_ENABLE};
    static const __code u16 c1[5] = {ADC_DDC_CONTROL_1, DDC0_DDC_CONTROL_1, DDC1_DDC_CONTROL_1, DDC2_DDC_CONTROL_1, DDC3_DDC_CONTROL_1};
    static const __code u16 c2[5] = {ADC_DDC_CONTROL_2, DDC0_DDC_CONTROL_2, DDC1_DDC_CONTROL_2, DDC2_DDC_CONTROL_2, DDC3_DDC_CONTROL_2};
    static const __code u16 irq[5] = {VGA_EDID_IRQ, DDC0_EDID_IRQ, DDC1_EDID_IRQ, DDC2_EDID_IRQ, DDC3_EDID_IRQ};
    u8 i;

    /* park/config pass */
    dbgmon_checkpoint(0x20);
    rmw(MCU_CLK_CONTROL_1,
        (u8)~(MCU_CLK_CONTROL_1_FORCE_DDCRAM_ST | MCU_CLK_CONTROL_1_ISP_DDC2B_SWITCH), 0);
    dbgmon_checkpoint(0x21);
    rmw(DDCRAM_ST_ADDRESS, (u8)~DDCRAM_ST_ALL, 0);   /* every DDC RAM at its default start address */
    dbgmon_checkpoint(0x22);
    for (i = 0; i < 5; i++)
        rmw(c2[i], (u8)~DDC_CONTROL_2_DDCSEG_EN, 0);
    dbgmon_checkpoint(0x23);
    for (i = 0; i < 5; i++)
        rmw(en[i], (u8)~DDC_ENABLE_DDC_ADDR, 0);     /* channel addresses -> 0xA0 family */
    for (i = 0; i < 5; i++)
        rmw(c1[i], 0xFF, DDC_CONTROL_1_DDC2);
    for (i = 0; i < 5; i++)
        rmw(c1[i], (u8)~DDC_CONTROL_1_SYS_CK_SEL, 0);
    dbgmon_checkpoint(0x24);
    /* debounce clock select */
    rmw(ADC_DDC_CONTROL_1, (u8)~FV(ADC_DDC_CONTROL_1_A_DBN_CLK_SEL, 2), 0);              /* DBN_CLK_SEL[1] = 0 */
    rmw(ADC_DDC_CONTROL_1, 0xFF, FV(ADC_DDC_CONTROL_1_A_DBN_CLK_SEL, 1) | FV(ADC_DDC_CONTROL_1_A_STOP_DBN_SEL, 3));
    rmw(DDC1_DDC_CONTROL_1, (u8)~DDC_CONTROL_1_DBN_CLK_SEL, 0);
    rmw(DDC1_DDC_CONTROL_1, 0xFF, FV(DDC1_DDC_CONTROL_1_DDC1_STOP_DBN_SEL, 1));
    rmw(DDC3_DDC_CONTROL_1, (u8)~FV(DDC3_DDC_CONTROL_1_DDC3_DBN_CLK_SEL, 2), 0);        /* as ADC_DDC_CONTROL_1 */
    rmw(DDC3_DDC_CONTROL_1, 0xFF, FV(DDC3_DDC_CONTROL_1_DDC3_DBN_CLK_SEL, 1) | FV(DDC3_DDC_CONTROL_1_DDC3_STOP_DBN_SEL, 3));
    dbgmon_checkpoint(0x25);
    for (i = 0; i < 5; i++)
        rmw(irq[i], 0xFF, DDC_EDID_IRQ_FORCE_NACK);
    dbgmon_checkpoint(0x26);
    for (i = 0; i < 5; i++)
        rmw(c2[i], (u8)~DDC_CONTROL_2_FORCE_SCL_L, 0);

    /* go live: external DDC access + debounce on every channel */
    dbgmon_checkpoint(0x27);
    for (i = 0; i < 5; i++)
        rmw(en[i], 0xFF, (DDC_ENABLE_DBN_EN | DDC_ENABLE_DDC_EN));
    dbgmon_checkpoint(0x28);
}

extern const __code u8 board_edid[256];

/* Serve the panel EDID at I2C 0x50: load it into the HDMI channel's DDC RAM
 * (XDATA 0xE680), clear the
 * force_nack "EDID not ready" gate on every channel, and drop the ISP-DDC
 * auto-select flags (ISP_FLAG) that suppress EDID decode.  Call AFTER
 * rtd_ddc_external_enable().  ddcram_w_en is left OFF so a host (GPU) read
 * returns the EDID, not a writable mailbox.  This is what keeps the GPU
 * seeing a healthy display so it keeps the DDC bus alive (DDC-ISP recovery
 * stays possible even if later code hangs). */
void rtd_serve_edid(void)
{
    u16 i;
    for (i = 0; i < 256; i++)
        SR(0xE680u + i) = board_edid[i];                 /* DDC RAM, not a register */
    SR(VGA_EDID_IRQ) &= ~VGA_EDID_IRQ_ADC_FORCE_NACK;
    SR(DDC0_EDID_IRQ) &= ~DDC0_EDID_IRQ_DDC0_FORCE_NACK;
    SR(DDC1_EDID_IRQ) &= ~DDC1_EDID_IRQ_DDC1_FORCE_NACK;
    SR(DDC2_EDID_IRQ) &= ~DDC2_EDID_IRQ_DDC2_FORCE_NACK;
    SR(DDC3_EDID_IRQ) &= ~DDC3_EDID_IRQ_DDC3_FORCE_NACK;
    SR(ISP_FLAG) = 0x00;   /* HW_AUTO_SELECT_ISP_DDC and the ISP flags off */
}
