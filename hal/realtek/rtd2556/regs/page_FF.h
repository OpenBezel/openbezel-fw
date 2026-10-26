/* RTD2556 scaler page 0xFF */
#ifndef RTD2556_REGS_PAGE_FF_H
#define RTD2556_REGS_PAGE_FF_H

/* --- EMCU LSADC (Page FF) --- */
#define ADC_ACONTROL                 0xFF08
#define   ADC_ACONTROL_STRT_ADC_ACKT                 0x80  /* [7] Write 1 to Start The A/D Conversion. Auto Clear When A/D Conversion... */
#define ADC_A0_CONVERT_RESULT        0xFF09
#define ADC_A1_CONVERT_RESULT        0xFF0A
#define ADC_CLK_DIV                  0xFF0D
#define   ADC_CLK_DIV_ADC_CLK_DIV                    0x7F  /* [6:0] ADC Clk Divider */
#define   ADC_CLK_DIV_ADC_CLK_DIV_SHIFT              0

/* --- EMCU DDC Part 1 (Page FF) --- */
#define ADC_DDC_ENABLE               0xFF1B
#define ADC_DDC_CONTROL_1            0xFF1C
#define   ADC_DDC_CONTROL_1_A_DBN_CLK_SEL            0xC0  /* [7:6] De-bounce Clock Divider */
#define   ADC_DDC_CONTROL_1_A_DBN_CLK_SEL_SHIFT      6
#define   ADC_DDC_CONTROL_1_A_STOP_DBN_SEL           0x30  /* [5:4] De-bounce Sda Stage */
#define   ADC_DDC_CONTROL_1_A_STOP_DBN_SEL_SHIFT     4
#define ADC_DDC_CONTROL_2            0xFF1D
#define DDC0_DDC_ENABLE              0xFF1E
#define   DDC0_DDC_ENABLE_DDC0_DDC_ADDR              0xE0  /* [7:5] DDC0 Channel Address Least Significant 3 Bits */
#define   DDC0_DDC_ENABLE_DDC0_DBN_EN                0x02  /* [1] DDC0 Debounce Enable */
#define   DDC0_DDC_ENABLE_DDC0_DDC_EN                0x01  /* [0] DDC0 Channel Enable Switch */
#define DDC0_DDC_CONTROL_1           0xFF1F
#define   DDC0_DDC_CONTROL_1_DDC0_DBN_CLK_SEL        0xC0  /* [7:6] De-bounce Clock Divider */
#define   DDC0_DDC_CONTROL_1_DDC0_SYS_CK_SEL         0x08  /* [3] De-bounce Reference Clock */
#define   DDC0_DDC_CONTROL_1_DDC0_DDC2               0x04  /* [2] Force to DDC0 to DDC2-mode */
#define DDC0_DDC_CONTROL_2           0xFF20
#define   DDC0_DDC_CONTROL_2_DDC0_DDCSEG_EN          0x04  /* [2] DDC0 Segment Channel Enable Switch */
#define   DDC0_DDC_CONTROL_2_DDC0_FORCE_SCL_L        0x01  /* [0] Force External SCL Bus Low */

