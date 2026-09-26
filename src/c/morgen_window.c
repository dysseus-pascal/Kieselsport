#include "morgen_window.h"
#include "telefon.h"
#include "thema.h"
#include "strings.h"

#define MORGEN_HOECHSTENS_MS 60000

static Window *s_fenster;
static Layer *s_flaeche;
static AppTimer *s_zu;
static bool s_fertig;
// Was geholt wurde: ein liegengebliebenes Training oder die Nacht.
static bool s_training;

static void prv_zeichne(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_unobstructed_bounds(layer);
  const int16_t w = b.size.w - KS_LEISTE_B;
  const int16_t rand = PBL_IF_ROUND_ELSE(44, KS_RAND);
  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  const char *text = s_training ? (s_fertig ? S(STR_TRAINING_FERTIG) : S(STR_TRAINING_SENDEN))
                                 : (s_fertig ? S(STR_NACHT_FERTIG) : S(STR_NACHT_SENDEN));
  graphics_draw_text(ctx, text,
                     fonts_get_system_font(KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD),
                     GRect(rand, b.size.h / 2 - 30, w - rand - 4, 60),
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  thema_leiste(ctx, b, SymbolKeins, SymbolKeins, SymbolKeins);
}

static void prv_zu(void *data) {
  s_zu = NULL;
  // Die ganze App geht zu: sie wurde nur zum Senden geholt.
  window_stack_pop_all(true);
}

static void prv_fertig(void) {
  s_fertig = true;
  if (s_flaeche) layer_mark_dirty(s_flaeche);
  if (s_zu) app_timer_cancel(s_zu);
  s_zu = app_timer_register(1500, prv_zu, NULL);
}

static void prv_laden(Window *fenster) {
  Layer *wurzel = window_get_root_layer(fenster);
  s_flaeche = layer_create(layer_get_bounds(wurzel));
  layer_set_update_proc(s_flaeche, prv_zeichne);
  layer_add_child(wurzel, s_flaeche);
}

static void prv_entladen(Window *fenster) {
  telefon_bei_nacht_fertig(NULL);
  if (s_zu) { app_timer_cancel(s_zu); s_zu = NULL; }
  layer_destroy(s_flaeche);
  s_flaeche = NULL;
  window_destroy(s_fenster);
  s_fenster = NULL;
}

void morgen_window_zeige(void) {
  s_fertig = false;
  // Die Zusammenfassung geht vor der Nacht (telefon.c); fertig ist es erst,
  // wenn auch die Nacht drueben ist - dann ruft telefon.c prv_fertig.
  s_training = telefon_wartet();
  s_fenster = window_create();
  window_set_background_color(s_fenster, KS_FARBE_GRUND);
  window_set_window_handlers(s_fenster, (WindowHandlers) { .load = prv_laden, .unload = prv_entladen });
  window_stack_push(s_fenster, false);
  telefon_bei_nacht_fertig(prv_fertig);
  // Ist das Telefon nicht da, bleibt die Nacht im Persist und geht beim
  // naechsten Oeffnen - die Uhr soll dafuer nicht den ganzen Morgen offen
  // stehen.
  s_zu = app_timer_register(MORGEN_HOECHSTENS_MS, prv_zu, NULL);
}
