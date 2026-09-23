#include "glanz.h"

static bool s_laeuft;
static Sportart s_art;
static uint32_t s_beginn;

static void prv_fuellen(AppGlanceReloadSession *sitzung, size_t platz, void *context) {
  if (!s_laeuft || platz == 0) return;

  // DIE ZEIT ZAEHLT DIE UHR SELBST. Der Text ist eine Vorlage: time_since
  // rechnet das System beim Anzeigen aus, nicht die App beim Verlassen -
  // sonst stuende eine halbe Stunde spaeter noch "seit 2 min" da.
  char vorlage[96];
  if (s_beginn > 0) {
    snprintf(vorlage, sizeof(vorlage),
             "%s im Hintergrund, seit {time_since(%lu)|format('>0S:%%aS','>0M:%%aM','>0H:%%aH')}",
             art_name(s_art), (unsigned long)s_beginn);
  } else {
    snprintf(vorlage, sizeof(vorlage), "%s im Hintergrund", art_name(s_art));
  }

  AppGlanceSlice scheibe = {
    .layout = {
      .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
      .subtitle_template_string = vorlage,
    },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  if (app_glance_add_slice(sitzung, scheibe) != APP_GLANCE_RESULT_SUCCESS) {
    // Versteht die Uhr die Vorlage nicht, wenigstens das Wesentliche.
    char schlicht[48];
    snprintf(schlicht, sizeof(schlicht), "%s im Hintergrund", art_name(s_art));
    scheibe.layout.subtitle_template_string = schlicht;
    app_glance_add_slice(sitzung, scheibe);
  }
}

void glanz_setzen(bool laeuft, Sportart art, uint32_t beginn) {
  s_laeuft = laeuft;
  s_art = art;
  s_beginn = beginn;
  app_glance_reload(prv_fuellen, NULL);
}
