/* Persistent settings log - see settings_store.h. */
#include "settings_store.h"
#include "flash_rw.h"

#define STORE_BASE 0x100000UL           /* first sector of the writable half */
#define STORE_REC 64U                   /* bytes per record, header and CRC included */
#define STORE_SLOTS (FLASH_SECTOR / STORE_REC)
#define STORE_VERSION 2                 /* 2: 64-byte records; a version-1 log loads defaults once */
#define STORE_HDR 6
#define STORE_PAYLOAD (STORE_REC - STORE_HDR - 1)

/* The settings must fit one record: a build that grows the struct past it
 * fails here, not in the field. */
typedef char store_record_fits[(sizeof(struct ob_settings) <= STORE_PAYLOAD) ? 1 : -1];

u8 store_sector = 0xFF, store_slot;
u16 store_seq;
u8 store_last = 0xFF;

/* Fixed in the XRAM above the linker's region:
 * scratch only, nothing relies on them starting at zero. */
static __xdata __at(XRAM_STORE_REC) u8 rec[STORE_REC];
static __xdata __at(XRAM_STORE_SAVED) struct ob_settings saved;
typedef char store_slots_fit[(STORE_REC <= 64 && sizeof(struct ob_settings) <= 64) ? 1 : -1];
static u8 have_saved, scanned;

static u32 slot_addr(u8 sector, u8 slot)
{
    return STORE_BASE + (u32)sector * FLASH_SECTOR + (u32)slot * STORE_REC;
}

static u8 crc8(u8 n)
{
    u8 c = 0, i, b;

    for (i = 0; i < n; i++) {
        c ^= rec[i];
        for (b = 0; b < 8; b++)
            c = (c & 0x80) ? (u8)((c << 1) ^ 0x07) : (u8)(c << 1);
    }
    return c;
}

static u8 rec_valid(void)
{
    return rec[0] == 'O' && rec[1] == 'B' && rec[2] == STORE_VERSION
        && rec[3] != 0 && rec[3] <= sizeof(struct ob_settings)
        && crc8(STORE_REC - 1) == rec[STORE_REC - 1];
}

static u8 same(const __xdata u8 *a, const __xdata u8 *b, u8 n)
{
    while (n--)
        if (*a++ != *b++)
            return 0;
    return 1;
}

u8 store_load(__xdata struct ob_settings *out)
{
    u8 sec, slot, best_sec = 0xFF, best_slot = 0, first_free[2], i;
    u16 best_seq = 0, s;

    scanned = 1;
    have_saved = 0;
    for (sec = 0; sec < 2; sec++) {
        first_free[sec] = STORE_SLOTS;
        for (slot = 0; slot < STORE_SLOTS; slot++) {
            flash_read(slot_addr(sec, slot), rec, 1);
            if (rec[0] == 0xFF) {       /* slots fill in order: the rest is erased */
                first_free[sec] = slot;
                break;
            }
            flash_read(slot_addr(sec, slot), rec, STORE_REC);
            if (!rec_valid())
                continue;               /* torn or foreign: counts as used */
            s = (u16)(rec[4] | ((u16)rec[5] << 8));
            if (best_sec == 0xFF || (s16)(s - best_seq) > 0) {
                best_sec = sec;
                best_slot = slot;
                best_seq = s;
            }
        }
    }
    if (best_sec == 0xFF) {
        store_sector = 0xFF;
        store_slot = 0;
        store_seq = 0;
        return STORE_NONE;
    }
    flash_read(slot_addr(best_sec, best_slot), rec, STORE_REC);
    /* A shorter record is from a build before the struct grew: only its
     * bytes are loaded, the appended fields keep what *out already holds
     * (settings_load() passes the defaults). */
    for (i = 0; i < rec[3]; i++)
        ((__xdata u8 *)out)[i] = rec[STORE_HDR + i];
    for (i = 0; i < sizeof(struct ob_settings); i++)
        ((__xdata u8 *)&saved)[i] = ((__xdata u8 *)out)[i];
    have_saved = 1;
    store_sector = best_sec;
    store_slot = first_free[best_sec];
    store_seq = best_seq;
    return STORE_OK;
}

static u8 store_save_(const __xdata struct ob_settings *s);

u8 store_save(const __xdata struct ob_settings *s)
{
    store_last = store_save_(s);
    return store_last;
}

static u8 store_save_(const __xdata struct ob_settings *s)
{
    u8 i, r, tries;

    if (!scanned) {
        static __xdata __at(XRAM_STORE_TMP) struct ob_settings tmp;
        store_load(&tmp);
    }
    if (have_saved && same((const __xdata u8 *)s, (const __xdata u8 *)&saved, sizeof(saved)))
        return STORE_OK;                /* nothing changed: no flash wear */

    for (tries = 0; tries < 2; tries++) {
        if (store_sector == 0xFF || store_slot >= STORE_SLOTS) {
            /* Start the other sector - never the one holding the newest
             * record, so that one survives a power cut mid-erase. */
            store_sector = (store_sector == 0xFF) ? 0 : (u8)(store_sector ^ 1);
            store_slot = 0;
            r = flash_erase_sector(slot_addr(store_sector, 0));
            if (r != FLASH_OK)
                return r;
        }
        rec[0] = 'O';
        rec[1] = 'B';
        rec[2] = STORE_VERSION;
        rec[3] = sizeof(struct ob_settings);
        rec[4] = LO(store_seq + 1);
        rec[5] = HI(store_seq + 1);
        for (i = 0; i < STORE_PAYLOAD; i++)
            rec[STORE_HDR + i] = (i < sizeof(struct ob_settings)) ? ((const __xdata u8 *)s)[i] : 0xFF;
        rec[STORE_REC - 1] = crc8(STORE_REC - 1);
        r = flash_program(slot_addr(store_sector, store_slot), rec, STORE_REC);
        store_slot++;                   /* used either way: a failed slot is not reused */
        if (r == FLASH_OK) {
            store_seq++;
            for (i = 0; i < sizeof(saved); i++)
                ((__xdata u8 *)&saved)[i] = ((const __xdata u8 *)s)[i];
            have_saved = 1;
            return STORE_OK;
        }
        if (r != FLASH_E_VERIFY)
            return r;
    }
    return FLASH_E_VERIFY;
}