/* --- EMCU DDC-CI Part 1 (Page FF) --- */
#define IIC_SET_SLAVE                0xFF23
#define IIC_SUB_IN                   0xFF24
#define IIC_DATA_IN                  0xFF25
#define IIC_DATA_OUT                 0xFF26
#define IIC_STATUS                   0xFF27
#define   IIC_STATUS_STOP_I                          0x10  /* [4] If IIC Detects A STOP Condition(slave Address Must Match), This Bit... */
#define   IIC_STATUS_D_IN_I                          0x04  /* [2] If IIC_DATA_IN Latched, This Bit Is Set to "1" . Read IIC_data_in (... */
#define   IIC_STATUS_SUB_I                           0x02  /* [1] If IIC_SUB Latched, This Bit Is Set to "1" Write 0 to Clear. */
#define IIC_IRQ_CONTROL              0xFF28
#define IIC_STATUS2                  0xFF29
#define   IIC_STATUS2_IIC_BUF_EMPTY                  0x02  /* [1] IIC_DATA_BUFFER Empty */
#define IIC_IRQ_CONTROL2             0xFF2A
#define   IIC_IRQ_CONTROL2_RST_DATA_BUF              0x40  /* [6] Reset IIC_DATA Buffer */
#define   IIC_IRQ_CONTROL2_DATA_BUF_WEN              0x20  /* [5] IIC_DATA Buffer Write Enable */
#define   IIC_IRQ_CONTROL2_DDC_CH_AUTO_SWITCH_EN     0x10  /* [4] DDCCI Channel Auto Switch Enable */
#define IIC_CHANNEL_CONTROL          0xFF2B
#define   IIC_CHANNEL_CONTROL_CH_SWITCH_AUTO_RST_EN  0x80  /* [7] In Auto Switch Mode, Auto Reset Buffer When Channel Switch Happen */
#define   IIC_CHANNEL_CONTROL_NULL_MSG_EN            0x20  /* [5] Null Message Enable */

/* --- EMCU DDC Part 2 (Page FF) --- */
#define DDC1_DDC_ENABLE              0xFF2C
#define   DDC1_DDC_ENABLE_DDC1_DBN_EN                0x02  /* [1] DDC1 De-bounce Enable */
#define   DDC1_DDC_ENABLE_DDC1_DDC_EN                0x01  /* [0] DDC1 Channel Enable Bit */
#define DDC1_DDC_CONTROL_1           0xFF2D
#define   DDC1_DDC_CONTROL_1_DDC1_STOP_DBN_SEL       0x30  /* [5:4] De-bounce Sda Stage */
#define   DDC1_DDC_CONTROL_1_DDC1_STOP_DBN_SEL_SHIFT 4
#define   DDC1_DDC_CONTROL_1_DDC1_DDC2               0x04  /* [2] Force to DDC1 to DDC2-mode */
#define DDC1_DDC_CONTROL_2           0xFF2E
#define   DDC1_DDC_CONTROL_2_DDC1_DDCISP_EN          0x08  /* [3] DDC1 ISP Channel Enable Switch */
#define   DDC1_DDC_CONTROL_2_DDC1_DDCSEG_EN          0x04  /* [2] DDC1 Segment Channel Enable Switch */
#define   DDC1_DDC_CONTROL_2_DDC1_SEG_WR             0x02  /* [1] DDC1 Segment Write Status */

/* --- EMCU PWM Part 1 (Page FF) --- */
#define PWM_CK_SEL                   0xFF3A
#define   PWM_CK_SEL_WDT_EN_2                        0x40  /* [6] Watch Dog enable 2; */
#define   PWM_CK_SEL_PWM5_CK_SEL                     0x20  /* [5] PWMx Clock Generator Input Source */
#define PWM45_M                      0xFF3C
#define   PWM45_M_PWM5_M                             0x0C  /* [3:2] PWMx Clock First Stage Divider */
#define PWM45_N_MSB                  0xFF43
#define   PWM45_N_MSB_PWM5H_N                        0x0F  /* [3:0] PWMx Clock Second Stage Divider MSB[11:8] */
#define PWM5_N_LSB                   0xFF45
#define PWML                         0xFF46
#define   PWML_PWM5L                                 0x20  /* [5] 0: Enable Active H */
#define PWM_VS_CTRL                  0xFF47
#define   PWM_VS_CTRL_PWM5_VS_RST_EN                 0x20  /* [5] 0: Disable */
#define PWM_EN                       0xFF48
#define   PWM_EN_PWM5_EN                             0x20  /* [5] 0: PWM output disable */
#define PWM_CK                       0xFF49
#define   PWM_CK_PWM5_REF_CTRL_EN                    0x80  /* [7] 0: PWM5 normal output */
#define   PWM_CK_PWM5_CK                             0x20  /* [5] 0: Select first stage output */
#define PWM5H_DUT                    0xFF51
#define PWM_DUT_TYPE                 0xFF53
#define   PWM_DUT_TYPE_PWM5_DUT_8B                   0x20  /* [5] PWM5 Duty Width Type */
#define PWM_CNT_MODE                 0xFF54
#define   PWM_CNT_MODE_PWM5_CNT_MODE                 0x20  /* [5] 0: Duty mode */

