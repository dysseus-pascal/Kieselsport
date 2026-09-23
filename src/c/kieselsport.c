#include <pebble.h>
#include "art.h"
#include "telefon.h"
#include "lauf_window.h"
#include "glanz.h"
#include "botschaft.h"

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

static uint16_t prv_zeilen(MenuLayer *liste, uint16_t abschnitt, void *ctx) {
  return ArtAnzahl;
}

static void prv_zeichne_zeile(GContext *ctx, const Layer *zelle,
                              MenuIndex *index, void *daten) {
  // DER OBERTITEL TRENNT, WAS ZUSAMMENGEHOERT: Strasse/Gravel und MTB
  // stehen im Menue nicht beieinander - die Zahl einer Art darf sich nie
  // verschieben, also kam MTB hinten dazu. Das Woertchen "Bike" darunter
  // sagt trotzdem, was beide sind.
  const Sportart art = (Sportart)index->row;
  menu_cell_basic_draw(ctx, zelle, art_name(art), art_gruppe(art), NULL);
}

static void prv_gewaehlt(MenuLayer *liste, MenuIndex *index, void *daten) {
  lauf_window_zeige((Sportart)index->row);
}

static void prv_laden(Window *fenster) {
  Layer *wurzel = window_get_root_layer(fenster);
  const GRect bounds = layer_get_bounds(wurzel);

  s_liste = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_liste, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_zeilen,
    .draw_row = prv_zeichne_zeile,
    .select_click = prv_gewaehlt,
  });
  menu_layer_set_click_config_onto_window(s_liste, fenster);
  layer_add_child(wurzel, menu_layer_get_layer(s_liste));
}

static void prv_entladen(Window *fenster) {
  menu_layer_destroy(s_liste);
}

static void prv_vom_worker(uint16_t typ, AppWorkerMessage *daten) {
  lauf_window_nachricht(typ, daten);
}

static void prv_init(void) {
  telefon_init();
  app_worker_message_subscribe(prv_vom_worker);

  s_menue = window_create();
  window_set_window_handlers(s_menue, (WindowHandlers) {
    .load = prv_laden,
    .unload = prv_entladen,
  });
  window_stack_push(s_menue, true);

  // LAEUFT SCHON ETWAS, GLEICH HINEIN. Wer die App aus dem Startmenue
  // oeffnet, weil dort "Laufen im Hintergrund" steht, will den Lauf sehen
  // und nicht das Menue.
  if (app_worker_is_running()) {
    lauf_window_zeige_laufend();
  }

#ifdef KS_DEMO
  // Gleich hinein: im Emulator laesst sich keine Taste druecken. Welche Art,
  // sagt KS_DEMO_ART - so laesst sich jeder Schirm ansehen, auch der von
  // Kraft und Schwimmen.
#ifndef KS_DEMO_ART
#define KS_DEMO_ART ArtLaufen
#endif
  if (!app_worker_is_running()) lauf_window_zeige(KS_DEMO_ART);
#endif
}

static void prv_ende(void) {
  // DER HINWEIS IM STARTMENUE. Laeuft ein Training weiter, steht es dort
  // unter Kieselsport - sonst wuesste man beim Blick auf die Uhr nicht, dass
  // im Hintergrund gezaehlt wird.
  // Ob etwas laeuft, weiss der Worker besser als der Schirm - der war
  // vielleicht nie offen, wenn jemand nur ins Menue schaute.
  glanz_setzen(app_worker_is_running() && lauf_window_laeuft(),
               lauf_window_art(), lauf_window_beginn());
  app_worker_message_unsubscribe();
  window_destroy(s_menue);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_ende();
}
