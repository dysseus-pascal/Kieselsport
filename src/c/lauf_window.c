#include "lauf_window.h"
#include "training.h"
#include "puls.h"
#include "telefon.h"

// Der laufende Schirm.
//
// EINE ZAHL IST GROSS, DIE ANDEREN SIND KLEIN. Beim Laufen schaut man im
// Vorbeigehen hin, mit schwingendem Arm; was dann lesbar sein muss, ist die
// Zeit und der Puls. Alles andere liest man in der Pause oder danach.
//
// DREI SEHR VERSCHIEDENE SCHIRME. 200x228 in Farbe, 144x168 schwarzweiss,
// 260x260 rund - Letzteres schneidet die Ecken ab. Deshalb wird nichts fest
// gesetzt, sondern alles aus der Fensterbreite gerechnet, und auf der runden
// Uhr bekommt jede Zeile mehr Luft an den Seiten.

static Window *s_fenster;
static Layer *s_flaeche;
static TextLayer *s_kopf;
static AppTimer *s_takt;
static int s_letzte_zone = -1;

static void prv_zeichne(Layer *layer, GContext *ctx);

static void prv_tick(void *data) {
  training_tick();

  // ZONENWECHSEL MELDEN. Das ist die eine Stelle, an der die Uhr von sich aus
  // etwas sagt - und der Grund, warum man sie beim Sport ueberhaupt anschaut.
  const int zone = puls_zone(training_puls());
  if (training_zustand() == LaufLaeuft && zone > 0 && s_letzte_zone > 0 && zone != s_letzte_zone) {
    // Hoch: zwei kurze. Runter: eine. Wer laeuft, soll sie unterscheiden
    // koennen, ohne hinzusehen.
    if (zone > s_letzte_zone) {
      static const uint32_t hoch[] = { 60, 80, 60 };
      VibePattern muster = { .durations = hoch, .num_segments = 3 };
      vibes_enqueue_custom_pattern(muster);
    } else {
      vibes_short_pulse();
    }
  }
  if (zone > 0) s_letzte_zone = zone;

  layer_mark_dirty(s_flaeche);
  s_takt = app_timer_register(1000, prv_tick, NULL);
}

