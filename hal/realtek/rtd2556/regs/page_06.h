/* RTD2556 scaler page 0x06 */
#ifndef RTD2556_REGS_PAGE_06_H
#define RTD2556_REGS_PAGE_06_H

/* --- Audio (Page 6) --- */
#define REG_DAC_POWER0   0x06C0
#define REG_DAC_POWER1   0x06C1
#define   REG_DAC_POWER1_REG_POW_DEPOP      0x80  /* [7] Power Down Control for DEPOP */
#define   REG_DAC_POWER1_REG_POW_DEPOP_CK   0x40  /* [6] Power Down Control for DEPOP Ckgen */
#define   REG_DAC_POWER1_REG_POW_DEPOP_CORE 0x20  /* [5] Power Down Control for DEPOP Core */
#define   REG_DAC_POWER1_REG_POW_DEPOP_OP   0x10  /* [4] Power Down Control for DEPOP Op */
#define   REG_DAC_POWER1_REG_VCM_SEL        0x03  /* [1:0] Analog Vcm Voltage Selection */
#define   REG_DAC_POWER1_REG_VCM_SEL_SHIFT  0
#define REG_AOUT_CTRL    0x06C6
#define   REG_AOUT_CTRL_REG_MUTE_AOUT_L     0x80  /* [7] Mute Control for AOUT_L */
#define   REG_AOUT_CTRL_REG_MUTE_AOUT_R     0x40  /* [6] Mute Control for AOUT_R */
#define   REG_AOUT_CTRL_REG_AOUT_NORM       0x08  /* [3] Select Operation Mode between Depop And Normal for AOUT. */
#define REG_HPOUT_CTRL   0x06C7
#define   REG_HPOUT_CTRL_REG_MUTE_HPOUT_L   0x80  /* [7] Mute Control for HPOUT_L */
#define   REG_HPOUT_CTRL_REG_MUTE_HPOUT_R   0x40  /* [6] Mute Control for HPOUT_R */
#define   REG_HPOUT_CTRL_REG_HP_NORM        0x08  /* [3] Select Operation Mode between Depop And Normal for HPOUT. */
#define REG_ANA_RESERVE0 0x06C9
#define DA_MUTE          0x06D1
#define   DA_MUTE_DA_MUTE_L                 0x80  /* [7] Digital Mute from DAC Left Channel Digital Mixer */
#define   DA_MUTE_DA_MUTE_R                 0x40  /* [6] Digital Mute from DAC Right Channel Digital Mixer */
#define   DA_MUTE_DAMOD_RST_N               0x01  /* [0] To reset sigma-delta modulator */
#define DA_GAIN_L        0x06D2
#define   DA_GAIN_L_DA_GAIN_DB              0x80  /* [7] Need to set this double buffer bit after adjusting DAC gain */
#define DA_GAIN_R        0x06D3
#define DA_CLK_CTRL      0x06D4
#define   DA_CLK_CTRL_DEPOP_CLK_EN          0x10  /* [4] De-pop Clock Enable */
#define   DA_CLK_CTRL_DEPOP_CLK_DIV         0x0F  /* [3:0] Clock to De-pop Circuit (14.318M / 64) */
#define   DA_CLK_CTRL_DEPOP_CLK_DIV_SHIFT   0

#endif /* RTD2556_REGS_PAGE_06_H */
