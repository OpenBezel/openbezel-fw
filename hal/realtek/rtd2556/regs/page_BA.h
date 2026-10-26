/* RTD2556 scaler page 0xBA */
#ifndef RTD2556_REGS_PAGE_BA_H
#define RTD2556_REGS_PAGE_BA_H

/* --- D1 Port DisplayPort 1.2 AUX Analog PHY (Page BA) --- */
#define D1_DIG_TX_04          0xBA60
#define   D1_DIG_TX_04_REG_AUX_SEWINS                    0x03  /* [1:0] AUX RC Single-ended Comparator Window Select */
#define   D1_DIG_TX_04_REG_AUX_SEWINS_50MV               0x01  /* 01 */
#define D1_AUX_1              0xBA61
#define   D1_AUX_1_REG_AUX_VLDO_1_05V                    0xA0  /* 101 */
#define   D1_AUX_1_REG_AUX_SELVCOM                       0x10  /* [4] AUX RX Common Mode Voltage Generation */
#define   D1_AUX_1_REG_AUX_RX_ADJR_P                     0x0F  /* [3:0] AUX RX P Channel 50ohm Hand Mode Setting */
#define   D1_AUX_1_REG_AUX_RX_ADJR_P_SHIFT               0
#define D1_AUX_2              0xBA62
#define   D1_AUX_2_REG_AUX_SEL_THRESHOLD_50MV            0x08  /* 01 */
#define   D1_AUX_2_REG_EN_AUX_PSM                        0x02  /* [1] AUX Power Saving Mode Enable */
#define   D1_AUX_2_REG_EN_AUX_RX_BIGZ0_P                 0x01  /* [0] AUX RX P Termination Z0 Big Enable */
#define D1_AUX_3              0xBA63
#define   D1_AUX_3_REG_EN_AUX_PSM_BIAS                   0x80  /* [7] AUX Power Saving Mode Current from POR Enable */
#define D1_AUX_4              0xBA64
#define   D1_AUX_4_REG_AUX_REV                           0xC0  /* [7:6] AUX Reserved Register */
#define   D1_AUX_4_REG_AUX_REV_400UA                     0xC0  /* 11 */
#define D1_AUX_5              0xBA65
#define   D1_AUX_5_REG_AUX_RSTB                          0x80  /* [7] AUX Re-set Enable */
#define D1_AUX_6              0xBA66
#define   D1_AUX_6_REG_EN_AUX_RX_BIGZ0_N                 0x10  /* [4] AUX RX N Termination Z0 Big Enable */
#define   D1_AUX_6_REG_AUX_RX_ADJR_N                     0x0F  /* [3:0] AUX RX N Channel 50ohm Hand Mode Setting */
#define   D1_AUX_6_REG_AUX_RX_ADJR_N_SHIFT               0
#define D1_DIG_TX_03          0xBA67
#define   D1_DIG_TX_03_REG_AUX_TX_ADJR_P                 0xF0  /* [7:4] AUX TX P Channel 50ohm Hand Mode Setting */
#define   D1_DIG_TX_03_REG_AUX_TX_ADJR_P_SHIFT           4
#define   D1_DIG_TX_03_REG_AUX_TX_ADJR_N                 0x0F  /* [3:0] AUX TX N Channel 50ohm Hand Mode Setting */
#define   D1_DIG_TX_03_REG_AUX_TX_ADJR_N_SHIFT           0

