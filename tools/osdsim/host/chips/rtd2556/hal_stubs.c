/* osdsim, RTD2556: the rest of the HAL the menu and settings code call - picture,
 * audio, backlight, persistence.  Nothing here affects the OSD, so they do nothing
 * (the settings themselves still change: the menu shows them). */
#include "ob_types.h"
#include "settings.h"
#include "settings_store.h"

void audio_set(u8 volume, u8 mute) { (void)volume; (void)mute; }
void backlight_pwm_level(u8 pct) { (void)pct; }
void display_dither(u8 on) { (void)on; }
void display_wait_vsync(void) { }
void hdmi_scaling_set(u8 mode) { (void)mode; }
void keys_set_repeat(u16 delay, u16 rate) { (void)delay; (void)rate; }
void mccs_note_setting(const void *val) { (void)val; }
void pipeline_gamma_load(u8 idx) { (void)idx; }
void pipeline_idither_load(u8 bpc, u8 on) { (void)bpc; (void)on; }
void pipeline_picture_levels(s8 b, s8 c, u8 t, u8 lb) { (void)b; (void)c; (void)t; (void)lb; }
void pipeline_saturation(u8 s) { (void)s; }
void pipeline_scaleup_coefs_load(u8 s) { (void)s; }
void pipeline_user_gains(u8 r, u8 g, u8 b) { (void)r; (void)g; (void)b; }
void source_remode(void) { }
u8 store_load(struct ob_settings *out) { (void)out; return STORE_NONE; }
u8 store_save(const struct ob_settings *s) { (void)s; return STORE_OK; }
