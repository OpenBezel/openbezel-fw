/* RTD2556 scaler page 0xFE */
#ifndef RTD2556_REGS_PAGE_FE_H
#define RTD2556_REGS_PAGE_FE_H

/* --- EMCU GPIO Control (Page FE) --- */
#define PORT40_PIN_REG     0xFE00
#define PORT42_PIN_REG     0xFE02
#define PORT43_PIN_REG     0xFE03
#define PORT46_PIN_REG     0xFE06
#define PORT47_PIN_REG     0xFE07
#define PORT50_PIN_REG     0xFE08
#define PORT51_PIN_REG     0xFE09
#define PORT52_PIN_REG     0xFE0A
#define PORT62_PIN_REG     0xFE12
#define PORT64_PIN_REG     0xFE14
#define PORT65_PIN_REG     0xFE15
#define PORT71_PIN_REG     0xFE19
#define PORT77_PIN_REG     0xFE1F
#define PORT82_PIN_REG     0xFE22
#define PORT86_PIN_REG     0xFE26
#define PORT91_PIN_REG     0xFE29
#define PORT92_PIN_REG     0xFE2A
#define PORT94_PIN_REG     0xFE2C
#define PORT95_PIN_REG     0xFE2D
#define PORTA3_PIN_REG     0xFE33
#define PORTA4_PIN_REG     0xFE34

/* --- EMCU DDC Part 0 (Page FE) --- */
#define DDC2_DDC_ENABLE    0xFE64
#define DDC2_DDC_CONTROL_1 0xFE65
#define DDC2_DDC_CONTROL_2 0xFE66
#define DDC2_EDID_IRQ      0xFE69
#define   DDC2_EDID_IRQ_DDC2_FORCE_NACK              0x20  /* [5] In DDC2B Mode, Force DDC2 SDA Output 1(NACK) */
#define DDC3_DDC_ENABLE    0xFE6D
#define DDC3_DDC_CONTROL_1 0xFE6E
#define   DDC3_DDC_CONTROL_1_DDC3_DBN_CLK_SEL        0xC0  /* [7:6] De-bounce Clock Divider */
#define   DDC3_DDC_CONTROL_1_DDC3_DBN_CLK_SEL_SHIFT  6
#define   DDC3_DDC_CONTROL_1_DDC3_STOP_DBN_SEL       0x30  /* [5:4] De-bounce Sda Stage */
#define   DDC3_DDC_CONTROL_1_DDC3_STOP_DBN_SEL_SHIFT 4
#define DDC3_DDC_CONTROL_2 0xFE6F
#define DDC3_EDID_IRQ      0xFE72
#define   DDC3_EDID_IRQ_DDC3_FORCE_NACK              0x20  /* [5] In DDC2B Mode, Force DDC3 SDA Output 1(NACK) */

/* --- EMCU DDCRAM Settings (Page FE) --- */
#define DDCRAM_ST_ADDRESS  0xFEB1
#define   DDCRAM_ST_ADDRESS_DDCRAM_5_ST              0x20  /* [5] DDCRAM5 Start Address: */
#define   DDCRAM_ST_ADDRESS_DDCRAM_4_ST              0x10  /* [4] DDCRAM4 Start Address: */
#define   DDCRAM_ST_ADDRESS_DDCRAM_3_ST              0x08  /* [3] DDCRAM3 Start Address: */
#define   DDCRAM_ST_ADDRESS_DDCRAM_2_ST              0x04  /* [2] DDCRAM2 Start Address: */
#define   DDCRAM_ST_ADDRESS_DDCRAM_1_ST              0x02  /* [1] DDCRAM1 Start Address: */
#define   DDCRAM_ST_ADDRESS_DDCRAM_0_ST              0x01  /* [0] DDCRAM0 Start Address: */

#endif /* RTD2556_REGS_PAGE_FE_H */
