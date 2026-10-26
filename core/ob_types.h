/* Basic types and helpers for OpenBezel firmware (SDCC, mcs51). */
#ifndef OB_TYPES_H
#define OB_TYPES_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

#include <8051.h> /* standard SFRs: IE, EA, TCON, ... */

/* The bytes of a 16- or 24-bit value, for registers split across bytes: a 16-bit
 * pair is HI/LO, a 24-bit H/M/L triple is BYTE2/HI/LO. */
#define LO(v) ((u8)(v))
#define HI(v) ((u8)((v) >> 8))
#define BYTE2(v) ((u8)((v) >> 16))

/* XRAM above the linker's data region (0xD700-0xDFFF):
 * 0xE000-0xE0FF is the flash program buffer - hands off; 0xE100-0xE1FF holds
 * these fixed 64-byte scratch buffers. */
#define XRAM_STORE_REC   0xE100
#define XRAM_STORE_SAVED 0xE140
#define XRAM_STORE_TMP   0xE180
#define XRAM_LANG_BUF    0xE1C0

#endif
