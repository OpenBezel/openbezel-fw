/* DDC/CI slave engine (page FF: IIC_* registers), polled.
 *
 * The RTD2556 has a hardware I2C slave for DDC/CI: address decode
 * (IIC_SET_SLAVE), the first byte after the address latched as a sub-address
 * (for DDC/CI that is the 0x51 source byte), a 16-byte data FIFO shared
 * between receive and transmit (IIC_IRQ_CONTROL2 DATA_BUF_WEN picks the
 * direction), and a hardware NULL-MESSAGE responder that answers a host read
 * with DDCCI_NULL_BYTE1..3 whenever the transmit side is empty.
 *
 * Deliberate design choices:
 *  - Polled from the main loop, not driven from external interrupt 1.
 *  - NO clock stretching.  Holding SCL low on the DDC channel from the end of
 *    a host message until the main loop has queued the reply would, if the
 *    firmware ever hung in that window, hold the HDMI DDC bus down
 *    indefinitely - and that bus is the ONLY reflash path.
 *    A host that reads before the reply is ready gets the null message and
 *    retries, which is exactly what DDC/CI specifies.
 *
 * Replies are kept to <= 16 bytes so they fit the FIFO (mccs.c fragments the
 * capabilities string accordingly). */
#ifndef RTD2556_DDCCI_HW_H
#define RTD2556_DDCCI_HW_H

#include "ob_types.h"

#define DDCCI_RX_MAX 32
#define DDCCI_TX_MAX 16

void ddcci_hw_init(void);

/* Poll the engine.  Returns the number of bytes received up to a STOP (0 if
 * none): one message after its source byte (length byte, payload, checksum),
 * or several back-to-back if the poll was slow - the source bytes of the later
 * ones are not in the FIFO (the engine latches them as sub-addresses).  The
 * last source byte seen is in *src. */
u8 ddcci_hw_poll(u8 *src, u8 *buf);

/* Queue a reply for the host's next read.  `buf` holds the complete reply as it
 * appears on the wire after the 0x6F address byte (source 0x6E, length,
 * payload, checksum). */
void ddcci_hw_reply(const u8 *buf, u8 n);

#endif
