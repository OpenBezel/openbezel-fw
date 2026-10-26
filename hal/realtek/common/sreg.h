/* Scaler register access (Realtek RTD25xx family).
 *
 * With scalar_addr_remapping on (board_early_init), every scaler page is
 * flat-mapped into XDATA: page P offset O = XDATA address 0xPPOO.  So a
 * register name from the generated headers (rtd2556_regs.h) is simply its
 * XDATA address, and SR(REG) is an lvalue byte.
 */
#ifndef HAL_REALTEK_SREG_H
#define HAL_REALTEK_SREG_H

#include "ob_types.h"

#define SR(addr) (*(volatile __xdata u8 *)(addr))

#define SR_SET(addr, mask) (SR(addr) |= (mask))
#define SR_CLR(addr, mask) (SR(addr) &= (u8)~(mask))
/* value v of a multi-bit field, positioned: FV(CMU_07_REG_SEL_DIV, 3) */
#define FV(field, v) ((u8)((v) << field##_SHIFT))
/* replace the bits under mask with val (val already positioned) */
#define SR_WRF(addr, mask, val) (SR(addr) = (SR(addr) & (u8)~(mask)) | ((val) & (mask)))
/* pulse: set then clear (or clear then set) a bit */
#define SR_PULSE_HI(addr, mask) do { SR(addr) |= (mask); SR(addr) &= (u8)~(mask); } while (0)
#define SR_PULSE_LO(addr, mask) do { SR(addr) &= (u8)~(mask); SR(addr) |= (mask); } while (0)

#endif
