#include <pebble_worker.h>
#include "training.h"
#include "puls.h"
#include "reps.h"
#include "bahnen.h"
#include "abschnitt.h"
#include "wartend.h"
#include "botschaft.h"
#include "schluessel.h"

// Der Worker: das Training laeuft hier, nicht in der App.
//
// WARUM: die App ist weg, sobald man mit Zurueck aufs Zifferblatt geht -
// und mit ihr waere das Training weg. Ein Hintergrund-Worker bleibt. Er
// zaehlt die Sekunden, liest den Puls, zaehlt Saetze und Bahnen und brummt
// beim Zonenwechsel, ob die App offen ist oder nicht. Die App ist nur noch
// die Anzeige und die Tasten.
//
// WAS ER NICHT KANN: mit dem Telefon reden. AppMessage gibt es im Worker
// nicht. Deshalb schreibt er die fertige Zusammenfassung in den Persist
// (wartend.h) und sagt der App Bescheid; die schickt sie.
//
// ER SENDET NUR, WENN JEMAND ZUSCHAUT. Die App erneuert ihr Abo alle paar
// Sekunden; bleibt es aus, hoert der Worker auf, jede Sekunde vier
// Nachrichten in die Leere zu schicken.

static int s_letzte_zone = -1;
static uint8_t s_abo_alter_s = 255;
// Was die App brummen soll, und seit wann es faellig ist. Bleibt stehen,
// bis eine zuschauende App es bekommen hat - hoechstens ein paar Sekunden,
// sonst brummte es fuer etwas, das laengst vorbei ist.
static uint8_t s_brumm = BrummNichts;
static time_t s_brumm_seit;
#define BRUMM_FRIST_S 6

static void prv_stand_senden(void) {
  const Trainingsstand t = training_stand();
  const uint16_t puls = training_puls();
  const bool frisch = training_puls_frisch();
  const int zone = frisch ? puls_zone(puls) : 0;
  AppWorkerMessage m;

  m.data0 = (uint16_t)(training_zustand() | (training_art() << 4));
  m.data1 = bot_kappe(t.dauer_s);
  m.data2 = (uint16_t)(puls | (frisch ? 0x8000 : 0));
  app_worker_send_message(BotStand1, &m);

  m.data0 = bot_kappe(t.schritte);
  m.data1 = bot_kappe(t.meter / 10);
  m.data2 = bot_kappe(t.kcal);
  app_worker_send_message(BotStand2, &m);

  m.data0 = t.saetze;
  m.data1 = t.reps;
  m.data2 = (uint16_t)(reps_laufend() | (reps_ruht() ? 0x8000 : 0));
  app_worker_send_message(BotStand3, &m);

  uint8_t brumm = BrummNichts;
  if (s_brumm != BrummNichts) {
    if (time(NULL) - s_brumm_seit <= BRUMM_FRIST_S) brumm = s_brumm;
    // Nur eine zuschauende App bekommt es - und dann genau einmal.
    if (s_abo_alter_s <= KS_ABO_S) s_brumm = BrummNichts;
  }
  m.data0 = reps_ruhe_s();
  m.data1 = t.bahnen;
  m.data2 = (uint16_t)((bahnen_bereit() ? 1 : 0) | (brumm << 4) | (zone << 8));
  app_worker_send_message(BotStand4, &m);

  m.data0 = (uint16_t)(t.beginn >> 16);
  m.data1 = (uint16_t)(t.beginn & 0xFFFF);
  m.data2 = puls_maximum();
  app_worker_send_message(BotStand5, &m);
}

static void prv_brumm_vormerken(uint8_t was) {
  s_brumm = was;
  s_brumm_seit = time(NULL);
  if (s_abo_alter_s > KS_ABO_S) {
    // NIEMAND SCHAUT ZU - die App ist zu. Brummen kann nur sie, also kommt
    // sie nach vorn: das Zifferblatt weicht dem Training. Das ist der
    // Preis dafuer, dass der Zonenwechsel auch im Hintergrund etwas sagt.
    worker_launch_app();
  } else {
    prv_stand_senden();
  }
}

