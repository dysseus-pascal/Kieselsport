#include <pebble.h>
#include "art.h"
#include "training.h"
#include "puls.h"
#include "telefon.h"
#include "lauf_window.h"

// Kieselsport - Training auf der Uhr, Auswertung im eigenen Haus.
//
// WARUM ES DIESE APP GIBT, obwohl es fertige Trainings-Apps für die Pebble
// gibt: die fertigen tragen ihre Daten in ihre eigene Welt. Diese schickt eine
// Zusammenfassung ans Telefon, wo Kiesel-Helper sie in die Gesundheitsakte
// einträgt - und damit steht ein Lauf im selben Wochenprofil wie Schlaf,
// Ruhepuls und Wasser. Das ist der ganze Unterschied, und es ist der Grund.
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
  menu_cell_basic_draw(ctx, zelle, art_name((Sportart)index->row), NULL, NULL);
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

static void prv_init(void) {
  training_init();
  telefon_init();

  s_menue = window_create();
  window_set_window_handlers(s_menue, (WindowHandlers) {
    .load = prv_laden,
    .unload = prv_entladen,
  });
  window_stack_push(s_menue, true);

#ifdef KS_DEMO
  // Gleich hinein: im Emulator laesst sich keine Taste druecken.
  lauf_window_zeige(ArtLaufen);
#endif
}

static void prv_ende(void) {
  window_destroy(s_menue);
  // DIE LETZTE ZEILE IST DIE WICHTIGSTE. Verlässt jemand die App aus einem
  // laufenden Training heraus, bliebe die dichte Pulsmessung sonst an - und
  // zwar über das Ende der App hinaus, so steht es in der Dokumentation.
  training_beenden_ganz();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_ende();
}
