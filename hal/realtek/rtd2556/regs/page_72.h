/* RTD2556 scaler page 0x72 */
#ifndef RTD2556_REGS_PAGE_72_H
#define RTD2556_REGS_PAGE_72_H

/* --- D3 Port HDMI2.0 Function (Page 72) --- */
#define HDMI_CTRL_00          0x7200
#define   HDMI_CTRL_00_HDMI_20_EN               0x01  /* [0] HDMI 2.0 Function Enable Bit */
#define HDMI_CTRL_01          0x7201
#define   HDMI_CTRL_01_HDMI_FMT_RST             0x80  /* [7] HDMI/DVI Format Flag Reset */
#define   HDMI_CTRL_01_SCRAM_AUTODET_EN         0x20  /* [5] Scrambling Enable Bit (According to 0x01[3]) */
#define   HDMI_CTRL_01_SCRAM_DET                0x08  /* [3] Sink Detect Scrambled Data */
#define   HDMI_CTRL_01_SSCP_NUM                 0x07  /* [2:0] SSCP Set Number */
#define   HDMI_CTRL_01_SSCP_NUM_SHIFT           0
#define HDMI_CTRL_0B          0x720B
#define   HDMI_CTRL_0B_ERR_DET_EN_CH0           0x80  /* [7] Character Error Detection Enable for Ch0 */
#define   HDMI_CTRL_0B_ERR_DET_EN_CH1           0x40  /* [6] Character Error Detection Enable for Ch1 */
#define   HDMI_CTRL_0B_ERR_DET_EN_CH2           0x20  /* [5] Character Error Detection Enable for Ch2 */
#define   HDMI_CTRL_0B_VALID_CH0                0x10  /* [4] Ch0 Valig Flag */
#define   HDMI_CTRL_0B_VALID_CH1                0x08  /* [3] Ch1 Valig Flag */
#define   HDMI_CTRL_0B_VALID_CH2                0x04  /* [2] Ch2 Valig Flag */
#define HDMI_CTRL_0C          0x720C
#define   HDMI_CTRL_0C_ERR_POPUP                0x80  /* [7] Error Counter Popup Enable Bit */
#define HDMI_CTRL_14          0x7214
#define   HDMI_CTRL_14_ERR_CNT_BOUND            0x0F  /* [3:0] Error Counter Threshold for Lock/Unlock Flag */
#define HDMI_CTRL_15          0x7215
#define HDMI_CTRL_17          0x7217
#define   HDMI_CTRL_17_CED_UPDATE_CH0           0x40  /* [6] CH0 Realtime CED_Update Flag */
#define   HDMI_CTRL_17_LOCK_TRA_CH2             0x04  /* [2] CH2 Lock Transition Flag */
#define   HDMI_CTRL_17_LOCK_TRA_CH1             0x02  /* [1] CH1 Lock Transition Flag */
#define   HDMI_CTRL_17_LOCK_TRA_CH0             0x01  /* [0] CH0 Lock Transition Flag */
#define HDMI_CTRL_18          0x7218
#define   HDMI_CTRL_18_LOCK_IE_CH2              0x04  /* [2] IRQ Enable Bit for CH2 Lock Transition Flag */
#define   HDMI_CTRL_18_LOCK_IE_CH1              0x02  /* [1] IRQ Enable Bit for CH1 Lock Transition Flag */
#define   HDMI_CTRL_18_LOCK_IE_CH0              0x01  /* [0] IRQ Enable Bit for CH0 Lock Transition Flag */
#define HDMI_CTRL_19          0x7219
#define   HDMI_CTRL_19_SCDC_I2CS_EN             0x80  /* [7] SCDC I2C Slave Module Enable Bit */
#define HDMI_CTRL_1A          0x721A
#define   HDMI_CTRL_1A_XTAL_SEL                 0x08  /* [3] 1ms unit reference crystal clock selection */
#define HDMI_CTRL_1B          0x721B
#define   HDMI_CTRL_1B_RD_REQ_SET               0x3F  /* [5:0] Read Request Set Point Timer Setting */
#define HDMI_CTRL_1C          0x721C
#define   HDMI_CTRL_1C_RR_IGNORE_EN             0x04  /* [2] RR Ignore Enable */
#define HDMI_CTRL_38          0x7238
#define   HDMI_CTRL_38_AUTO_SET_SCRAM_ST        0x10  /* [4] Auto Set Scrambling_Status in SCDC Register (SCDC 0x21[0]) */
#define   HDMI_CTRL_38_SCDC_AUTO_INC            0x01  /* [0] SCDC Accessing Port Auto Increase(For Hist Side) */
#define HDMI_CTRL_39          0x7239
#define HDMI_CTRL_3A          0x723A