/* --- EMCU SPI-FLASH Part 1 (Page FF) --- */
#define COMMON_INST_EN               0xFF60
#define   COMMON_INST_EN_COM_INST_EN                 0x01  /* [0] Common Instruction Enable (auto Clear When Finish) */
#define COMMON_OP_CODE               0xFF61
#define WREN_OP_CODE                 0xFF62
#define FLASH_PROG_ISP0              0xFF64
#define FLASH_PROG_ISP1              0xFF65
#define FLASH_PROG_ISP2              0xFF66
#define COMMON_INST_READ_PORT0       0xFF67
#define COMMON_INST_READ_PORT1       0xFF68
#define COMMON_INST_READ_PORT2       0xFF69
#define PROGRAM_OP_CODE              0xFF6D
#define READ_STATUS_REGISTER_OP_CODE 0xFF6E
#define PROGRAM_INSTRUCTION          0xFF6F
#define   PROGRAM_INSTRUCTION_PROG_EN                0x20  /* [5] 0: Finish */
#define PROGRAM_LENGTH               0xFF71

/* --- EMCU DDCCI Part 2 (Page FF) --- */
#define DDCCI_NULL_BYTE1             0xFFB1
#define DDCCI_NULL_BYTE2             0xFFB2
#define DDCCI_NULL_BYTE3             0xFFB3

/* --- EMCU In System Programming Part 1 (Page FF) --- */
#define ISP_FLAG                     0xFFE0

/* --- EMCU DDC Part 3 (Page FF) --- */
#define DDC0_EDID_IRQ                0xFFE4
#define   DDC0_EDID_IRQ_DDC0_FORCE_NACK              0x20  /* [5] In DDC2B Mode, Force DDC0 SDA No Output */
#define DDC1_EDID_IRQ                0xFFE5
#define   DDC1_EDID_IRQ_DDC1_FORCE_NACK              0x20  /* [5] In DDC2B Mode, Force DDC1 SDA Output 1(NACK) */
#define VGA_EDID_IRQ                 0xFFE7
#define   VGA_EDID_IRQ_ADC_FORCE_NACK                0x20  /* [5] In DDC2B Mode, Force ADC SDA Output 1(NACK) */

/* --- EMCU In System Programming Part 1 (Page FF) --- */
#define MCU_CLK_CONTROL_1            0xFFE9
#define   MCU_CLK_CONTROL_1_WDT_EN_3                 0x80  /* [7] Watch Dog enable 3; */
#define   MCU_CLK_CONTROL_1_FORCE_DDCRAM_ST          0x40  /* [6] Force ADC-DDC/DDC0/DDC1/DDC2/DDC3/DDC4/DDC5 DDC RAM Start Address t... */
#define   MCU_CLK_CONTROL_1_ISP_DDC2B_SWITCH         0x20  /* [5] 0:Not Allow DDC2B access in ISP Mode,ddc can program */
#define   MCU_CLK_CONTROL_1_MCU_CLK_SEL_EPLL         0x10  /* [4] CPU Clock Source Select */
#define   MCU_CLK_CONTROL_1_FLASH_CLK_DIV2           0x0F  /* [3:0] SPI-FLASH Clock Divider 2, Its Clock Source Is The Clock Divided by... */
#define   MCU_CLK_CONTROL_1_FLASH_CLK_DIV2_SHIFT     0

