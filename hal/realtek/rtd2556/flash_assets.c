/* Burst engine driver - see flash_assets.h. */
#include "flash_assets.h"
#include "rtd2556_regs.h"

#define BURST_TIMEOUT 200       /* retries */

static u8 assets_ready;

void assets_enable(void)
{
    assets_ready = 1;
}

void asset_burst_to_port(u16 off, u16 len, u8 page, u8 port)
{
    flash_burst_to_port(ASSET_BASE + off, len, page, port);
}

void flash_burst_to_port(u32 addr, u16 len, u8 page, u8 port)
{
    u8 guard;

    if (!assets_ready)
        return;                 /* before the ISP window: fail safe */

    SR(P00_PAGE_SEL) = page;
    /* clear any previous burst selection, then hold the port address fixed:
     * the destination is one FIFO port, not a span of registers */
    SR(SCA_INF_CONTROL) &= (u8)~(SCA_INF_CONTROL_REG_BURCMD_WR
                               | SCA_INF_CONTROL_REG_BURDAT_WR
                               | SCA_INF_CONTROL_ADDR_NON_INC);
    SR(SCA_INF_CONTROL) |= SCA_INF_CONTROL_ADDR_NON_INC;
    SR(SCA_INF_ADDR) = port;

    SR(SCA_INF_BWR_ADRH) = BYTE2(addr);
    SR(SCA_INF_BWR_ADRM) = HI(addr);
    SR(SCA_INF_BWR_ADRL) = LO(addr);
    SR(SCA_INF_BWR_COUNT_H) = HI(len);
    SR(SCA_INF_BWR_COUNT_L) = LO(len);
    SR(SCA_INF_PERIOD) = 2;

    /* burst_data_source_sel = 0 selects EXTERNAL FLASH as the source (1 would
     * be internal XRAM), per the vendor field description. */
    SR(BANK_SWITCH_CONTROL) &= (u8)~BANK_SWITCH_CONTROL_BURST_DATA_SOURCE_SEL;

    /* Start.  The MCU halts until the transfer finishes, but an interrupt can
     * release it early, so re-trigger until the bit clears. */
    SR(SCA_INF_CONTROL) = SCA_INF_CONTROL_REG_BURDAT_WR | SCA_INF_CONTROL_ADDR_NON_INC;
    for (guard = 0; guard < BURST_TIMEOUT; guard++) {
        if (!(SR(SCA_INF_CONTROL) & SCA_INF_CONTROL_REG_BURDAT_WR))
            break;
        SR(SCA_INF_CONTROL) |= SCA_INF_CONTROL_REG_BURDAT_WR;
    }
    SR(BANK_SWITCH_CONTROL) &= (u8)~BANK_SWITCH_CONTROL_BURST_DATA_SOURCE_SEL;
}
