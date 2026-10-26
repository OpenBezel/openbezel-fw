/* RTD2556 scaler page 0xB8 */
#ifndef RTD2556_REGS_PAGE_B8_H
#define RTD2556_REGS_PAGE_B8_H

/* --- D1 Port DisplayPort 1.2 Digital MAC(Page B8) --- */
#define D1_MAC_DIG_RESET_CTRL     0xB800
#define   D1_MAC_DIG_RESET_CTRL_DPF_RESET                0x80  /* [7] Display format block reset */
#define D1_STHD_CTRL_1            0xB802
#define   D1_STHD_CTRL_1_ST_LOAD_BY_ACT                  0x80  /* [7] 1 :load new stream payload by receiving ACT */
#define D1_PG_CTRL_0              0xB820
#define   D1_PG_CTRL_0_COMPONENT_FORMAT_RGB              0x00  /* 00 */
#define   D1_PG_CTRL_0_COLORBIT                          0x07  /* [2:0] color depth */
#define   D1_PG_CTRL_0_COLORBIT_SHIFT                    0
#define D1_PG_CTRL_1              0xB821
#define   D1_PG_CTRL_1_BE_OCR                            0x40  /* [6] 0: "BE" not occur (no video) */
#define   D1_PG_CTRL_1_VFIFO_EMPTY                       0x20  /* [5] video FIFO empty flag */
#define   D1_PG_CTRL_1_VFIFO_UDFL                        0x10  /* [4] video FIFO underflow flag */
#define   D1_PG_CTRL_1_VFIFO_FULL                        0x08  /* [3] video FIFO full flag */
#define   D1_PG_CTRL_1_VFIFO_OVFL                        0x04  /* [2] video FIFO overflow flag */
#define   D1_PG_CTRL_1_VFIFO_OVFL_PRI                    0x01  /* [0] video fifo overflow assert prior to underflow */
#define D1_DPF_CTRL_0             0xB830
#define   D1_DPF_CTRL_0_DISP_EN                          0x80  /* [7] Start to generate display format */
#define   D1_DPF_CTRL_0_DISP_DBUF                        0x40  /* [6] Double buffer of the display format. */
#define D1_DP_OUTPUT_CTRL         0xB831
#define   D1_DP_OUTPUT_CTRL_ROUT_EN                      0x04  /* [2] R output enable. (It will be also disabled when video watch-dog occ... */
#define   D1_DP_OUTPUT_CTRL_GOUT_EN                      0x02  /* [1] G output enable. (It will be also disabled when video watch-dog occ... */
#define   D1_DP_OUTPUT_CTRL_BOUT_EN                      0x01  /* [0] B output enable. (It will be also disabled when video watch-dog occ... */
#define D1_EVBLK2VS_H             0xB832
#define D1_EVBLK2VS_M             0xB833
#define D1_EVBLK2VS_L             0xB834
#define D1_OVBLK2VS_H             0xB835
#define D1_OVBLK2VS_M             0xB836
#define D1_OVBLK2VS_L             0xB837
#define D1_MN_DPF_HTT_M           0xB841
#define D1_MN_DPF_HST_M           0xB843
#define D1_MN_DPF_HWD_M           0xB845
#define D1_MN_DPF_HSW_M           0xB847
#define D1_MN_DPF_VTT_M           0xB849
#define D1_MN_DPF_VST_M           0xB84B
#define D1_MN_DPF_VHT_M           0xB84D
#define D1_MN_DPF_VSW_M           0xB84F
#define D1_MN_DPF_BG_RED_M        0xB851
#define D1_MN_DPF_BG_RED_L        0xB852
#define D1_MN_DPF_BG_GRN_M        0xB853
#define D1_MN_DPF_BG_GRN_L        0xB854
#define D1_MN_DPF_BG_BLU_M        0xB855
#define D1_MN_DPF_BG_BLU_L        0xB856
#define D1_INTERLACE_MODE_CONFIG  0xB857
#define   D1_INTERLACE_MODE_CONFIG_FAKE_INTERLACE_DETECT 0x01  /* [0] if 2 field sometimes have the same numbers of BS, it may not be a i... */
#define D1_MN_MEAS_CTRL           0xB858
#define   D1_MN_MEAS_CTRL_MEAS_EN                        0x80  /* [7] Measurement enable */
#define   D1_MN_MEAS_CTRL_MEAS_DONE                      0x40  /* [6] Measurement finished. */
#define   D1_MN_MEAS_CTRL_MEAS_POPUP                     0x20  /* [5] Pop up measure result. */
#define   D1_MN_MEAS_CTRL_V_BS2BS_LENGTH                 0x1F  /* [4:0] Bit20~16 of the length between 2 vertical BS. (Counted by crystal c... */
#define D1_MN_MEAS_VLN_M          0xB859
#define D1_MN_MEAS_VLN_L          0xB85A

