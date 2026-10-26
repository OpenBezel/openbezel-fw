/* Menu languages: the strings of the menu and popups, by id (S_* in the generated
 * lang_ids.h), in the language ob_settings.language selects.
 *
 * The text lives in the asset partition (tools/gen_lang.py from lang/<code>.txt, docs in
 * lang/en.txt), not in code: every language costs flash, not a code bank.  A string
 * is read from flash into ONE shared buffer, so the pointer is good until the next
 * lang_* call - use it straight away (con_puts copies it). */
#ifndef OB_LANG_H
#define OB_LANG_H

#include "ob_types.h"
#include "lang_ids.h"

const char *lang_str(u8 id);                /* in the current language */
const char *lang_str_in(u8 lang, u8 id);    /* in language `lang` (0..LANG_COUNT-1) */

#endif
