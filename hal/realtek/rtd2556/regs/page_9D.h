/* RTD2556 scaler page 0x9D */
#ifndef RTD2556_REGS_PAGE_9D_H
#define RTD2556_REGS_PAGE_9D_H

/* --- DisplayPort Tx AUX Analog PHY CTRL (Page BD) --- */
#define AUX_1              0x9D61                 /* from BD61 */
#define   AUX_1_REG_AUX_VLDO                0xE0  /* [7:5] AUX TX LDO Voltage Select */
#define   AUX_1_REG_AUX_VLDO_1_05V          0xA0  /* 101 */
#define   AUX_1_REG_AUX_RX_ADJR_P           0x0F  /* [3:0] AUX RX P Channel 50ohm Hand Mode Setting */
#define   AUX_1_REG_AUX_RX_ADJR_P_SHIFT     0
#define AUX_2              0x9D62                 /* from BD62 */
#define   AUX_2_REG_AUX_EN_SINGLEEND        0x20  /* [5] AUX Single-ended Enable */
#define   AUX_2_REG_EN_AUX_RX_BIGZ0_P       0x01  /* [0] AUX RX P Termination Z0 Big Enable */
#define AUX_6              0x9D66                 /* from BD66 */
#define   AUX_6_REG_EN_AUX_RX_BIGZ0_N       0x10  /* [4] AUX RX N Termination Z0 Big Enable */
#define   AUX_6_REG_AUX_RX_ADJR_N           0x0F  /* [3:0] AUX RX N Channel 50ohm Hand Mode Setting */
#define   AUX_6_REG_AUX_RX_ADJR_N_SHIFT     0
#define DIG_TX_03          0x9D67                 /* from BD67 */
#define   DIG_TX_03_REG_AUX_TX_ADJR_P       0xF0  /* [7:4] AUX TX P Channel 50ohm Hand Mode Setting */
#define   DIG_TX_03_REG_AUX_TX_ADJR_P_SHIFT 4
#define   DIG_TX_03_REG_AUX_TX_ADJR_N       0x0F  /* [3:0] AUX TX N Channel 50ohm Hand Mode Setting */
#define   DIG_TX_03_REG_AUX_TX_ADJR_N_SHIFT 0