/* --- D1 Port DisplayPort 1.2 Digital MAC PLL(Page B8) --- */
#define D1_MN_SCLKG_CTRL          0xB8A0
#define   D1_MN_SCLKG_CTRL_SCLKG_PLL_IN_SEL              0xC0  /* [7:6] PLL Input Clock Selector. */
#define   D1_MN_SCLKG_CTRL_SCLKG_PLL_IN_SEL_SHIFT        6
#define   D1_MN_SCLKG_CTRL_SCLKG_DBUF                    0x10  /* [4] Double Buffer of All Stream Clk Setting. */
#define D1_PB8_MN_SCLKG_DIVM      0xB8A1
#define D1_PB8_MN_SCLKG_DIVN      0xB8A2
#define D1_MN_SCLKG_DIVS          0xB8A3
#define D1_MN_SCLKG_OFFS_H        0xB8A4
#define   D1_MN_SCLKG_OFFS_H_SCLKG_LD_D_CODE             0x80  /* [7] Enable This Bit to Load Offset Value */
#define   D1_MN_SCLKG_OFFS_H_SCLKG_PS_OLD                0x40  /* [6] Phase Swallow Tracking Old Mode Enable */
#define   D1_MN_SCLKG_OFFS_H_SCLKG_MN_BL_EN              0x20  /* [5] MN Value Block Enable */
#define   D1_MN_SCLKG_OFFS_H_SCLKG_MN_BT                 0x18  /* [4:3] Strength of Blocking MN Input. */
#define   D1_MN_SCLKG_OFFS_H_SCLKG_PLL_OFFS              0x07  /* [2:0] Bit18~16 of Offset Value. (This Is 2's Complement.) */
#define D1_MN_SCLKG_OFFS_M        0xB8A5
#define D1_MN_SCLKG_OFFS_L        0xB8A6
#define D1_MN_SCLKG_TRK_CTRL      0xB8A7
#define   D1_MN_SCLKG_TRK_CTRL_SCLKG_VS_EN               0x40  /* [6] Enable VS/HS Tracking */
#define   D1_MN_SCLKG_TRK_CTRL_VS_HS_SEL                 0x10  /* [4] VS Or HS Tracking Select */
#define   D1_MN_SCLKG_TRK_CTRL_PE_TRAIL_NEW_MODE         0x08  /* [3] Clear Lead Flag (new Mode) */
#define D1_MN_SCLKG_TRK_MN_I_H    0xB8A8
#define   D1_MN_SCLKG_TRK_MN_I_H_SCLKG_VS_PERX_EN        0xF0  /* [7:4] Enable VS/HS Tracking Every (x+1) VS/HS. */
#define   D1_MN_SCLKG_TRK_MN_I_H_SCLKG_VS_PERX_EN_SHIFT  4
#define D1_MN_SCLKG_TRK_VS_I_M    0xB8B1
#define D1_MN_SCLKG_TRK_VS_I_L    0xB8B2
#define D1_MN_SCLKG_TRK_VS_P_H    0xB8B3
#define D1_MN_SCLKG_TRK_VS_P_M    0xB8B4
#define D1_MN_SCLKG_TRK_VS_P_L    0xB8B5
#define D1_MN_SCLKG_TRK_VS_PE     0xB8B6
#define D1_MN_SCLKG_TRK_VS_NLOCK  0xB8B7
#define   D1_MN_SCLKG_TRK_VS_NLOCK_SCLKG_VS_NLOCK        0x80  /* [7] The Flag of The PE Larger Than The Phase Error Threshold, (This Bit... */
#define D1_MN_SCLKG_SDM_CTRL      0xB8B8
#define   D1_MN_SCLKG_SDM_CTRL_OCLK_EN                   0x40  /* [6] Output Pixel Clock (to I Domain)clock Enable */
#define   D1_MN_SCLKG_SDM_CTRL_SDM_EN                    0x10  /* [4] Enable SDM (phase Swallow) */
#define   D1_MN_SCLKG_SDM_CTRL_HS_TRACKING_NEW_MODE      0x04  /* [2] active when vs_hs_sel = 1, hs_tracking_mode = 0 */
#define   D1_MN_SCLKG_SDM_CTRL_HS_TRACKING_MODE          0x02  /* [1] active when vs_hs_sel = 1 */
#define D1_MN_SCLKG_SDM_TEST      0xB8B9
#define   D1_MN_SCLKG_SDM_TEST_MANUAL_VBID_SEL           0x80  /* [7] 0: refer to source vb_id[0] */
#define D1_PB8_MN_SCLKG_PLL_PWR   0xB8BD
#define   D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_PWDN          0x80  /* [7] DPLL Power Down */
#define   D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_CALSW         0x10  /* [4] Calibration Validated */
#define   D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_CALLCH        0x08  /* [3] Latch Calibration */
#define   D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_CMPEN         0x04  /* [2] CMP Enable */
#define   D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_O             0x03  /* [1:0] DPLL Output Divider */
#define   D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_O_SHIFT       0
#define D1_PB8_MN_SCLKG_PLL_CHP   0xB8BE
#define   D1_PB8_MN_SCLKG_PLL_CHP_REG_DPLL_RS            0xE0  /* [7:5] DPLL Loop Filter Resistor Contorl */
#define   D1_PB8_MN_SCLKG_PLL_CHP_REG_DPLL_RS_SHIFT      5
#define   D1_PB8_MN_SCLKG_PLL_CHP_REG_DPLL_CS            0x18  /* [4:3] DPLL Loop Filter Capacitor Control */
#define   D1_PB8_MN_SCLKG_PLL_CHP_REG_DPLL_CS_SHIFT      3
#define D1_PB8_MN_SCLKG_PLL_WD    0xB8BF
#define   D1_PB8_MN_SCLKG_PLL_WD_REG_DPLL_WDO            0x80  /* [7] DPLL WD Status Register */
#define   D1_PB8_MN_SCLKG_PLL_WD_REG_DPLL_VCOMD          0x18  /* [4:3] DPLL VCO Default Mode */
#define   D1_PB8_MN_SCLKG_PLL_WD_REG_DPLL_VCOMD_SHIFT    3
#define   D1_PB8_MN_SCLKG_PLL_WD_REG_DPLL_RESERVE        0x04  /* [2] DPLL Phase Swallow Circuit */
#define   D1_PB8_MN_SCLKG_PLL_WD_REG_DPLL_STOP           0x02  /* [1] DPLL Frequency Tuning */
#define D1_PB8_MN_SCLKG_PLL_INSEL 0xB8C0
#define   D1_PB8_MN_SCLKG_PLL_INSEL_REG_DPLL_VO2         0x80  /* [7] DPLL CAL OUT2 */
#define   D1_PB8_MN_SCLKG_PLL_INSEL_REG_DPLL_CAL         0x30  /* [5:4] DPLL Calibrated VCO Code */
#define   D1_PB8_MN_SCLKG_PLL_INSEL_REG_DPLL_CAL_SHIFT   4
#define D1_HS_TRACKING_NEW_MODE1  0xB8C2
#define D1_VBID_MAN_MADE          0xB8C7
#define   D1_VBID_MAN_MADE_BE_START_NUM                  0xF0  /* [7:4] BE START NUM */
#define   D1_VBID_MAN_MADE_BE_START_NUM_SHIFT            4
#define   D1_VBID_MAN_MADE_BE_END_NUM                    0x0F  /* [3:0] BE END NUM */
#define   D1_VBID_MAN_MADE_BE_END_NUM_SHIFT              0

/* --- D1 Port DisplayPort 1.2 Digital MAC(Page B8) --- */
#define D1_DP_RSV0                0xB8F0

#endif /* RTD2556_REGS_PAGE_B8_H */