/* --- D3 Port Overall HDMI System Function Block (Page 72) --- */
#define TMDS_CTRL             0x72A4
#define   TMDS_CTRL_BCD                         0x80  /* [7] Blue-Channel Detect Whether Data_enable Is High 128 Clk (DE Low 128... */
#define   TMDS_CTRL_GCD                         0x40  /* [6] Green-Channel Detect Whether Data_enable Is High 128 Clk (DE Low 12... */
#define   TMDS_CTRL_RCD                         0x20  /* [5] Red-Channel Detect Whether Data_enable Is High 128 Clk (DE Low 128 ... */
#define   TMDS_CTRL_HO                          0x10  /* [4] The Source of Hsync Is from HDCP. (write 1 Clear) */
#define   TMDS_CTRL_YO                          0x08  /* [3] The Source of Vsync Is from HDCP. */
#define TMDS_OUTCTL           0x72A6
#define   TMDS_OUTCTL_AOE                       0x80  /* [7] Auto Output Enable (video Output Fsm Mode) */
#define   TMDS_OUTCTL_TRCOE                     0x40  /* [6] TMDS R Channel Output Enable */
#define   TMDS_OUTCTL_TGCOE                     0x20  /* [5] TMDS G Channel Output Enable */
#define   TMDS_OUTCTL_TBCOE                     0x10  /* [4] TMDS B Channel Output Enable */
#define   TMDS_OUTCTL_OCKE                      0x08  /* [3] OCLK Enable */
#define TMDS_PWDCTL           0x72A7
#define   TMDS_PWDCTL_DEO                       0x80  /* [7] DE-only: Generate VS/HS from DE Signal */
#define   TMDS_PWDCTL_BRCW                      0x40  /* [6] B/R Channel Swap */
#define   TMDS_PWDCTL_PNSW                      0x20  /* [5] P/N Swap */
#define   TMDS_PWDCTL_ICCAF                     0x10  /* [4] Input Channel Control by Auto Function */
#define   TMDS_PWDCTL_ECC                       0x08  /* [3] Enable Clock Channel: Turn on Clock Channel PLL (For Manual Use) */
#define   TMDS_PWDCTL_ERIP                      0x04  /* [2] Enable Red Input Port (For Manual Use, Cut Off 50ohm Internal Resis... */
#define   TMDS_PWDCTL_EGIP                      0x02  /* [1] Enable Green Input Port (For Manual Use, Cut Off 50ohm Internal Res... */
#define   TMDS_PWDCTL_EBIP                      0x01  /* [0] Enable Blue Input Port (For Manual Use, Cut Off 50ohm Internal Resi... */
#define TMDS_ABC0             0x72A9
#define   TMDS_ABC0_WR_AKSV_FLAG                0x80  /* [7] Write AKSV Flag (Write 1 Clear)(in Power on Region) */
#define   TMDS_ABC0_RD_RI_FLAG                  0x40  /* [6] Read Ri Flag (Write 1 Clear) (in Power on Region) */
#define   TMDS_ABC0_RD_BKSV_FLAG                0x20  /* [5] Read BKSV Flag (Write 1 Clear) (in Power on Region) */
#define   TMDS_ABC0_HDCP_KEY_ACCESS_MODE        0x10  /* [4] HDCP Key SRAM Access Right Control Mode */
#define   TMDS_ABC0_HIT_I2C_SLAVE_FLAG          0x04  /* [2] Hit I2C Slave Address(Write 1 Clear)(in Power on Region) */
#define TMDS_ABC1             0x72AA
#define   TMDS_ABC1_HDCP_ENC_EN                 0x20  /* [5] HDCP ENC EN Flag */
#define   TMDS_ABC1_IRQ_AKSV_EN                 0x10  /* [4] IRQ Control For AKSV (Flag Refer to 0xA9) */
#define TMDS_ACC2             0x72AB
#define   TMDS_ACC2_REG_HS_WIDTH_SEL            0x80  /* [7] Hsnc Pulse Width Selection */
#define   TMDS_ACC2_HDCP_VS_SEL                 0x40  /* [6] HDCP Vsnc Selection */
#define TMDS_Z0CC2            0x72AC
#define   TMDS_Z0CC2_DDCDBNC                    0x80  /* [7] HDCP DDC DEBOUNCE */
#define   TMDS_Z0CC2_HDE                        0x40  /* [6] HDMI/DVI Function Enable (HDCP Enable Is Moved to HDCP ) */
#define   TMDS_Z0CC2_DBNC_LEVEL                 0x20  /* [5] HDCP Debounce Level Selection */
#define   TMDS_Z0CC2_KM_CLK_SEL                 0x10  /* [4] Clock Selection for KM Calculation */
#define TMDS_CPS              0x72AD
#define   TMDS_CPS_CLR_INFOFRAME_DVI            0x08  /* [3] Clear Info-frame Data When DVI Mode */
#define   TMDS_CPS_AUTO_DVI2HDMI                0x04  /* [2] In HDMI/DVI Auto Detection Mode, Auto Switch to DVI Mode When No Si... */
#define   TMDS_CPS_GLB_POWDN                    0x02  /* [1] HDMI global module power down enable: */
#define TMDS_RPS              0x72AE
#define   TMDS_RPS_HDCP_KEY_RDY                 0x02  /* [1] HDCP KEY Ready Flag */
#define   TMDS_RPS_AUTHST_MODE_SEL              0x01  /* [0] Selection of AUTHST Mode. */
#define TMDS_WDC              0x72AF
#define   TMDS_WDC_VS_OUT_INV                   0x08  /* [3] Inverse the output Vsync */
#define   TMDS_WDC_HS_OUT_INV                   0x04  /* [2] Inverse the output Hsync */
#define   TMDS_WDC_DVI_ENC_MODE                 0x01  /* [0] Selection of DVI OESS Mode. */
#define TMDS_CHANNEL_FIFO     0x72B0
#define   TMDS_CHANNEL_FIFO_CHANNEL_FIFO_EN     0x80  /* [7] HDMI 3-channel FIFO Function Global Enable */
#define   TMDS_CHANNEL_FIFO_CHANNEL_FIFO_RST    0x40  /* [6] HDMI 3-channel FIFO global reset */
#define TMDS_UDC0             0x72B5
#define   TMDS_UDC0_DPC_BYPASS_DIS              0x80  /* [7] Deep color output selection: */
#define TMDS_UDC1             0x72B6
#define   TMDS_UDC1_NO_CLK_IN                   0x80  /* [7] No Clock Input. */
#define TMDS_UDC2             0x72B7
#define   TMDS_UDC2_NL                          0xE0  /* [7:5] The Selection of Error Correction, ERRC_SEL<2:0> */
#define   TMDS_UDC2_NL_SHIFT                    5
#define TMDS_DPC1             0x72B8
#define   TMDS_DPC1_DPC_CLK_SOURCE              0x08  /* [3] Select The Reference Clock of Deep Color Pll */
#define   TMDS_DPC1_DPC_AUTO                    0x04  /* [2] 0: Manual mode (CD/PP/default_ phase fields are specified by FW) */
#define HDCP_CR               0x72C0
#define   HDCP_CR_AC_MOD                        0x80  /* [7] Advance Cipher Mode */
#define   HDCP_CR_DKAPDE                        0x02  /* [1] Device Key Access Port Download Enable (download Key from MCU) */
#define   HDCP_CR_ENABLE                        0x01  /* [0] HDCP Enable */
#define HDCP_DKAP             0x72C1
#define HDCP_PCR              0x72C2
#define   HDCP_PCR_DKAP_CLK_GATING              0x80  /* [7] Device Key Accessing SRAM Clock gating select: */
#define   HDCP_PCR_HDCP_CLK_GATING              0x40  /* [6] HDCP Module Clock gating select: */
#define   HDCP_PCR_DDCSEL                       0x06  /* [2:1] DDC Channel SEL for Key Access */
#define HDCP_AP               0x72C3
#define HDCP_DP               0x72C4
#define HDMI_APC              0x72C8
#define   HDMI_APC_AAIF                         0x01  /* [0] Address Auto Increase Function */
#define HDMI_AP               0x72C9
#define HDMI_DP               0x72CA
#define HDMI_SR               0x72CB
#define   HDMI_SR_MODE                          0x01  /* [0] HDMI/DVI Mode Detected by Auto Function, Even in Manual Mode, This ... */
#define HDMI_GPVS             0x72CC
#define   HDMI_GPVS_PIS                         0x60  /* [6:5] Packet Input Status */
#define   HDMI_GPVS_PIS_SHIFT                   5
#define HDMI_PSAP             0x72CD
#define HDMI_PSDP             0x72CE
#define AUX_PS_REPLY          0x72E1
#define   AUX_PS_REPLY_FREQDET_CLK_GATING       0x80  /* [7] Freq detect module clock gating select: */
#define HDMI_FREQDET_CTRL     0x72E6
#define   HDMI_FREQDET_CTRL_FREQDET_START       0x80  /* [7] Start The Frequency Detection Process. Once The Bit Has Been Set, T... */
#define   HDMI_FREQDET_CTRL_FREQ_SOURCE         0x30  /* [5:4] Choose Which Clock to Be Measured. */
#define   HDMI_FREQDET_CTRL_FREQ_UNSTABLE       0x04  /* [2] The Freq Status Has Been Unstable. (Write 1 to Clear) */
#define   HDMI_FREQDET_CTRL_IRQ_FREQ_UNSTABLE   0x02  /* [1] Interrupt As Long As The Freq Is Unstable. */
#define HDMI_FREQDET_OFFSET   0x72E7
#define   HDMI_FREQDET_OFFSET_MCLK_DIV          0x40  /* [6] Input Clock to be measure divider */
#define   HDMI_FREQDET_OFFSET_FREQDET_OFFSET    0x07  /* [2:0] Determine The Pre-set Threshold, Within The Frequency Ratio Means T... */
#define HDMI_FREQDET_UPPER_M  0x72E8
#define HDMI_FREQDET_UPPER_L  0x72E9
#define HDMI_FREQDET_LOWER_M  0x72EA
#define HDMI_FREQDET_LOWER_L  0x72EB
#define HDMI_FREQDET_STABLE   0x72EC
#define   HDMI_FREQDET_STABLE_RESULT_POPUP      0x80  /* [7] POP UP The Counter Value in HW */
#define   HDMI_FREQDET_STABLE_STABLE_TIME       0x7F  /* [6:0] Determine How Much Times That Freq Needs to Be Within The Pre-set T... */
#define   HDMI_FREQDET_STABLE_STABLE_TIME_SHIFT 0
#define HDMI_FREQDET_RESULT_M 0x72ED
#define HDMI_FREQDET_RESULT_L 0x72EE
#define HDMI_ERROR_TH         0x72EF
#define   HDMI_ERROR_TH_FREQ_STABLE_IRQ_EN      0x80  /* [7] Interrupt As Long As The Freq Is Stable. */
#define   HDMI_ERROR_TH_FREQ_STABLE             0x40  /* [6] Determine If Clock Frequency Is Stable in Measure Process. (write 1... */
#define HDMI_FREQDET_BOUND_H  0x72F6
#define   HDMI_FREQDET_BOUND_H_FREQDET_COUNT    0x03  /* [1:0] The Frequency Detect Reference Counter */

#endif /* RTD2556_REGS_PAGE_72_H */
