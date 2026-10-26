/* Firmware-side SPI flash access: read, 4 KB sector erase, program - for the
 * settings log (core/settings_store.c).  NOT the host ISP path: the program
 * sequence there needs PROGRAM_INSTRUCTION isp_en, which halts the 8051.
 * The firmware path:
 *
 *   read     common instruction, type READ, 3 address bytes, 3 data bytes
 *            (COMMON_INST_EN 0x5E/0x5F, opcode 03)
 *   erase    common instruction, type ERASE, 3 address bytes (0xB8/0xB9,
 *            opcode 20)
 *   program  address in FLASH_PROG_ISP0..2, PROGRAM_LENGTH = n - 1, the DATA
 *            IN XRAM AT 0xE000, SFR 0x8E[2:0] = 7 around PROGRAM_INSTRUCTION
 *            prog_en, interrupts off
 *
 * The controller probably holds the MCU's own code fetches until the flash is
 * ready after an erase; we poll WIP anyway.
 *
 * SAFETY: every write is refused outside FLASH_RW_LO..FLASH_RW_HI (the upper
 * 1 MB), and the flash itself is set (rtdflash.py protect, SR1 = 0xB4) to
 * protect the lower 1 MB - code, assets, NVRAM - so even a bug here
 * cannot reach code.  Nothing here may run before the ISP window. */
#ifndef RTD2556_FLASH_RW_H
#define RTD2556_FLASH_RW_H

#include "ob_types.h"

#define FLASH_RW_LO 0x100000UL
#define FLASH_RW_HI 0x200000UL          /* exclusive */
#define FLASH_SECTOR 4096U
#define FLASH_PAGE 256U
#define FLASH_PROG_BUF ((volatile __xdata u8 *)0xE000)   /* outside the linker's XDATA */

#define FLASH_OK 0
#define FLASH_E_RANGE 1                 /* outside the writable window */
#define FLASH_E_TIMEOUT 2               /* WIP never cleared */
#define FLASH_E_VERIFY 3                /* read-back differs */

/* The opcodes the controller issues ON ITS OWN - the write-enable before a
 * program, the program itself, and its busy polling - are registers.  Reset
 * values are WREN 06, PROGRAM **FF**, RDSR 05: left alone, every program goes
 * out as opcode FF and silently does nothing (WEL left set).  A host ISP
 * session loads them, which masks this until a real power cycle.  We set
 * them before the first write, and keep
 * the reset values for the debug peek (WREN, PROGRAM, RDSR).  READ / FAST_READ
 * are left alone - they are what the CPU fetches code with. */
extern u8 flash_op_reset[3];

void flash_read(u32 addr, __xdata u8 *dst, u16 n);
u8 flash_status(void);                  /* RDSR */
u8 flash_erase_sector(u32 addr);
/* Program n (1..256) bytes from src; must not cross a 256-byte page and the
 * destination must be erased.  Verifies by reading back. */
u8 flash_program(u32 addr, const __xdata u8 *src, u16 n);

#endif
