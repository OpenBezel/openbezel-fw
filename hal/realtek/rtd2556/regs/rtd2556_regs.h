/* RTD2556 register names - the one header drivers include.
 *
 * regs/page_XX.h hold the registers OpenBezel uses.
 *
 * Naming: <REGISTER> is the XDATA address (pages are flat-mapped), and
 * <REGISTER>_<FIELD> is the field's bit mask (plus _SHIFT for multi-bit
 * fields, and <REGISTER>_<FIELD>_<VALUE> for its listed codes, already in
 * place).  Use them through sreg.h:  SR(REG), SR_SET(REG, REG_FIELD), ...
 */
#ifndef RTD2556_REGS_H
#define RTD2556_REGS_H

#include "sreg.h"

#include "page_00.h"   /* chip control, IRQ, power, watchdog, display format port, gamma, dither, OSD port */
#include "page_01.h"   /* PLLs: APLL, DPLL (display), M2PLL */
#include "page_05.h"
#include "page_06.h"
#include "page_07.h"   /* DCC/ICM, D-domain pattern generator */
#include "page_10.h"   /* pin share */
#include "page_14.h"
#include "page_16.h"
#include "page_30.h"
#include "page_31.h"   /* I/D/IMD double-buffer control */
#include "page_36.h"   /* CRC engines */
#include "page_38.h"   /* PHY0: DP TX digital PHY + CMU */
#include "page_39.h"   /* LVDS/eDP display conversion */
#include "page_3A.h"   /* OSD frame control */
#include "page_40.h"   /* sync/measure clock dividers */
#include "page_55.h"   /* PHY0 analog (TXCD) */
#include "page_6F.h"   /* ADI power control */
#include "page_72.h"   /* HDMI RX D3: HDMI 2.0 control, TMDS decoder, HDCP, frequency detector */
#include "page_77.h"   /* HDMI RX D3: MHL MAC */
#include "page_7A.h"   /* DP RX D1 (USB-C): GDI PHY (CMU, CDR, DFE) - D1_ names */
#include "page_7C.h"   /* HDMI RX D3: GDI PHY (CMU, CDR, DFE) */
#include "page_80.h"   /* GDI clock divider, XTAL */
#include "page_9C.h"   /* TX1 MAC / HPD / stream (BB layout) */
#include "page_9D.h"   /* TX1 AUX (BD layout) */
#include "page_B0.h"   /* DP RX D1: digital PHY, HDCP */
#include "page_B8.h"   /* DP RX D1: MAC, stream clock PLL */
#include "page_B9.h"   /* DP RX D1: MSA / SEC data, DP audio */
#include "page_BA.h"   /* DP RX D1: AUX, DPCD access port */
#include "page_BF.h"   /* stream clock generator */
#include "page_FD.h"   /* cache controller */
#include "page_FE.h"   /* EMCU GPIO */
#include "page_FF.h"   /* XFR: MCU clocks, ISP, LSADC, DDC */

/* keyholes: registers behind an index / data port, one header per data port */
#include "port_0025.h"   /* scale-down */
#include "port_002B.h"   /* display format (DTG) */
#include "port_0031.h"   /* FIFO window */
#include "port_0034.h"   /* scale factor */
#include "port_005D.h"   /* sync processor */
#include "port_0065.h"   /* contrast / brightness */
#include "port_07D9.h"   /* DCR */
#include "port_723A.h"   /* HDMI SCDC */
#include "port_72C4.h"   /* HDCP */
#include "port_72CA.h"   /* HDMI */
#include "port_BAC4.h"   /* DP RX DPCD */

/* D_DITHER_COMMON_CTRL1 / I_DITHER_COMMON_CTRL1_SETA dither_access [7:6]: which table
 * the data port reaches */
#define   DITHER_ACCESS_SEQ_TABLE         0x40  /* sequence table */
#define   DITHER_ACCESS_DITHER_TABLE      0x80  /* dither table */
#define   DITHER_ACCESS_TEMPORAL          0xC0  /* temporal table */

/* OSD_WRITE_OPTION sram_sel [3:2] */
#define   OSD_SRAM_WINDOW                 0x00  /* window control registers */
#define   OSD_SRAM_FONT                   0x08  /* font select and font map SRAM */

/* DPTX_ML_PAT_SEL tx_ml_pat_sel [7:4] */
#define   DPTX_ML_PAT_SEL_TX_ML_PAT_SEL_VIDEO 0x40  /* video, double-buffered by vsync */

#define DPTX_TOP_RSV6_TX1          0x9CF3       /* bit0 cleared by stock's stream-on */

#endif