/* --- D1 Port DisplayPort 1.2 AUX Digital PHY (Page BA) --- */
#define D1_AUX_DIG_PHY2       0xBA72
#define   D1_AUX_DIG_PHY2_AVE_POS                        0xF8  /* [7:3] Average Start Position (average 8 Cycles) */
#define   D1_AUX_DIG_PHY2_AVE_POS_SHIFT                  3
#define D1_AUX_DIG_PHY3       0xBA73
#define   D1_AUX_DIG_PHY3_AUX_CLKSEL_AUTO_EN             0x80  /* [7] Used Rx Counter Value to Set The AUX CLK  Select */
#define   D1_AUX_DIG_PHY3_AUX_NEW_MODE                   0x40  /* [6] Modify The Sync Pattern */
#define   D1_AUX_DIG_PHY3_AUX_PHY_REF_CLK_DIV            0x0F  /* [3:0] Divider of  Aux Phy Reference clk */
#define   D1_AUX_DIG_PHY3_AUX_PHY_REF_CLK_DIV_SHIFT      0
#define D1_AUX_DIG_PHY4       0xBA74
#define   D1_AUX_DIG_PHY4_PCHARGE_NUM                    0x1F  /* [4:0] When AUX_RX in Precharge State, It Will Go to Sync0 State When Rece... */
#define   D1_AUX_DIG_PHY4_PCHARGE_NUM_SHIFT              0
#define D1_AUX_DIG_PHY5       0xBA75
#define   D1_AUX_DIG_PHY5_AVE_MAN_EN                     0x80  /* [7] 1.manual Mode */
#define   D1_AUX_DIG_PHY5_AVE_NUM                        0x7F  /* [6:0] Average Number */
#define   D1_AUX_DIG_PHY5_AVE_NUM_SHIFT                  0
#define D1_AUX_DIG_PHY6       0xBA76
#define   D1_AUX_DIG_PHY6_AUX_SDM_EN                     0x80  /* [7] Apply AUX SDM to Avoid Clk Drift When Sync_end */
#define   D1_AUX_DIG_PHY6_SYNC_CNT_MODE                  0x40  /* [6] 1: Rounding */
#define D1_AUX_DIG_PHY7       0xBA77
#define   D1_AUX_DIG_PHY7_AUX_PHY_INT_EN                 0x80  /* [7] AUX PHY INTERRUPT ENABLE */
#define   D1_AUX_DIG_PHY7_AUX_PHY_INT                    0x40  /* [6] When Aux Phy Found Aux Signal Toggle =>interrupt */
#define D1_AUX_DIG_PHY8       0xBA78
#define   D1_AUX_DIG_PHY8_AUX_PHY_TEST_SEL               0x80  /* [7] Reserved */
#define   D1_AUX_DIG_PHY8_EHANDLE_EN                     0x20  /* [5] Enable : */
#define   D1_AUX_DIG_PHY8_SYNC1_WO_IDLE                  0x10  /* [4] Sync1 State Keep When Encounter IDLE */
#define D1_AUX_DIG_PHY9       0xBA79
#define   D1_AUX_DIG_PHY9_SINGLE_END_MODE                0x80  /* [7] 1: Receive single end data (collocate analog) */
#define   D1_AUX_DIG_PHY9_MAN_ERROR                      0x08  /* [3] Manchester Error (write 1 to Clear) */
#define   D1_AUX_DIG_PHY9_STOP_ERROR                     0x04  /* [2] Stop Pattern Error(write 1 to Clear) */
#define   D1_AUX_DIG_PHY9_SYNCEND_ERROR                  0x02  /* [1] Sync End Pattern Error(write 1 to Clear) */
#define   D1_AUX_DIG_PHY9_DATA_8X_ERROR                  0x01  /* [0] Data Not 8x Error(write 1 to Clear) */
#define D1_AUX_DIG_PHYA       0xBA7A
#define   D1_AUX_DIG_PHYA_AUX_RXST_INT_EN                0x10  /* [4] Normal Aux Rx State Interrupt Enable */
#define   D1_AUX_DIG_PHYA_AUX_RXST_INT                   0x01  /* [0] Normal Aux Rx State Interrupt Flag */

