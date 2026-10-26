/* DDC/CI slave engine - see ddcci_hw.h. */
#include "ddcci_hw.h"
#include "rtd2556_regs.h"

#define DDCCI_ADDR 0x6E

/* The IIC_STATUS flags are write-0-to-clear; writing 1s elsewhere leaves them. */
#define IIC_CLR(bit) (SR(IIC_STATUS) = (u8)~(bit))

static u8 rx_len;
static __xdata u8 rx[DDCCI_RX_MAX];
static u8 rx_src;

void ddcci_hw_init(void)
{
    SR(IIC_IRQ_CONTROL) = 0;                            /* polled: no interrupt sources */
    /* Follow whichever DDC channel the host talks on.  Every channel serves the
     * same EDID (ddc.c), and DDC1 is the HDMI one,
     * but auto-switch avoids hard-coding the channel map. */
    SR(IIC_IRQ_CONTROL2) = IIC_IRQ_CONTROL2_DDC_CH_AUTO_SWITCH_EN | IIC_IRQ_CONTROL2_RST_DATA_BUF;
    SR(IIC_CHANNEL_CONTROL) = IIC_CHANNEL_CONTROL_CH_SWITCH_AUTO_RST_EN
                            | IIC_CHANNEL_CONTROL_NULL_MSG_EN;
    /* the DDC/CI null message: source 6E, length 80, checksum 50^6E^80 = BE */
    SR(DDCCI_NULL_BYTE1) = 0x6E;
    SR(DDCCI_NULL_BYTE2) = 0x80;
    SR(DDCCI_NULL_BYTE3) = 0xBE;
    SR(IIC_SET_SLAVE) = DDCCI_ADDR;
    SR(IIC_STATUS) = 0;                                 /* clear everything */
    SR(IIC_STATUS2) = 0;
    rx_len = 0;
}

u8 ddcci_hw_poll(u8 *src, u8 *buf)
{
    u8 st = SR(IIC_STATUS);
    u8 i, n;

    /* Reply fully read: turn the FIFO back to receive NOW, not at the next
     * SUB_I.  Bytes a host writes while WEN is still set are discarded, and a
     * polled SUB_I comes far too late - a Set VCP following a read reply would
     * be lost. */
    if ((SR(IIC_IRQ_CONTROL2) & IIC_IRQ_CONTROL2_DATA_BUF_WEN)
        && (SR(IIC_STATUS2) & IIC_STATUS2_IIC_BUF_EMPTY))
        SR_CLR(IIC_IRQ_CONTROL2, IIC_IRQ_CONTROL2_DATA_BUF_WEN);

    if (st & IIC_STATUS_SUB_I) {
        /* Start of a host WRITE: the sub-address is the DDC/CI source byte.
         * Turn the FIFO round to receive by clearing WEN ONLY - never reset
         * it here.  The host's bytes land in the FIFO whatever the direction,
         * so by the time a slow poll sees SUB_I the message (sometimes a Set
         * VCP and the Get that verifies it) is already in there, and a reset
         * would drop it.  A reply the host abandoned
         * would be left in front of the message; mccs.c's framing check
         * drops that run and the host retries. */
        rx_src = SR(IIC_SUB_IN);
        SR_CLR(IIC_IRQ_CONTROL2, IIC_IRQ_CONTROL2_DATA_BUF_WEN);
        IIC_CLR(IIC_STATUS_SUB_I);
    }
    if (st & (IIC_STATUS_D_IN_I | IIC_STATUS_STOP_I)) {
        while (!(SR(IIC_STATUS2) & IIC_STATUS2_IIC_BUF_EMPTY)) {
            u8 b = SR(IIC_DATA_IN);
            if (rx_len < DDCCI_RX_MAX)
                rx[rx_len++] = b;
        }
        IIC_CLR(IIC_STATUS_D_IN_I);
    }
    if (st & IIC_STATUS_STOP_I) {
        IIC_CLR(IIC_STATUS_STOP_I);
        SR(IIC_STATUS2) = 0;                            /* clear buffer over/underflow */
        n = rx_len;
        rx_len = 0;
        if (n) {
            *src = rx_src;
            for (i = 0; i < n; i++)
                buf[i] = rx[i];
            return n;
        }
    }
    return 0;
}

void ddcci_hw_reply(const u8 *buf, u8 n)
{
    u8 i;

    if (n > DDCCI_TX_MAX)
        n = DDCCI_TX_MAX;
    SR_SET(IIC_IRQ_CONTROL2, IIC_IRQ_CONTROL2_RST_DATA_BUF);
    SR_SET(IIC_IRQ_CONTROL2, IIC_IRQ_CONTROL2_DATA_BUF_WEN);     /* FIFO now transmits */
    for (i = 0; i < n; i++)
        SR(IIC_DATA_OUT) = buf[i];
}
