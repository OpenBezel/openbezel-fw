/* osdsim: compile the firmware's SDCC C with a host compiler.  Force-included
 * (-include) before every firmware source. */
#ifndef OSDSIM_HOST_SDCC_H
#define OSDSIM_HOST_SDCC_H

#define __xdata
#define __code
#define __data
#define __idata
#define __pdata
#define __near
#define __far
#define __at(x)
#define __banked
#define __nonbanked
#define __reentrant
#define __naked
#define __critical
#define __interrupt(x)
#define __using(x)
#define __bit unsigned char
#define __sfr unsigned char
#define __sbit unsigned char

/* mkimage's build-info block (CODE 0xFF04) does not exist on the host */
#define OB_FW_VERSION "osdsim"

#endif