/* --- DisplayPort Tx AUX MAC CTRL (Page BD) --- */
#define AUX_TX_CTRL        0x9DA0                 /* from BDA0 */
#define   AUX_TX_CTRL_AUX_EN                0x01  /* [0] Aux Channel Enable */
#define AUX_TIMEOUT        0x9DA1                 /* from BDA1 */
#define   AUX_TIMEOUT_AUX_TIMEOUT_EN        0x80  /* [7] Aux Timeout Enable */
#define   AUX_TIMEOUT_AUX_TIMEOUT_NUM       0x7F  /* [6:0] XTAL_period * (TIMEOUT_NUM+1)*512 */
#define   AUX_TIMEOUT_AUX_TIMEOUT_NUM_SHIFT 0
#define AUX_FSM_STATUS     0x9DA2                 /* from BDA2 */
#define   AUX_FSM_STATUS_AUXCH_FSM          0x0F  /* [3:0] Aux Ch Fsm State */
#define AUXTX_TRAN_CTRL    0x9DA3                 /* from BDA3 */
#define   AUXTX_TRAN_CTRL_TX_ADDRONLY       0x80  /* [7] Active in I2C over AUX Transaction. */
#define   AUXTX_TRAN_CTRL_AUX_FW_HANDLE     0x20  /* [5] F/w Handle Mode Enable */
#define   AUXTX_TRAN_CTRL_TX_START          0x01  /* [0] Set 1 to Transmit Aux Data And Then HW Auto Clear When Finished. */
#define AUXTX_REQ_CMD      0x9DA4                 /* from BDA4 */
#define   AUXTX_REQ_CMD_TX_CMD_SHIFT        4
#define   AUXTX_REQ_CMD_TX_ADDR             0x0F  /* [3:0] AUX TX Request Address[19:16] */
#define AUXTX_REQ_ADDR_M   0x9DA5                 /* from BDA5 */
#define AUXTX_REQ_ADDR_L   0x9DA6                 /* from BDA6 */
#define AUXTX_REQ_LEN      0x9DA7                 /* from BDA7 */
#define AUXTX_REQ_DATA     0x9DA8                 /* from BDA8 */
#define AUX_REPLY_CMD      0x9DA9                 /* from BDA9 */
#define   AUX_REPLY_CMD_AUX_RX_CMD          0xF0  /* [7:4] Read AUX RX CMD from FIFO */
#define   AUX_REPLY_CMD_AUX_RX_CMD_SHIFT    4
#define AUX_REPLY_DATA     0x9DAA                 /* from BDAA */
#define AUX_FIFO_CTRL      0x9DAB                 /* from BDAB */
#define   AUX_FIFO_CTRL_I2C_FIFO_RST        0x02  /* [1] 1: Reset I2C AUX FIFO (write 1 to clear) */
#define   AUX_FIFO_CTRL_NA_FIFO_RST         0x01  /* [0] 1: Reset Native AUX FIFO (write 1 to clear) */
#define AUX_TX_FIFO_STATUS 0x9DAC                 /* from BDAC */
#define AUX_FIFO_WR_PTR    0x9DAE                 /* from BDAE */
#define   AUX_FIFO_WR_PTR_WR_PTR            0x1F  /* [4:0] AUX FIFO Write Pointer. */
#define AUX_RETRY_2        0x9DB0                 /* from BDB0 */
#define   AUX_RETRY_2_RETRY_EN              0x80  /* [7] When Receiving I2c Defer Or Aux Defer Or Time Out, Retry by Sending... */
#define   AUX_RETRY_2_RETRY_DEFER_EN        0x40  /* [6] Active When RETRY_EN = 1 */
#define   AUX_RETRY_2_RETRY_TIMEOUT_EN      0x20  /* [5] Active When RETRY_EN = 1 */
#define   AUX_RETRY_2_RETRY_NACK_EN         0x10  /* [4] Active When RETRY_EN = 1 */
#define   AUX_RETRY_2_RETRY_ERROR_EN        0x08  /* [3] Active When RETRY_EN = 1 */
#define AUX_IRQ_EVENT      0x9DB1                 /* from BDB1 */
#define   AUX_IRQ_EVENT_AUXDONE_IRQ         0x20  /* [5] Aux Transaction Done Flag. */
#define   AUX_IRQ_EVENT_RXERROR_IRQ         0x10  /* [4] Rx Error Interrupt Flag */
#define   AUX_IRQ_EVENT_READFAIL_IRQ        0x08  /* [3] When Native AUX Read , Rx Just Return ACK Without Enough Data. */
#define   AUX_IRQ_EVENT_NACK_IRQ            0x04  /* [2] When Receiving The AUX Or I2C NACK, Assert This IRQ. */
#define   AUX_IRQ_EVENT_RETRY_IRQ           0x02  /* [1] Retry Times Reaches RETRY_NUM, Assert This IRQ */
#define   AUX_IRQ_EVENT_TIMEOUT_IRQ         0x01  /* [0] 400us Receiving Timeout IRQ */

/* --- DisplayPort Tx AUX MAC CTRL (Page BD) --- */
#define AUX_DIG_PHY7       0x9DF5                 /* from BDF5 */
#define   AUX_DIG_PHY7_AUX_PHY_INT          0x40  /* [6] When Aux Phy Found Aux Signal Toggle =>interrupt */
#define AUX_DIG_PHY8       0x9DF6                 /* from BDF6 */
#define   AUX_DIG_PHY8_EHANDLE_SYNCEND_DIS  0x02  /* [1] Sync_end Pattern Error Handle Disable */
#define AUX_DIG_PHY9       0x9DF7                 /* from BDF7 */
#define   AUX_DIG_PHY9_MAN_ERROR            0x08  /* [3] Manchester Error (write 1 to Clear) */
#define   AUX_DIG_PHY9_STOP_ERROR           0x04  /* [2] Stop Pattern Error(write 1 to Clear) */
#define   AUX_DIG_PHY9_SYNCEND_ERROR        0x02  /* [1] Sync End Pattern Error(write 1 to Clear) */
#define   AUX_DIG_PHY9_DATA_8X_ERROR        0x01  /* [0] Data Not 8x Error(write 1 to Clear) */
#define AUX_DEBUG          0x9DF8                 /* from BDF8 */
#define   AUX_DEBUG_FSM_STATE               0x07  /* [2:0] AUX Tx FSM State */

#endif /* RTD2556_REGS_PAGE_9D_H */
