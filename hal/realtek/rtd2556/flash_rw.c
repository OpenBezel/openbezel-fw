/* Firmware-side SPI flash access - see flash_rw.h. */
#include "flash_rw.h"
#include "rtd2556_regs.h"

__sfr __at(0x8E) SFR_8E;        /* [2:0] = 7 around prog_en */
__sbit __at(0xAF) EA_BIT;

/* COMMON_INST_EN: [7:5] type, [4:3] write bytes, [2:1] read bytes, [0] go */
#define CI(type, wr, rd) (u8)(((type) << 5) | ((wr) << 3) | ((rd) << 1))
#define CI_READ 2
#define CI_ERASE 5

#define OP_READ 0x03
#define OP_RDSR 0x05
#define OP_SE 0x20

#define WIP_POLLS 60000U        /* each poll is a full common command: >> 1 s */

#define OP_WREN 0x06
#define OP_PP 0x02

u8 flash_op_reset[3];
static u8 ops_set;

static void ops_init(void)
{
    if (ops_set)
        return;
    flash_op_reset[0] = SR(WREN_OP_CODE);
    flash_op_reset[1] = SR(PROGRAM_OP_CODE);
    flash_op_reset[2] = SR(READ_STATUS_REGISTER_OP_CODE);
    SR(WREN_OP_CODE) = OP_WREN;
    SR(PROGRAM_OP_CODE) = OP_PP;
    SR(READ_STATUS_REGISTER_OP_CODE) = OP_RDSR;
    ops_set = 1;
}

static void ci_addr(u32 a)
{
    SR(FLASH_PROG_ISP0) = BYTE2(a);
    SR(FLASH_PROG_ISP1) = HI(a);
    SR(FLASH_PROG_ISP2) = LO(a);
}

static void ci_go(u8 inst, u8 op)
{
    u16 n = 0;

    SR(COMMON_INST_EN) = inst;
    SR(COMMON_OP_CODE) = op;
    SR(COMMON_INST_EN) = (u8)(inst | COMMON_INST_EN_COM_INST_EN);
    while ((SR(COMMON_INST_EN) & COMMON_INST_EN_COM_INST_EN) && ++n)
        ;
}

u8 flash_status(void)
{
    ci_go(CI(CI_READ, 0, 1), OP_RDSR);
    return SR(COMMON_INST_READ_PORT0);
}

static u8 wait_idle(void)
{
    u16 n;

    for (n = 0; n < WIP_POLLS; n++)
        if (!(flash_status() & 0x01))
            return FLASH_OK;
    return FLASH_E_TIMEOUT;
}

void flash_read(u32 addr, __xdata u8 *dst, u16 n)
{
    u8 k;

    while (n) {
        k = (n > 3) ? 3 : (u8)n;
        ci_addr(addr);
        ci_go(CI(CI_READ, 3, k), OP_READ);
        dst[0] = SR(COMMON_INST_READ_PORT0);
        if (k > 1) dst[1] = SR(COMMON_INST_READ_PORT1);
        if (k > 2) dst[2] = SR(COMMON_INST_READ_PORT2);
        dst += k;
        addr += k;
        n -= k;
    }
}

static u8 in_window(u32 addr, u16 n)
{
    return (addr >= FLASH_RW_LO && addr + n <= FLASH_RW_HI) ? 1 : 0;
}

u8 flash_erase_sector(u32 addr)
{
    addr &= ~(u32)(FLASH_SECTOR - 1);
    if (!in_window(addr, FLASH_SECTOR))
        return FLASH_E_RANGE;
    ops_init();
    ci_addr(addr);
    ci_go(CI(CI_ERASE, 3, 0), OP_SE);
    SR(COMMON_OP_CODE) = 0xFF;
    return wait_idle();
}

u8 flash_program(u32 addr, const __xdata u8 *src, u16 n)
{
    static __xdata u8 rb[3];
    u16 i;
    u8 r;

    if (n == 0 || n > FLASH_PAGE || !in_window(addr, n)
        || ((u16)(addr & (FLASH_PAGE - 1)) + n) > FLASH_PAGE)
        return FLASH_E_RANGE;
    ops_init();
    for (i = 0; i < n; i++)
        FLASH_PROG_BUF[i] = src[i];
    EA_BIT = 0;
    ci_addr(addr);
    SR(PROGRAM_LENGTH) = (u8)(n - 1);
    SFR_8E = (u8)((SFR_8E & 0xF8) | 0x07);
    SR(PROGRAM_INSTRUCTION) = (u8)((SR(PROGRAM_INSTRUCTION) & (u8)~PROGRAM_INSTRUCTION_PROG_EN)
                                   | PROGRAM_INSTRUCTION_PROG_EN);
    SFR_8E &= 0xF8;
    EA_BIT = 1;
    for (i = 1; (SR(PROGRAM_INSTRUCTION) & PROGRAM_INSTRUCTION_PROG_EN) && i; i++)
        ;
    r = wait_idle();
    if (r != FLASH_OK)
        return r;
    for (i = 0; i < n; i += 3) {
        u8 k = (n - i > 3) ? 3 : (u8)(n - i), j;
        flash_read(addr + i, rb, k);
        for (j = 0; j < k; j++)
            if (rb[j] != src[i + j])
                return FLASH_E_VERIFY;
    }
    return FLASH_OK;
}
