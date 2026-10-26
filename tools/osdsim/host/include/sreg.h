/* osdsim override of hal/realtek/common/sreg.h: the scaler register space is a
 * host array (osdsim_sr), which the chip renderer reads back. */
#ifndef HAL_REALTEK_SREG_H
#define HAL_REALTEK_SREG_H

#include "ob_types.h"

extern volatile u8 osdsim_sr[65536];
#define SR(addr) (osdsim_sr[(u16)(addr)])

#define SR_SET(addr, mask) (SR(addr) |= (mask))
#define SR_CLR(addr, mask) (SR(addr) &= (u8)~(mask))
#define SR_WRF(addr, mask, val) (SR(addr) = (SR(addr) & (u8)~(mask)) | ((val) & (mask)))
#define SR_PULSE_HI(addr, mask) do { SR(addr) |= (mask); SR(addr) &= (u8)~(mask); } while (0)
#define SR_PULSE_LO(addr, mask) do { SR(addr) &= (u8)~(mask); SR(addr) |= (mask); } while (0)

#endif