static void prv_zonenwechsel(void) {
  // Das ist die eine Stelle, an der die Uhr von sich aus etwas sagt - und
  // der Grund, warum man sie beim Sport ueberhaupt anschaut. Nur ein
  // frischer Wert darf brummen: ein alter wechselt keine Zone.
  const int zone = training_puls_frisch() ? puls_zone(training_puls()) : 0;
  if (training_zustand() == LaufLaeuft && zone > 0 && s_letzte_zone > 0 && zone != s_letzte_zone) {
    prv_brumm_vormerken(zone > s_letzte_zone ? BrummZoneHoch : BrummZoneRunter);
  }
  if (zone > 0) s_letzte_zone = zone;
  if (reps_brumm_holen()) prv_brumm_vormerken(BrummPauseUm);
}

static void prv_tick(struct tm *zeit, TimeUnits einheiten) {
  if (training_zustand() == LaufAus) return;
  training_tick();
  prv_zonenwechsel();
  if (s_abo_alter_s < 255) s_abo_alter_s++;
  if (s_abo_alter_s <= KS_ABO_S) prv_stand_senden();
}

static void prv_starten_nach_bestellung(void) {
  if (!persist_exists(PERSIST_START_ART)) return;
  const Sportart art = (Sportart)persist_read_int(PERSIST_START_ART);
  const uint32_t beginn = persist_exists(PERSIST_START_BEGINN)
      ? (uint32_t)persist_read_int(PERSIST_START_BEGINN) : 0;
  persist_delete(PERSIST_START_ART);
  persist_delete(PERSIST_START_BEGINN);
  s_letzte_zone = -1;
  training_starte_ab(art, beginn);
  APP_LOG(APP_LOG_LEVEL_INFO, "Training gestartet: Art %d", (int)art);
}

static void prv_speichern(void) {
  const Trainingsstand t = training_stoppe();
  AppWorkerMessage leer = { 0, 0, 0 };
  // EIN TRAINING UNTER EINER MINUTE IST KEINES - es geht nicht ans Telefon.
  if (t.dauer_s < 60) {
    abschnitt_leeren();
    app_worker_send_message(BotVerworfen, &leer);
    return;
  }
  // Nicht auf dem Stapel: der ist im Worker klein.
  static char liste[KS_LISTE_MAX];
  liste[0] = 0;
  if (abschnitt_anzahl() > 0) abschnitt_als_text(liste, sizeof(liste));
  wartend_merken(&t, liste);
  abschnitt_leeren();
  app_worker_send_message(BotFertig, &leer);
}

static void prv_befehl(uint16_t typ, AppWorkerMessage *daten) {
  AppWorkerMessage leer = { 0, 0, 0 };
  switch (typ) {
    case BefehlAbo:
      s_abo_alter_s = 0;
      prv_stand_senden();
      break;
    case BefehlStart:
      // Der Worker lief schon, als die App starten wollte - also hat ihn
      // niemand neu hochgefahren, und die Bestellung liegt noch im Persist.
      prv_starten_nach_bestellung();
      s_abo_alter_s = 0;
      prv_stand_senden();
      break;
    case BefehlPause:
      training_pause_umschalten();
      prv_stand_senden();
      break;
    case BefehlSpeichern:
      prv_speichern();
      break;
    case BefehlVerwerfen:
      training_verwerfen();
      app_worker_send_message(BotVerworfen, &leer);
      break;
    default:
      break;
  }
}

static void prv_init(void) {
  training_init();
  app_worker_message_subscribe(prv_befehl);
  prv_starten_nach_bestellung();
  tick_timer_service_subscribe(SECOND_UNIT, prv_tick);
}

static void prv_ende(void) {
  tick_timer_service_unsubscribe();
  app_worker_message_unsubscribe();
  // Wird der Worker beendet, waehrend ein Training laeuft, bleibt sonst die
  // dichte Pulsmessung an - die teuerste Falle dieser App.
  puls_normal_messen();
  puls_ignorieren();
}

int main(void) {
  prv_init();
  worker_event_loop();
  prv_ende();
}