/* --- D1 Port DisplayPort 1.2 AUX (Page BA) --- */
#define D1_AUX_RESERVE5       0xBAA5
#define   D1_AUX_RESERVE5_WR_AKSV_IRQ_EN                 0x40  /* [6] Enable IRQ when DP source write complete Aksv value in DPCD 0x6800B */
#define   D1_AUX_RESERVE5_WR_AKSV_FLAG                   0x04  /* [2] Set to one when DP source write complete Aksv value in DPCD 0x6800B */
#define   D1_AUX_RESERVE5_WR_TYPE_FLAG                   0x02  /* [1] Set to one when write type register in DPCD 0x69494 */
#define   D1_AUX_RESERVE5_WR_SKE_SEND_EKS_FLAG           0x01  /* [0] Set to one when write SKE_send_eks register */
#define D1_AUX_PAYLOAD_CLEAR  0xBAB0
#define   D1_AUX_PAYLOAD_CLEAR_AUX_MAC_REF_CLK_DIV       0x0F  /* [3:0] Divider of  Aux Mac Reference clk */
#define   D1_AUX_PAYLOAD_CLEAR_AUX_MAC_REF_CLK_DIV_SHIFT 0
#define D1_AUX_IRQ_ADDR0_MSB  0xBAB4
#define   D1_AUX_IRQ_ADDR0_MSB_DPCD_ADDR0_WILDCARD       0xF0  /* [7:4] Bit[4] indicate whether dpcd_addr0[3:0] be discard */
#define   D1_AUX_IRQ_ADDR0_MSB_DPCD_ADDR0_WILDCARD_SHIFT 4
#define D1_AUX_IRQ_ADDR0_MSB1 0xBAB5
#define D1_AUX_IRQ_ADDR0_LSB  0xBAB6
#define D1_AUX_IRQ_ADDR3_MSB  0xBABD
#define   D1_AUX_IRQ_ADDR3_MSB_DPCD_ADDR3_WILDCARD       0xF0  /* [7:4] Bit[4] indicate whether dpcd_addr3[3:0] be discard */
#define   D1_AUX_IRQ_ADDR3_MSB_DPCD_ADDR3_WILDCARD_SHIFT 4
#define D1_AUX_IRQ_ADDR3_MSB1 0xBABE
#define D1_AUX_IRQ_ADDR3_LSB  0xBABF
#define D1_DPCD_CTRL          0xBAC0
#define   D1_DPCD_CTRL_AUX_RESET                         0x80  /* [7] 0: Normal run. */
#define   D1_DPCD_CTRL_RX_MSG_RESET                      0x40  /* [6] 0: Normal run. */
#define   D1_DPCD_CTRL_DPCD_RESET                        0x20  /* [5] 0: Normal run */
#define   D1_DPCD_CTRL_AUX_DATA_FIFO_SEL                 0x02  /* [1] 0: Native AUX Fifo */
#define   D1_DPCD_CTRL_DPCD_ACCESS_PORT_AUTO_INC         0x01  /* [0] 1: Auto increase */
#define D1_DPCD_ADDR_PORT_H   0xBAC1
#define D1_DPCD_ADDR_PORT_M   0xBAC2
#define D1_DPCD_ADDR_PORT_L   0xBAC3
#define D1_DPCD_DATA_PORT     0xBAC4
#define D1_AUX_PHY_DIG2       0xBAC6
#define   D1_AUX_PHY_DIG2_FAKE_LT_EN                     0x10  /* [4] Fake Link Training Enable */
#define D1_AUX_MODE_SET       0xBAD0
#define   D1_AUX_MODE_SET_DP_AUX_FORCE_DEFER_EN          0x10  /* [4] If aux command timeout HW Force Reply Defer */
#define   D1_AUX_MODE_SET_ERROR_RST_FSM_EN               0x08  /* [3] 1: If rx_error, reset AUX FSM */
#define   D1_AUX_MODE_SET_DP_LINK_TRANS_EN               0x04  /* [2] Enable Automatically Link Training Process. */
#define   D1_AUX_MODE_SET_DP_AUX_AUTO_EN                 0x02  /* [1] Native AUX-CH Handle Selection: */
#define   D1_AUX_MODE_SET_DP_AUX_EN                      0x01  /* [0] Enable DisplayPort AUX Channel */
#define D1_DP_IIC_SET         0xBAD1
#define   D1_DP_IIC_SET_DP_IICRD_NEW                     0x02  /* [1] 0: Treat 2nd repeated I2C command with MOT = 0 as 'status update' w... */
#define D1_AUX_RX_CMD         0xBAD2
#define D1_AUX_RX_ADDR_M      0xBAD3
#define D1_AUX_RX_ADDR_L      0xBAD4
#define D1_AUX_FIFO_RST       0xBADA
#define   D1_AUX_FIFO_RST_IIC_SLAVE_ADD_INT              0x10  /* [4] IIC Slave Address int */
#define   D1_AUX_FIFO_RST_SCL_RATE_CHG                   0x08  /* [3] Due to The Aux Mac Domain Clk Change */
#define   D1_AUX_FIFO_RST_WRHDCP_INT_EN                  0x04  /* [2] DP Source Write DPCD 68xxx Interrupt Enable */
#define   D1_AUX_FIFO_RST_WRHDCP_INT                     0x02  /* [1] DP Source Has Written DPCD 68xxx(for HDCP) Address */
#define   D1_AUX_FIFO_RST_AUX_FIFO_RST                   0x01  /* [0] Write '1' to Clear AUX Data FIFO Pointer. */
#define D1_AUX_STATUS         0xBADB
#define   D1_AUX_STATUS_AUXNACKM                         0x20  /* [5] When Write Read Only DPCD */
#define   D1_AUX_STATUS_AUXNOREPLY                       0x10  /* [4] When Aux Error, */
#define D1_AUX_IRQ_STATUS     0xBADC
#define   D1_AUX_IRQ_STATUS_WROT_INT_EN                  0x20  /* [5] Write Other Address (reference to DD) Interrupt Enable */
#define   D1_AUX_IRQ_STATUS_WROT_INT                     0x10  /* [4] Write Other Address (reference to DD) Interrupt */
#define   D1_AUX_IRQ_STATUS_TIMEOUT_EN                   0x08  /* [3] Enable Aux-CH Timeout As An Interrupt Source */
#define   D1_AUX_IRQ_STATUS_AUX_RX_EN                    0x04  /* [2] Enable Receiving AUX-CH As An Interrupt Source */
#define   D1_AUX_IRQ_STATUS_TIMEOUT_INT                  0x02  /* [1] Timeout And Auto Reply. Write 1 to Clear. */
#define   D1_AUX_IRQ_STATUS_AUX_RX_INT                   0x01  /* [0] Receive AUX Channel Command, Clear When Writing '1'. */
#define D1_AUX_DPCD_IRQ       0xBADD
#define   D1_AUX_DPCD_IRQ_WR102_INT                      0x80  /* [7] DP Source Has Written DPCD 00102 Address */
#define   D1_AUX_DPCD_IRQ_WR100_101_INT                  0x40  /* [6] DP Source Has Written DPCD 00100 And 00101 Address */
#define   D1_AUX_DPCD_IRQ_WR103_108_INT                  0x20  /* [5] DP Source Has Written DPCD 00103 to 00108 Address */
#define   D1_AUX_DPCD_IRQ_WR600_INT                      0x01  /* [0] DP Source Has Written DPCD 00103 to 00600 Address for Power Saving ... */
#define D1_AUX_DPCD_IRQ_EN    0xBADE
#define   D1_AUX_DPCD_IRQ_EN_WR102_INT_EN                0x80  /* [7] WR102_INT Enable */
#define   D1_AUX_DPCD_IRQ_EN_WR100_101_INT_EN            0x40  /* [6] WR100_101_INT Enable */
#define   D1_AUX_DPCD_IRQ_EN_WR103_108_INT_EN            0x20  /* [5] WR103_108_INT Enable */
#define   D1_AUX_DPCD_IRQ_EN_WR270_INT_EN                0x04  /* [2] WR270_INT Enable */
#define   D1_AUX_DPCD_IRQ_EN_WR600_INT_EN                0x01  /* [0] WR600_INT Enable */
#define D1_AUX_TX_TIMER       0xBAF0
#define   D1_AUX_TX_TIMER_AUX_TIMER_EN                   0x20  /* [5] 1:Use aux_tx_timer to delay the ACK Timer */
#define   D1_AUX_TX_TIMER_IIC_ACK_TIMER_EN               0x04  /* [2] 1:Use aux_tx_timer to delay the IIC ACK Timer */
#define D1_AUX_TX_TIMER_2     0xBAF1
#define   D1_AUX_TX_TIMER_2_TIMEOUT_TARGET               0x3F  /* [5:0] Aux Over I2C Timeout Target Value Settings */
#define   D1_AUX_TX_TIMER_2_TIMEOUT_TARGET_SHIFT         0
#define D1_MCUIIC             0xBAF4
#define   D1_MCUIIC_IIC_DEFER_CASE1                      0x40  /* [6] 0 : no action */
#define   D1_MCUIIC_IIC_DEFER_CASE2                      0x20  /* [5] 0 : no action */
#define D1_PBA_DUMMY_1        0xBAFB
#define   D1_PBA_DUMMY_1_DPCD_ADDR0_IRQ                  0x80  /* [7] Aux wr/rd dpcd addr0 flag */
#define   D1_PBA_DUMMY_1_DPCD_ADDR0_IRQ_EN               0x40  /* [6] Aux wr/rd dpcd addr0 flag irq enable */
#define   D1_PBA_DUMMY_1_DPCD_ADDR1_IRQ                  0x20  /* [5] Aux wr/rd dpcd addr1 flag */
#define   D1_PBA_DUMMY_1_DPCD_ADDR2_IRQ                  0x08  /* [3] Aux wr/rd dpcd addr2 flag */
#define   D1_PBA_DUMMY_1_DPCD_ADDR3_IRQ                  0x02  /* [1] Aux wr/rd dpcd addr3 flag */

#endif /* RTD2556_REGS_PAGE_BA_H */
