/* HDCP 1.4 receiver keys for both inputs (HDMI D3, DP D1).
 *
 * The board's device keys and KSV sit in flash bank 1 (KSV 0x01306F,
 * the 320-byte key block 0x013074), which is never written.  They are moved
 * flash -> receiver key ports by the burst engine only: no key byte ever
 * passes through the CPU, RAM, a log or this repository. */
#ifndef RTD2556_HDCP_H
#define RTD2556_HDCP_H

#include "ob_types.h"

extern u8 hdcp_ready;                   /* keys loaded: advertise HDCP (DP Bcaps, CP_READY) */

void hdcp_init(void) __banked;          /* after the ISP window; before the DP port's HPD */

#endif
