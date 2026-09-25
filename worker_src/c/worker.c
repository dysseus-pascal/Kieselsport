#include <pebble_worker.h>
#include "training.h"
#include "puls.h"
#include "reps.h"
#include "bahnen.h"
#include "abschnitt.h"
#include "wartend.h"
#include "kurve.h"
#include "botschaft.h"
#include "schluessel.h"
#include "nacht.h"

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
// ER LAEUFT DAUERND, seit er auch die Nacht misst (nacht.h). Ohne Training
// tickt er nur jede Minute; das kostet so gut wie nichts.
//
// ER SENDET NUR, WENN JEMAND ZUSCHAUT. Die App erneuert ihr Abo alle paar
// Sekunden; bleibt es aus, hoert der Worker auf, jede Sekunde vier
// Nachrichten in die Leere zu schicken.

static int s_letzte_zone = -1;

// DIE PULSKURVE GEHT UEBER DATA LOGGING ANS TELEFON - der eine Weg, den ein
// Worker dorthin hat. Jede Sekunde ein Satz: Beginn des Trainings (damit das
// Telefon weiss, wozu er gehoert), Sekunde, Puls (0 = kein frischer Wert).
// Die Uhr sammelt, das Telefon holt ab, sobald es erreichbar ist - auch
// Stunden spaeter. Kiesel-Helper macht daraus die Kurve in der Akte.
#define LOG_TAG_PULS 1
typedef struct __attribute__((packed)) {
  uint32_t beginn;
  uint16_t sekunde;
  uint16_t puls;
} Pulssatz;
static DataLoggingSessionRef s_log;

static void prv_log_start(void) {
  if (s_log) return;
  s_log = data_logging_create(LOG_TAG_PULS, DATA_LOGGING_BYTE_ARRAY, sizeof(Pulssatz), true);
}

static void prv_log_stop(void) {
  if (!s_log) return;
  data_logging_finish(s_log);
  s_log = NULL;
}

static void prv_log_puls(void) {
  if (!s_log) return;
  const Trainingsstand t = training_stand();
  Pulssatz satz = {
    .beginn = t.beginn,
    .sekunde = (uint16_t)t.dauer_s,
    .puls = training_puls_frisch() ? training_puls() : 0,
  };
  data_logging_log(s_log, &satz, 1);
}

// AKKU: unter KS_AKKU_SPARSAM Prozent misst der Puls seltener. Beim Start
// und jede Minute nachgesehen, nicht jede Sekunde - der Stand aendert sich
// nicht schneller.
static void prv_akku_pruefen(void) {
  const BatteryChargeState b = battery_state_service_peek();
  const bool sparsam = !b.is_plugged && b.charge_percent < KS_AKKU_SPARSAM;
  if (sparsam != puls_sparsam()) {
    puls_setze_sparsam(sparsam);
    if (training_zustand() == LaufLaeuft) puls_dicht_messen();
  }
}
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

  if (training_art() == ArtYoga) {
    m.data0 = t.hrv_ms;
    m.data1 = puls_hrv_anzahl();
    m.data2 = 0;
  } else {
    m.data0 = t.saetze;
    m.data1 = t.reps;
    m.data2 = (uint16_t)(reps_laufend() | (reps_ruht() ? 0x8000 : 0));
  }
  app_worker_send_message(BotStand3, &m);

  uint8_t brumm = BrummNichts;
  if (s_brumm != BrummNichts) {
    if (time(NULL) - s_brumm_seit <= BRUMM_FRIST_S) brumm = s_brumm;
    // Nur eine zuschauende App bekommt es - und dann genau einmal.
    if (s_abo_alter_s <= KS_ABO_S) s_brumm = BrummNichts;
  }
  m.data0 = reps_ruhe_s();
  m.data1 = t.bahnen;
  m.data2 = (uint16_t)((bahnen_bereit() ? 1 : 0) | (puls_sparsam() ? 2 : 0) | (brumm << 4) | (zone << 8));
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

static void prv_tick(struct tm *zeit, TimeUnits einheiten);