static void prv_zeit(char *aus, size_t platz, uint32_t sekunden) {
  const uint32_t h = sekunden / 3600;
  const uint32_t m = (sekunden / 60) % 60;
  const uint32_t s = sekunden % 60;
  if (h > 0) snprintf(aus, platz, "%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
  else snprintf(aus, platz, "%u:%02u", (unsigned)m, (unsigned)s);
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const int16_t rand = PBL_IF_ROUND_ELSE(bounds.size.w / 6, 8);
  const int16_t breite = bounds.size.w - 2 * rand;
  // Auf der runden Uhr tiefer anfangen: der Block ist rund 150 Punkte hoch,
  // auf 260 Punkten stuende er sonst oben und liesse das untere Drittel
  // leer - und genau dort ist der Kreis am breitesten.
  int16_t y = PBL_IF_ROUND_ELSE(52, 4);

  graphics_context_set_text_color(ctx, GColorBlack);

  const Trainingsstand t = training_stand();
  const uint16_t puls = training_puls();
  const int zone = puls_zone(puls);
  char text[24];

  // --- Die Zeit, so gross wie moeglich ---
  prv_zeit(text, sizeof(text), t.dauer_s);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS),
                     GRect(rand, y, breite, 44),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += 42;

  // --- Puls mit Zonenbalken ---
  if (puls > 0) {
    snprintf(text, sizeof(text), "%u", (unsigned)puls);
    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS),
                       GRect(rand, y, breite / 2, 32),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    snprintf(text, sizeof(text), "Zone %d", zone);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(rand + breite / 2, y + 8, breite / 2, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    y += 34;

    // Der Balken: fuenf Felder, das erreichte gefuellt. Auf schwarzweissen
    // Uhren ist er die Zone, weil dort keine Farbe sie tragen kann.
    const int16_t fach = breite / KS_ZONEN;
    for (int z = 1; z <= KS_ZONEN; z++) {
      const GRect kasten = GRect(rand + (z - 1) * fach, y, fach - 2, 6);
      if (z <= zone) {
        graphics_context_set_fill_color(ctx, puls_zonenfarbe(z));
        graphics_fill_rect(ctx, kasten, 0, GCornerNone);
      } else {
        graphics_context_set_stroke_color(ctx, GColorDarkGray);
        graphics_draw_rect(ctx, kasten);
      }
    }
    y += 12;
  } else {
    graphics_draw_text(ctx, "kein Puls", fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(rand, y, breite, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    y += 26;
  }

  // --- Die kleinen Felder, nur was die Art hergibt ---
  //
  // EINE REIHE, SO VIELE SPALTEN WIE FELDER. Der erste Entwurf setzte zwei
  // je Reihe - auf flint fiel damit das dritte Feld unten aus dem Schirm.
  // 168 Punkte tragen keine zwei Reihen mehr, 228 schon; also richtet sich
  // die Aufteilung nach der Zahl der Felder und nicht nach einer Annahme.
  const ArtInfo *info = art_info(training_art());
  const char *namen[3];
  char werte[3][16];
  int felder = 0;

  if (info->schritte) {
    // Auf schmalen Spalten abgekuerzt: "Schrit..." sagt weniger als "Schr."
    namen[felder] = (breite / (info->distanz ? 3 : 2)) < 50 ? "Schr." : "Schritte";
    snprintf(werte[felder], sizeof(werte[0]), "%u", (unsigned)t.schritte);
    felder++;
  }
  if (info->distanz) {
    namen[felder] = "km";
    snprintf(werte[felder], sizeof(werte[0]), "%u.%02u", (unsigned)(t.meter / 1000),
             (unsigned)((t.meter % 1000) / 10));
    felder++;
  }
  namen[felder] = "kcal";
  snprintf(werte[felder], sizeof(werte[0]), "%u", (unsigned)t.kcal);
  felder++;

  const int16_t spaltenbreite = breite / felder;
  // Die Zahl schrumpft mit der Spalte: "1234" in 24 Punkt braucht gut
  // vierzig Punkte Breite, und drei Spalten auf 144 lassen keine vierzig.
  GFont zahlenschrift = spaltenbreite >= 62
      ? fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD)
      : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  for (int i = 0; i < felder; i++) {
    const GRect kasten = GRect(rand + i * spaltenbreite, y, spaltenbreite - 3, 40);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, namen[i], fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(kasten.origin.x, kasten.origin.y, kasten.size.w, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, werte[i], zahlenschrift,
                       GRect(kasten.origin.x, kasten.origin.y + 13, kasten.size.w, 28),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  // --- Pause als Zustand, nicht als Meldung ---
  if (training_zustand() == LaufPause) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "PAUSE", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(rand, bounds.size.h - PBL_IF_ROUND_ELSE(40, 24), breite, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// --- Bedienung ---

static void prv_auswahl(ClickRecognizerRef anlass, void *context) {
  training_pause_umschalten();
  layer_mark_dirty(s_flaeche);
}

static void prv_zurueck(ClickRecognizerRef anlass, void *context) {
  // ZURUECK BEENDET NICHT SOFORT. Ein Druck auf die falsche Taste beim Laufen
  // duerfte sonst ein Training kosten. Erst in der Pause geht es hinaus.
  if (training_zustand() == LaufLaeuft) {
    training_pause_umschalten();
    layer_mark_dirty(s_flaeche);
    return;
  }
  const Trainingsstand t = training_stoppe();
  if (t.dauer_s >= 60) telefon_sende(&t);
  window_stack_pop(true);
}

static void prv_tasten(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_auswahl);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_zurueck);
}

// --- Fenster ---

static void prv_laden(Window *fenster) {
  Layer *wurzel = window_get_root_layer(fenster);
  const GRect bounds = layer_get_bounds(wurzel);

  s_kopf = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(14, 0), bounds.size.w, 20));
  text_layer_set_text(s_kopf, art_name(training_art()));
  text_layer_set_text_alignment(s_kopf, GTextAlignmentCenter);
  text_layer_set_font(s_kopf, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_background_color(s_kopf, GColorClear);
  layer_add_child(wurzel, text_layer_get_layer(s_kopf));

  s_flaeche = layer_create(GRect(0, PBL_IF_ROUND_ELSE(20, 18),
                                 bounds.size.w, bounds.size.h - 18));
  layer_set_update_proc(s_flaeche, prv_zeichne);
  layer_add_child(wurzel, s_flaeche);
}

static void prv_entladen(Window *fenster) {
  if (s_takt) { app_timer_cancel(s_takt); s_takt = NULL; }
  layer_destroy(s_flaeche);
  text_layer_destroy(s_kopf);
  window_destroy(s_fenster);
  s_fenster = NULL;
}

void lauf_window_zeige(Sportart art) {
  training_starte(art);
  s_letzte_zone = -1;

  s_fenster = window_create();
  window_set_background_color(s_fenster, GColorWhite);
  window_set_click_config_provider(s_fenster, prv_tasten);
  window_set_window_handlers(s_fenster, (WindowHandlers) {
    .load = prv_laden,
    .unload = prv_entladen,
  });
  window_stack_push(s_fenster, true);
  s_takt = app_timer_register(1000, prv_tick, NULL);
}