/* --- EMCU Watchdog Timer Part 2 (Page FF) --- */
#define WATCHDOG_TIMER               0xFFEA
#define   WATCHDOG_TIMER_WDT_EN                      0x80  /* [7] 0: Disable watchdog timer */
#define   WATCHDOG_TIMER_CLR_WDT                     0x40  /* [6] 0: No effect */
#define   WATCHDOG_TIMER_CNT1                        0x07  /* [2:0] The Number N of Counter1 */

/* --- EMCU In System Programming Part 2 (Page FF) --- */
#define MCU_CONTROL                  0xFFED
#define   MCU_CONTROL_FLASH_CLK_DIV                  0x3C  /* [5:2] SPI-FLASH Clock Divider, Its Clock Source Is Selected by MCU_CLK_SE... */
#define   MCU_CONTROL_FLASH_CLK_DIV_SHIFT            2
#define   MCU_CONTROL_MCU_CLK_SEL                    0x02  /* [1] CPU Clock Source Select */
#define MCU_CLOCK_CONTROL            0xFFEE
#define   MCU_CLOCK_CONTROL_MCU_PERI_NON_STOP        0x40  /* [6] 1: Keep mcu peripheral running whiel mcu stopped by spi flash access */
#define   MCU_CLOCK_CONTROL_MCU_CLK_DIV              0x3C  /* [5:2] MCU Clock Is FLASH Clock/MCU_CLK_DIV. */
#define   MCU_CLOCK_CONTROL_MCU_CLK_DIV_SHIFT        2

/* --- EMCU Scalar Interface (Page FF) --- */
#define SCA_INF_CONTROL              0xFFF3
#define   SCA_INF_CONTROL_ADDR_NON_INC               0x20  /* [5] 1: Turn-off address auto inc */
#define   SCA_INF_CONTROL_REG_BURCMD_WR              0x10  /* [4] Enable Burst Write Function, Mcu Will Halt Till Action Done Or An I... */
#define   SCA_INF_CONTROL_REG_BURDAT_WR              0x08  /* [3] Enable Burst Write Data to HOST_ADDR, Mcu Will Halt Till Action Don... */
#define SCA_INF_ADDR                 0xFFF4
#define SCA_INF_BWR_ADRH             0xFFF6
#define SCA_INF_BWR_ADRM             0xFFF7
#define SCA_INF_BWR_ADRL             0xFFF8
#define SCA_INF_BWR_COUNT_H          0xFFF9
#define SCA_INF_BWR_COUNT_L          0xFFFA
#define SCA_INF_PERIOD               0xFFFB

/* --- EMCU Bankswitch (Page FF) --- */
#define BANK_SWITCH_CONTROL          0xFFFC
#define   BANK_SWITCH_CONTROL_BURST_DATA_SOURCE_SEL  0x20  /* [5] 0: Burst data from Ext flash */
#define   BANK_SWITCH_CONTROL_SCALAR_ADDR_REMAPPING  0x10  /* [4] Scalar Addresss Remapping to Xdata, (0x00_00 ~ 0xFE_FF) */
#define   BANK_SWITCH_CONTROL_GLOBAL_XFR             0x08  /* [3] 1: XFR will occupy the address space of all XDATA banks. */
#define   BANK_SWITCH_CONTROL_GLOBAL_XRAM            0x04  /* [2] 1: XRAM will occupy the address space of all XDATA banks. */
#define   BANK_SWITCH_CONTROL_SW_MODE                0x02  /* [1] 0: Using P3.5 as A16 */
#define   BANK_SWITCH_CONTROL_BANK_EN                0x01  /* [0] 1: Enable Bank Switching Function for program address space */
#define XDATA_BANK_START             0xFFFD
#define XDATA_BANK_SEL               0xFFFE
#define PBANK_SWITCH                 0xFFFF

#endif /* RTD2556_REGS_PAGE_FF_H */