// Im Training jede Sekunde, sonst jede Minute.
static bool s_sekundentakt;
static void prv_takt(bool sekunde) {
  if (sekunde == s_sekundentakt) return;
  s_sekundentakt = sekunde;
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(sekunde ? SECOND_UNIT : MINUTE_UNIT, prv_tick);
}

static void prv_laeuft(bool ja) {
  // Fuer die App: laeuft ein Training? Der Worker selbst laeuft ja immer.
  persist_write_bool(PERSIST_LAEUFT, ja);
  prv_takt(ja);
}

// EIN GESPEICHERTES TRAINING, DAS NIE ANKAM. Die App schickt die
// Zusammenfassung und wartet zwanzig Sekunden auf das Telefon; war es in
// der Zeit nicht zu erreichen, geht sie zu, und das Training lag im
// Persist, bis jemand die App wieder oeffnete - am 25.9. ein Krafttraining,
// das im Verlauf bei "pausiert" stehenblieb. Jetzt holt der Worker die App
// selbst: nach 2, 10 und 30 Minuten, dann nicht mehr - ist das Telefon so
// lange weg, geht es beim naechsten Oeffnen.
static const uint8_t NACHSENDEN_MIN[] = { 2, 10, 30 };
static uint8_t s_nachsenden_versuch;
static uint16_t s_nachsenden_minuten;

static bool prv_nachsenden(void) {
  if (!persist_exists(PERSIST_WARTET)) {
    s_nachsenden_versuch = 0;
    s_nachsenden_minuten = 0;
    return false;
  }
  if (s_nachsenden_versuch >= ARRAY_LENGTH(NACHSENDEN_MIN)) return false;
  s_nachsenden_minuten++;
  if (s_nachsenden_minuten < NACHSENDEN_MIN[s_nachsenden_versuch]) return false;
  s_nachsenden_versuch++;
  return true;
}

static void prv_tick(struct tm *zeit, TimeUnits einheiten) {
  if (training_zustand() == LaufAus) {
    if (zeit->tm_sec == 0 && prv_nachsenden()) {
      worker_launch_app();
      return;
    }
    // DIE NACHT - nur ohne Training, und nur zur vollen Minute. Ist sie eben
    // fertig geworden, kommt die App nach vorn und schickt sie ans Telefon;
    // der Worker kann es nicht (siehe oben).
    if (zeit->tm_sec == 0 && nacht_minute(time(NULL))) worker_launch_app();
    return;
  }
  training_tick();
  if (training_zustand() == LaufLaeuft) {
    prv_log_puls();
    kurve_tick(training_stand().dauer_s, training_puls_frisch() ? training_puls() : 0);
    if (zeit->tm_sec == 0) prv_akku_pruefen();
  }
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
  // Ein Training schlaegt die Nacht: ein laufendes HRV-Fenster endet hier.
  nacht_unterbrechen();
  prv_akku_pruefen();
  training_starte_ab(art, beginn);
  prv_laeuft(true);
  kurve_start();
  prv_log_start();
  APP_LOG(APP_LOG_LEVEL_INFO, "Training gestartet: Art %d", (int)art);
}

static void prv_speichern(void) {
  prv_log_stop();
  const Trainingsstand t = training_stoppe();
  prv_laeuft(false);
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
  s_nachsenden_versuch = 0;
  s_nachsenden_minuten = 0;
  // Die Kurve dazu - die App schickt sie, sobald die Zusammenfassung
  // drueben ist.
  kurve_merken(t.beginn);
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
      prv_log_stop();
      training_verwerfen();
      prv_laeuft(false);
      app_worker_send_message(BotVerworfen, &leer);
      break;
    default:
      break;
  }
}

static void prv_init(void) {
  training_init();
  app_worker_message_subscribe(prv_befehl);
  // Ohne Bestellung laeuft kein Training - auch wenn der Persist nach einem
  // Absturz noch etwas anderes behauptet.
  persist_write_bool(PERSIST_LAEUFT, false);
  s_sekundentakt = false;
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick);
  prv_starten_nach_bestellung();
}

static void prv_ende(void) {
  prv_log_stop();
  nacht_unterbrechen();
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
