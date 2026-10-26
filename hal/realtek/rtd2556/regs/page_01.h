/* RTD2556 scaler page 0x01 */
#ifndef RTD2556_REGS_PAGE_01_H
#define RTD2556_REGS_PAGE_01_H

/* --- DISPLAY PLL (Page 1) --- */
#define DPLL_M                    0x01BF
#define DPLL_N                    0x01C0
#define DPLL_CRNT                 0x01C1
#define   DPLL_CRNT_REG_DPLL_RS_13K             0x60  /* 011 */
#define   DPLL_CRNT_REG_DPLL_CS_66PF            0x18  /* 11 */
#define   DPLL_CRNT_REG_DPLL_IP                 0x07  /* [2:0] DPLL Charge Pump Current Control */
#define   DPLL_CRNT_REG_DPLL_IP_SHIFT           0

/* --- DCLK Spread Spectrum (Page 1) --- */
#define DPLL_WD                   0x01C2
#define   DPLL_WD_REG_DPLL_WDSET                0x20  /* [5] DPLL WD Set */
#define   DPLL_WD_REG_DPLL_PWDN                 0x01  /* [0] DPLL power down */
#define DPLL_CAL                  0x01C3
#define   DPLL_CAL_REG_DPLL_VCOMD               0xC0  /* [7:6] DPLL VCO Default Mode */
#define   DPLL_CAL_REG_DPLL_VCOMD_SHIFT         6
#define   DPLL_CAL_REG_DPLL_CALBP               0x20  /* [5] Bypass calibration */
#define   DPLL_CAL_REG_DPLL_CALSW               0x10  /* [4] Calibration Validated */
#define   DPLL_CAL_REG_DPLL_CALLCH              0x08  /* [3] Latch Calibration */
#define   DPLL_CAL_REG_DPLL_CMPEN               0x04  /* [2] CMP enable */
#define DCLK_FINE_TUNE_OFFSET_MSB 0x01C4
#define DCLK_FINE_TUNE_OFFSET_LSB 0x01C5
#define DCLK_SPREAD_SPECTRUM      0x01C6
#define   DCLK_SPREAD_SPECTRUM_DCLK_READY       0x04  /* [2] Spread Spectrum Setting Ready for Writing (Auto Clear) */
#define FIXED_LAST_LINE_CTRL      0x01CA
#define   FIXED_LAST_LINE_CTRL_DCLK_DDS_EN      0x01  /* [0] Enable DDS Spread Spectrum Output Function */
#define DCLK_SSC_COUNT            0x01D6
#define   DCLK_SSC_COUNT_DCLK_FMDIV_CNT_EN      0x80  /* [7] Spread Spectrum FMDIV Count Manual Mode */
#define   DCLK_SSC_COUNT_DCLK_FMDIV_CNT         0x7E  /* [6:1] The Numbers of Count for Spread Spectrum FMDIV: */
#define   DCLK_SSC_COUNT_DCLK_FMDIV_CNT_SHIFT   1
#define DPLL_M_N_MSB              0x01D9
#define   DPLL_M_N_MSB_DPLL_M_N_SET_MODE        0x80  /* [7] DPLL M, N code double buffer write mode */
#define DPLL_OUTPUT_CLK_DIV       0x01DA
#define   DPLL_OUTPUT_CLK_DIV_PIXELCLK_DIV_DIV2 0x01  /* 01 */
#define DCLK_GATED_CTRL2          0x01DC
#define   DCLK_GATED_CTRL2_VBYO_CLKEN           0x02  /* [1] Enable V by One and DPTX 8 lane clk */
#define   DCLK_GATED_CTRL2_DPTX_CLKEN           0x01  /* [0] Enable DPTX MST clk */

/* --- Multiply PLL for Input Crystal (Page 1) --- */
#define M2PLL_M                   0x01E0
#define M2PLL_N                   0x01E1
#define M2PLL_CRNT                0x01E4
#define   M2PLL_CRNT_REG_DPLL_RS_13K            0x60  /* 011 */
#define   M2PLL_CRNT_REG_DPLL_CS_58PF           0x10  /* 10 */
#define   M2PLL_CRNT_REG_DPLL_IP                0x07  /* [2:0] DPLL Charge Pump Current Control */
#define   M2PLL_CRNT_REG_DPLL_IP_SHIFT          0
#define M2PLL_WD                  0x01E5
#define   M2PLL_WD_REG_DPLL_WDSET               0x20  /* [5] DPLL WD Set */
#define   M2PLL_WD_REG_DPLL_VCORSTB             0x02  /* [1] DPLL VCO reseet */
#define   M2PLL_WD_REG_DPLL_PWDN                0x01  /* [0] DPLL power down */

#endif /* RTD2556_REGS_PAGE_01_H */
