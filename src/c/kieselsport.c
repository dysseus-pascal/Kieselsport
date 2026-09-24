#include <pebble.h>
#include "art.h"
#include "telefon.h"
#include "lauf_window.h"
#include "glanz.h"
#include "botschaft.h"
#include "thema.h"
#include "symbole.h"
#include "hrv_window.h"
#include "morgen_window.h"
#include "schluessel.h"
#include "strings.h"
#include "strings.h"

// Kieselsport - Training auf der Uhr, Auswertung im eigenen Haus.
//
// WARUM ES DIESE APP GIBT, obwohl es fertige Trainings-Apps für die Pebble
// gibt: die fertigen tragen ihre Daten in ihre eigene Welt. Diese schickt eine
// Zusammenfassung ans Telefon, wo Kiesel-Helper sie in die Gesundheitsakte
// einträgt - und damit steht ein Lauf im selben Wochenprofil wie Schlaf,
// Ruhepuls und Wasser. Das ist der ganze Unterschied, und es ist der Grund.
//
// ZWEI TEILE: der Worker misst (worker_src), die App zeigt und bedient. So
// laeuft ein Training weiter, wenn man mit Zurueck aufs Zifferblatt geht.
//
// Was sie NICHT tut: Karten, Routen, Höhenprofile. Die Uhr hat kein GPS; alles
// davon käme ohnehin vom Telefon.

static Window *s_menue;
static MenuLayer *s_liste;
static Layer *s_leiste;

// Die Arten, und darunter die HRV-Messung - was Herzintervall war.
#define KS_ZEILE_HRV ArtAnzahl

static uint16_t prv_zeilen(MenuLayer *liste, uint16_t abschnitt, void *ctx) {
  return ArtAnzahl + 1;
}

static int16_t prv_zeilenhoehe(MenuLayer *liste, MenuIndex *index, void *ctx) {
  return PBL_IF_ROUND_ELSE(56, KS_BREIT ? 52 : 44);
}

