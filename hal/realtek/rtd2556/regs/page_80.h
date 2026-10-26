/* RTD2556 scaler page 0x80 */
#ifndef RTD2556_REGS_PAGE_80_H
#define RTD2556_REGS_PAGE_80_H

/* --- GDI Clock Divider (Page 80) --- */
#define GDI_CLK_DIVIDER 0x8006
#define   GDI_CLK_DIVIDER_GDI_CLK_SEL       0x10  /* [4] GDI Clock Select */
#define   GDI_CLK_DIVIDER_GDI_CLK_DIV       0x0F  /* [3:0] Divider of Fxtal * M2PLL_M / M2PLL_N Clock. Enable When Bit 4 Is Se... */
#define   GDI_CLK_DIVIDER_GDI_CLK_DIV_SHIFT 0

/* --- XTAL (Page 80) --- */
#define XTAL_CTRL1      0x800F
#define   XTAL_CTRL1_REG_G2B                0x10  /* [4] Gain Control */
#define   XTAL_CTRL1_REG_G1B                0x08  /* [3] Gain Control */

#endif /* RTD2556_REGS_PAGE_80_H */
