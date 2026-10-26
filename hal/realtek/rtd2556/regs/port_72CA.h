/* RTD2556 keyhole: HDMI (index 72C9, data 72CA) */
#ifndef RTD2556_REGS_PORT_72CA_H
#define RTD2556_REGS_PORT_72CA_H

#define HDMI_SCR               0x00
#define   HDMI_SCR_PIS2                                0x80  /* [7] Packet Input Status */
#define   HDMI_SCR_PIS3                                0x40  /* [6] Packet Input Status */
#define   HDMI_SCR_DVI_ACLK_MODE                       0x20  /* [5] When Switch to DVI Mode, Select Audio Clock Control Mode */
#define   HDMI_SCR_REG_PACKET_IGNORE                   0x10  /* [4] HDMI/DVI Mode Detection Method Selection */
#define   HDMI_SCR_MODE                                0x08  /* [3] HDMI/DVI Switch Mode */
#define   HDMI_SCR_MSMODE                              0x04  /* [2] When Manual Mode, Select HDMI Or DVI */
#define   HDMI_SCR_CABS                                0x02  /* [1] DVI/HDMI Condition A, B Select */
#define HDMI_BCHCR             0x02
#define   HDMI_BCHCR_BCH_FLAG_CLR_DVI                  0x40  /* [6] In DVI Mode, Clear BCH Error Flag (0xCA-02[2]) */
#define   HDMI_BCHCR_BCHE                              0x08  /* [3] BCH Function Enable */
#define   HDMI_BCHCR_BCHES                             0x04  /* [2] BCH Function's Result, One Bit Error. It Is Set by This Case, And C... */
#define   HDMI_BCHCR_BCHES2                            0x02  /* [1] BCH Function's Result, Two Bits Error. It Is Set by This Case, And ... */
#define HDMI_AFCR              0x03
#define   HDMI_AFCR_AUDIO_TEST_ENABLE                  0x10  /* [4] 0:Disable */
#define HDMI_MAG_M_FINAL       0x07
#define   HDMI_MAG_M_FINAL_AUTO_DLY_MOD                0x08  /* [3] Automatically Modify The "Auto Gain Delay" */
#define HDMI_AUDIO_LD_P_TIME_M 0x0A
#define   HDMI_AUDIO_LD_P_TIME_M_LDP_TIME              0x03  /* [1:0] Wait-time for HW Getting Stable. (Base Clock Is 5kHz), During This ... */
#define HDMI_AUDIO_LD_P_TIME_N 0x0B
#define HDMI_ZCD_TIMEOUT       0x0D
#define   HDMI_ZCD_TIMEOUT_ZCD_TIMEOUT                 0x7F  /* [6:0] Force to Apply Final Gain When Time-out. */
#define HDMI_CMCR              0x10
#define   HDMI_CMCR_DBDCB                              0x10  /* [4] Double Buffer Download Control Bit */
#define   HDMI_CMCR_REG_DPLL_N                         0x0F  /* [3:0] K Code of Audio PLL, The Value Set Here Adding 1 Is Real Div Value */
#define HDMI_MCAPR             0x11
#define HDMI_SCAPR             0x12
#define   HDMI_SCAPR_SLC                               0x80  /* [7] S1 Code */
#define   HDMI_SCAPR_SC                                0x7F  /* [6:0] S/2 Code */
#define   HDMI_SCAPR_SC_SHIFT                          0
#define HDMI_PSCR              0x15
#define   HDMI_PSCR_FDINT                              0xE0  /* [7:5] When Max. FIFO Depth Increase for N Times Or Min. FIFO Depth Decrea... */
#define   HDMI_PSCR_FDINT_N_8                          0xE0  /* 111 */
#define   HDMI_PSCR_ETFD                               0x08  /* [3] Enable Tracking of The Trend of FIFO Depth */
#define   HDMI_PSCR_ETFBC                              0x04  /* [2] Enable Tracking of FIFO Boundary Condition */
#define HDMI_FBR               0x1B
#define   HDMI_FBR_BAD                                 0x07  /* [2:0] Boundary Address Distance for Triggering Audio PLL Tracking Where B... */
#define HDMI_ICTPSR0           0x20
#define HDMI_ICTPSR1           0x21
#define HDMI_PCTPSR0           0x22
#define HDMI_PCTPSR1           0x23
#define HDMI_ICBPSR0           0x24
#define HDMI_ICBPSR1           0x25
#define HDMI_PCBPSR0           0x26
#define HDMI_PCBPSR1           0x27
#define HDMI_NTX1024TR0        0x28
#define   HDMI_NTX1024TR0_RMCLK_DIV                    0x40  /* [6] Input Clock to be measure divider */
#define HDMI_AAPNR             0x2D
#define   HDMI_AAPNR_ESDM                              0x02  /* [1] Enable SDM(phase Swallow) */
#define HDMI_AVMCR             0x30
#define   HDMI_AVMCR_AUDIO_OUTPUT_EN                   0x40  /* [6] Audio Output Enable/disable Control */
#define   HDMI_AVMCR_AOMC                              0x20  /* [5] Audio Output Mute Control */
#define   HDMI_AVMCR_VE                                0x08  /* [3] Video Clock Output Enable. When Video Watch Dog Occurs, This Bit Wi... */
#define HDMI_WDCR0             0x31
#define   HDMI_WDCR0_ASMFE                             0x80  /* [7] Auto SET_ AVMUTE Function Enable */
#define   HDMI_WDCR0_AUDIO_WD_BY_VIC                   0x20  /* [5] In Audio FIFO Tracking Process(trend And Boundary Tracking), When P... */
#define   HDMI_WDCR0_AWDFO                             0x04  /* [2] Audio Watch Dog Function for Audio FIFO Overflow for "X" Sample. */
#define   HDMI_WDCR0_AWDFU                             0x02  /* [1] Audio Watch Dog Function for Audio FIFO Underflow for "Y" Sample. */
#define HDMI_WDCR1             0x32
#define   HDMI_WDCR1_VWDMOD                            0x20  /* [5] Video Watch Dog Mode */
#define   HDMI_WDCR1_XV                                0x0F  /* [3:0] X Value */
#define   HDMI_WDCR1_XV_SHIFT                          0
#define HDMI_WDCR2             0x33
#define   HDMI_WDCR2_YV                                0x0F  /* [3:0] Y Value */
#define   HDMI_WDCR2_YV_SHIFT                          0
#define HDMI_DPCR0             0x38
#define   HDMI_DPCR0_REG_DPLL_PWDN                     0x80  /* [7] DPLLPWDN */
#define   HDMI_DPCR0_REG_DPLL_O                        0x30  /* [5:4] DPLLO */
#define   HDMI_DPCR0_REG_DPLL_CALSW                    0x04  /* [2] Calibration Validated (go High after Power on 1200us) */
#define   HDMI_DPCR0_REG_DPLL_CALLCH                   0x02  /* [1] Latch Calibration */
#define   HDMI_DPCR0_REG_DPLL_CMPEN                    0x01  /* [0] CMP Enable */
#define HDMI_DPCR2             0x3A
#define   HDMI_DPCR2_REG_DPLL_WDO                      0x80  /* [7] DPLLSTATUS(DPLL WD Status) */
#define   HDMI_DPCR2_REG_DPLL_VCOMD                    0x18  /* [4:3] DPLL VCO Default Mode */
#define   HDMI_DPCR2_REG_DPLL_VCOMD_SHIFT              3
#define   HDMI_DPCR2_REG_DPLL_RESERVE                  0x04  /* [2] DPLLRESERVE, Phase Swallow Circuit Clock Select */
#define HDMI_DPCR3             0x3B
#define   HDMI_DPCR3_REG_DPLL_VCORSTB                  0x01  /* [0] DPLL VCO Reset */
#define HDMI_VWDSR             0x41
#define   HDMI_VWDSR_VWDPVSB                           0x1F  /* [4:0] Video Watch Dog for Packet Variation Status Bit */
#define   HDMI_VWDSR_VWDPVSB_SHIFT                     0
#define HDMI_PTRSV0            0x43
#define HDMI_PTRSV1            0x44
#define HDMI_PVGCR0            0x45
#define HDMI_PVGCR1            0x46
#define   HDMI_PVGCR1_PVSEF8                           0x01  /* [0] Packet Variation Status Enable: Refer to Packet Table, This Byte, C... */
#define HDMI_PVGCR2            0x47
#define   HDMI_PVGCR2_PVSEF22                          0x08  /* [3] Packet Variation Status Enable: Refer to Packet Table, This Byte, C... */
#define   HDMI_PVGCR2_PVSEF21                          0x04  /* [2] Packet Variation Status Enable: Refer to Packet Table, This Byte, C... */
#define   HDMI_PVGCR2_PVSEF20                          0x02  /* [1] Packet Variation Status Enable: Refer to Packet Table, This Byte, C... */
#define   HDMI_PVGCR2_PVSEF19                          0x01  /* [0] Packet Variation Status Enable: Refer to Packet Table, This Byte, C... */
#define HDMI_VCR               0x50
#define   HDMI_VCR_DSC                                 0x0F  /* [3:0] Down Sample Control (only Valid If Video Down Sampling Auto Mode Di... */
#define HDMI_ACRCR             0x51
#define   HDMI_ACRCR_CSAM                              0x40  /* [6] Color Space Translation */
#define   HDMI_ACRCR_CSC                               0x38  /* [5:3] Color Space Control (if CSAM=1, CSC Will Be Read-only) */
#define   HDMI_ACRCR_PRDSAM                            0x04  /* [2] Pixel Repetition Down Sampling Auto Mode */
#define HDMI_INTCR             0x60
#define   HDMI_INTCR_PENDING                           0x80  /* [7] When IRQ Occurred, This Bit Would Be Assigned to 1 by HW, And IRQ W... */
#define HDMI_AOCR              0x62
#define HDMI_DPC_SET0          0x80
#define   HDMI_DPC_SET0_DPC_EN                         0x80  /* [7] Deep Color Mode */
#define HDMI_AUDIO_FREQDET     0x8A
#define   HDMI_AUDIO_FREQDET_AUDIO_FREQ_DETECT         0x80  /* [7] Enable The Detection of The Audio Sampling Rate. */
#define   HDMI_AUDIO_FREQDET_AUTO_LOAD_SCODE           0x40  /* [6] Auto Load S-code & D-code */
#define HDMI_AUDIO_FREQ_RESULT 0x8B
#define   HDMI_AUDIO_FREQ_RESULT_AUD_441_48_SAME_EN    0x40  /* [6] Treat 44.1kHz And 48kHz As The Same Audio Frequency Range in 48Khz */
#define   HDMI_AUDIO_FREQ_RESULT_AUD_882_96_SAME_EN    0x20  /* [5] Treat 88.2kHz And 96kHz As The Same Audio Frequency Range in 96Khz */
#define   HDMI_AUDIO_FREQ_RESULT_AUD_176_192_SAME_EN   0x10  /* [4] Treat 176.4kHz And 192kHz As The Same Audio Frequency Range in 192Khz */
#define   HDMI_AUDIO_FREQ_RESULT_AFREQ_MEAS_RANGE      0x0F  /* [3:0] Automatic Detection of Audio Sampling Rate. */
#define   HDMI_AUDIO_FREQ_RESULT_M                     0x8C
#define   HDMI_AUDIO_FREQ_RESULT_M_AUD_FREQ_TIME       0x60  /* [6:5] Frequency Monitor Time */
#define   HDMI_AUDIO_FREQ_RESULT_M_AUD_FREQ_TIME_SHIFT 5
#define HDMI_TH1               0x90
#define HDMI_TH2               0x91
#define HDMI_TH3               0x92
#define HDMI_TH4               0x93
#define HDMI_TH5               0x94
#define HDMI_PRESET_S_CODE0    0x95
#define HDMI_PRESET_S_CODE1    0x96
#define HDMI_PRESET_S_CODE2    0x97
#define HDMI_PRESET_D_CODE_0   0x99
#define HDMI_PRESET_D_CODE_1   0x9A
#define HDMI_PRESET_D_CODE_2   0x9B
#define HDMI_PRESET_D_CODE_3   0x9C
#define HDMI_PRESET_D_CODE_4   0x9D
#define HDMI_PRESET_D_CODE_5   0x9E
#define HDMI_PRESET_AFSM_MOD   0x9F
#define   HDMI_PRESET_AFSM_MOD_DP_ABUF_WR_MOD_EN       0x80  /* [7] Before HW Confirms There Has Been Any Audio-input, HW Stops The Dp-... */
#define   HDMI_PRESET_AFSM_MOD_AUTO_STOP_TRK_EN        0x40  /* [6] HW Automatically Pause The FIFO Tracking Process Right after No Aud... */
#define   HDMI_PRESET_AFSM_MOD_TRK_MOD_EN              0x20  /* [5] FIFO Tracking (trend & Boundary) Is Activated Only When Audio FSM I... */
#define   HDMI_PRESET_AFSM_MOD_AFSM_MOD_EN             0x10  /* [4] As Long As No Audio Has Been Detected, HW Automatically Turn Down T... */
#define   HDMI_PRESET_AFSM_MOD_DBUF_FREQ_CHANGE_SEL    0x04  /* [2] Double Buffer Freq Change Select: */
#define HDMI_PVSR_4            0xA9
#define   HDMI_PVSR_4_CHG_GMT_CGSN                     0x80  /* [7] Packet Variation Status: Refer to Packet Table, This Byte, Correspo... */
#define   HDMI_PVSR_4_CHG_GMT_PSC                      0x40  /* [6] Packet Variation Status: Refer to Packet Table, This Byte, Correspo... */
#define   HDMI_PVSR_4_CHG_GMT_NOCMT                    0x20  /* [5] Packet Variation Status: Refer to Packet Table, This Byte, Correspo... */
#define HDMI_PVGCR3            0xAA
#define HDMI_PVGCR4            0xAB
#define   HDMI_PVGCR4_CHG_GMT_CGSN_EN                  0x80  /* [7] Packet Variation Status Enable: Refer to Packet Table, This Byte, C... */
#define   HDMI_PVGCR4_CHG_GMT_PSC_EN                   0x40  /* [6] Packet Variation Status Enable: Refer to Packet Table, This Byte, C... */
#define   HDMI_PVGCR4_CHG_GMT_NOCMT_EN                 0x20  /* [5] Packet Variation Status Enable: Refer to Packet Table, This Byte, C... */

#endif /* RTD2556_REGS_PORT_72CA_H */
