/* Picture modes - see picmode.h. */
#include "picmode.h"
#include "settings.h"
#include "pipeline.h"

struct preset {
    s8 brightness, contrast;
    u8 saturation, gamma, sharpness, ctemp, low_blue, dcr;
};

/* gamma: 0 = panel native (bypass), 2 = 2.0, 3 = 2.2, 4 = 2.4 (core/mccs.c) */
static const __code struct preset presets[PICMODE_COUNT - 1] = {
    /* Standard: the factory defaults */
    { 0, 0, PIPELINE_SAT_NORMAL, PIPELINE_GAMMA_DEFAULT, PIPELINE_SHARPNESS_DEFAULT, PIPELINE_CTEMP_NORMAL, 0, 0 },
    /* Movie: deeper blacks (2.4, dynamic contrast), a little richer and warmer */
    { -4, 6, 110, 4, 0, PIPELINE_CTEMP_WARM, 0, 1 },
    /* Game: lifted shadows (2.0), brighter, more saturated, crisper */
    { 4, 4, 120, 2, 2, PIPELINE_CTEMP_NORMAL, 0, 0 },
    /* Text: softer contrast, muted color, warm with a low blue cut */
    { 0, -6, 90, 3, 1, PIPELINE_CTEMP_WARM, 1, 0 },
};

static __xdata u8 pm_i;
static const __code struct preset *__xdata pm_p;

u8 picmode_now(void)
{
    for (pm_i = 0; pm_i < PICMODE_COUNT - 1; pm_i++) {
        pm_p = &presets[pm_i];
        if (pm_p->brightness != ob_settings.brightness) continue;
        if (pm_p->contrast != ob_settings.contrast) continue;
        if (pm_p->saturation != ob_settings.saturation) continue;
        if (pm_p->gamma != ob_settings.gamma) continue;
        if (pm_p->sharpness != ob_settings.sharpness) continue;
        if (pm_p->ctemp != ob_settings.ctemp) continue;
#ifdef CONFIG_FEATURE_LOW_BLUE
        if (pm_p->low_blue != ob_settings.low_blue) continue;
#endif
#ifdef CONFIG_FEATURE_DCR
        if (pm_p->dcr != ob_settings.dcr) continue;
#endif
        return (u8)(pm_i + 1);
    }
    return PICMODE_CUSTOM;
}

void picmode_apply(u8 mode)
{
    if (mode == PICMODE_CUSTOM || mode >= PICMODE_COUNT)
        return;                         /* Custom is what the settings are: nothing to set */
    pm_p = &presets[mode - 1];
    ob_settings.brightness = pm_p->brightness;
    ob_settings.contrast = pm_p->contrast;
    ob_settings.saturation = pm_p->saturation;
    ob_settings.gamma = pm_p->gamma;
    ob_settings.sharpness = pm_p->sharpness;
    ob_settings.ctemp = pm_p->ctemp;
#ifdef CONFIG_FEATURE_LOW_BLUE
    ob_settings.low_blue = pm_p->low_blue;
#endif
#ifdef CONFIG_FEATURE_DCR
    ob_settings.dcr = pm_p->dcr;        /* dcr_tick() follows */
#endif
    settings_apply_levels();
    settings_apply_saturation();
    settings_apply_gamma();
    settings_apply_sharpness();
}