static void prv_zeichne_zeile(GContext *ctx, const Layer *zelle,
                              MenuIndex *index, void *daten) {
  // VOR JEDER ART IHR BILD, wie Run, Walk und Workout in der Workout-App.
  // Das Bild liest man schneller als das Wort - und es unterscheidet
  // Strasse/Gravel von MTB, die beide "Bike" heissen.
  //
  // DER OBERTITEL TRENNT, WAS ZUSAMMENGEHOERT: Strasse/Gravel und MTB
  // stehen im Menue nicht beieinander - die Zahl einer Art darf sich nie
  // verschieben, also kam MTB hinten dazu. Das Woertchen "Bike" darunter
  // sagt trotzdem, was beide sind.
  const Sportart art = (Sportart)index->row;
  const bool hrv = index->row == KS_ZEILE_HRV;
  const GRect b = layer_get_bounds(zelle);
  const bool hell = menu_cell_layer_is_highlighted(zelle);
  const GColor farbe = hell ? KS_FARBE_AUF_LEISTE : KS_FARBE_TEXT;
  const int16_t g = KS_BREIT ? 32 : 26;
  const int16_t links = PBL_IF_ROUND_ELSE(30, 6);
  if (hrv) symbol_herz(ctx, GPoint(links + g / 2, b.size.h / 2), g, farbe);
  else symbol_sport(ctx, art, GPoint(links + g / 2, b.size.h / 2), g, farbe);

  const int16_t tx = links + g + 8;
  const char *gruppe = hrv ? S(STR_HRV_UNTER) : art_gruppe(art);
  graphics_context_set_text_color(ctx, farbe);
  const GFont name = fonts_get_system_font(KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  const int16_t nh = KS_BREIT ? 28 : 22;
  const int16_t ny = gruppe ? b.size.h / 2 - nh + 4 : (b.size.h - nh) / 2 - 3;
  graphics_draw_text(ctx, hrv ? "HRV" : art_name(art), name, GRect(tx, ny, b.size.w - tx - 2, nh),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (gruppe) {
    graphics_draw_text(ctx, gruppe, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(tx, b.size.h / 2 + 2, b.size.w - tx - 2, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void prv_gewaehlt(MenuLayer *liste, MenuIndex *index, void *daten) {
  if (index->row == KS_ZEILE_HRV) hrv_window_zeige();
  else lauf_window_zeige((Sportart)index->row);
}

static void prv_zeichne_leiste(Layer *layer, GContext *ctx) {
  // Im Menue ist das Herz hohl: gemessen wird erst im Training. Select
  // waehlt - Oben und Unten blaettern, das sagt die Liste selbst.
  thema_leiste(ctx, layer_get_bounds(layer), false, GColorWhite, SymbolKeins, SymbolWahl, SymbolKeins);
}

static void prv_laden(Window *fenster) {
  Layer *wurzel = window_get_root_layer(fenster);
  const GRect bounds = layer_get_bounds(wurzel);

  // DIE LISTE LAESST DER LEISTE PLATZ - dasselbe Bild wie der laufende
  // Schirm, damit der Wechsel dorthin kein Sprung ist. Die gewaehlte Zeile
  // traegt die Leistenfarbe.
  s_liste = menu_layer_create(GRect(0, 0, bounds.size.w - KS_LEISTE_B, bounds.size.h));
  menu_layer_set_callbacks(s_liste, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_zeilen,
    .get_cell_height = prv_zeilenhoehe,
    .draw_row = prv_zeichne_zeile,
    .select_click = prv_gewaehlt,
  });
  menu_layer_set_normal_colors(s_liste, KS_FARBE_GRUND, KS_FARBE_TEXT);
  menu_layer_set_highlight_colors(s_liste, KS_FARBE_LEISTE, KS_FARBE_AUF_LEISTE);
  menu_layer_set_click_config_onto_window(s_liste, fenster);
  layer_add_child(wurzel, menu_layer_get_layer(s_liste));

  s_leiste = layer_create(bounds);
  layer_set_update_proc(s_leiste, prv_zeichne_leiste);
  layer_add_child(wurzel, s_leiste);
}

static void prv_entladen(Window *fenster) {
  layer_destroy(s_leiste);
  menu_layer_destroy(s_liste);
}

static void prv_vom_worker(uint16_t typ, AppWorkerMessage *daten) {
  lauf_window_nachricht(typ, daten);
}

static void prv_init(void) {
  // Die Sprache der Uhr einmal lesen, bevor irgendein Text gezeichnet wird.
  // Auch der Glance beim Beenden nimmt sie von hier.
  strings_refresh();
  telefon_init();
  app_worker_message_subscribe(prv_vom_worker);

  s_menue = window_create();
  window_set_background_color(s_menue, KS_FARBE_GRUND);
  window_set_window_handlers(s_menue, (WindowHandlers) {
    .load = prv_laden,
    .unload = prv_entladen,
  });
  window_stack_push(s_menue, true);

  // DER WORKER LAEUFT IMMER - er misst auch die Nacht (nacht.h). Ist er aus,
  // etwa nach dem Installieren, faehrt ihn jedes Oeffnen der App wieder hoch.
  if (!app_worker_is_running()) app_worker_launch();

  // LAEUFT SCHON ETWAS, GLEICH HINEIN. Wer die App aus dem Startmenue
  // oeffnet, weil dort "Laufen im Hintergrund" steht, will den Lauf sehen
  // und nicht das Menue. Ob ein Training laeuft, sagt der Persist: der
  // Worker allein sagt es nicht mehr.
  const bool laeuft = persist_exists(PERSIST_LAEUFT) && persist_read_bool(PERSIST_LAEUFT);
  if (laeuft) {
    lauf_window_zeige_laufend();
  } else if (launch_reason() == APP_LAUNCH_WORKER && telefon_nacht_wartet()) {
    // AM MORGEN: der Worker hat die App geholt, weil eine Nacht fertig ist.
    morgen_window_zeige();
  } else if (launch_reason() == APP_LAUNCH_TIMELINE_ACTION) {
    // AUS DEM PIN "TRAINING": die Aktion "Jetzt starten" traegt die Art als
    // Launch-Code (Art + 1, damit 0 "nichts" bleibt). Der Schirm steht dann
    // bereit; Select startet - wie beim Weg ueber das Menue.
    const int32_t code = launch_get_args();
    if (code >= 1 && code <= ArtAnzahl) lauf_window_zeige((Sportart)(code - 1));
  }

#ifdef KS_DEMO
  // Gleich hinein: im Emulator laesst sich keine Taste druecken. Welche Art,
  // sagt KS_DEMO_ART - so laesst sich jeder Schirm ansehen, auch der von
  // Kraft und Schwimmen.
#ifndef KS_DEMO_ART
#define KS_DEMO_ART ArtLaufen
#endif
  if (!laeuft) lauf_window_zeige(KS_DEMO_ART);
#endif
}

static void prv_ende(void) {
  // DER HINWEIS IM STARTMENUE. Laeuft ein Training weiter, steht es dort
  // unter Kieselsport - sonst wuesste man beim Blick auf die Uhr nicht, dass
  // im Hintergrund gezaehlt wird.
  // Ob etwas laeuft, weiss der Worker besser als der Schirm - der war
  // vielleicht nie offen, wenn jemand nur ins Menue schaute.
  glanz_setzen(persist_exists(PERSIST_LAEUFT) && persist_read_bool(PERSIST_LAEUFT) && lauf_window_laeuft(),
               lauf_window_art(), lauf_window_beginn());
  app_worker_message_unsubscribe();
  window_destroy(s_menue);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_ende();
}
