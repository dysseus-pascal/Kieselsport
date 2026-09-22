#include "training.h"
#include "puls.h"

// Persist-Schluessel. 1 gehoert dem Maximalpuls (siehe puls.c).
#define PERSIST_ARCHIV_ANZAHL 10
#define PERSIST_ARCHIV_NAECHST 11
#define PERSIST_ARCHIV_BASIS 100

static Laufzustand s_zustand = LaufAus;
static Sportart s_art = ArtLaufen;
static uint32_t s_beginn = 0;
static uint32_t s_sekunden = 0;

// Die Staende der Uhr beim Start - die Summen sind die Differenz dazu.
static uint32_t s_schritte_anfang = 0;
static uint32_t s_meter_anfang = 0;
static uint32_t s_kcal_anfang = 0;

static uint32_t s_puls_summe = 0;
static uint32_t s_puls_messungen = 0;
static uint16_t s_puls_max = 0;

static uint32_t prv_metrik(HealthMetric m) {
  const time_t jetzt = time(NULL);
  const time_t heute = time_start_of_today();
  if (health_service_metric_accessible(m, heute, jetzt) != HealthServiceAccessibilityMaskAvailable) {
    return 0;
  }
  const HealthValue wert = health_service_sum_today(m);
  return wert > 0 ? (uint32_t)wert : 0;
}

uint16_t training_puls(void) {
#ifdef KS_DEMO
  // NUR IM PRUEFBAU. Der Emulator hat keinen Sensor; ohne erfundene Werte
  // laesst sich der laufende Schirm nie ansehen. Der Puls wandert langsam
  // durch die Zonen, damit auch der Zonenwechsel sichtbar wird.
  return (uint16_t)(105 + (s_sekunden % 90));
#else
  const time_t jetzt = time(NULL);
  if (health_service_metric_accessible(HealthMetricHeartRateBPM, jetzt, jetzt)
      != HealthServiceAccessibilityMaskAvailable) {
    return 0;
  }
  const HealthValue wert = health_service_peek_current_value(HealthMetricHeartRateBPM);
  return wert > 0 ? (uint16_t)wert : 0;
#endif
}

void training_init(void) {
  puls_init();
}

void training_beenden_ganz(void) {
  // SICHERHEITSNETZ BEIM BEENDEN DER APP. Wer sie aus dem laufenden Training
  // heraus verlaesst, laesst sonst die dichte Pulsmessung an.
  puls_normal_messen();
}

Laufzustand training_zustand(void) { return s_zustand; }
Sportart training_art(void) { return s_art; }

void training_starte(Sportart art) {
  s_art = art;
  s_zustand = LaufLaeuft;
  s_beginn = (uint32_t)time(NULL);
  s_sekunden = 0;
  s_schritte_anfang = prv_metrik(HealthMetricStepCount);
  s_meter_anfang = prv_metrik(HealthMetricWalkedDistanceMeters);
  s_kcal_anfang = prv_metrik(HealthMetricActiveKCalories);
  s_puls_summe = 0;
  s_puls_messungen = 0;
  s_puls_max = 0;
  puls_dicht_messen();
}

void training_pause_umschalten(void) {
  if (s_zustand == LaufLaeuft) {
    s_zustand = LaufPause;
    // In der Pause reicht der normale Takt - eine Pause ist kein Training.
    puls_normal_messen();
  } else if (s_zustand == LaufPause) {
    s_zustand = LaufLaeuft;
    puls_dicht_messen();
  }
}

void training_tick(void) {
  if (s_zustand != LaufLaeuft) return;
  s_sekunden++;

  const uint16_t puls = training_puls();
  if (puls > 0) {
    s_puls_summe += puls;
    s_puls_messungen++;
    if (puls > s_puls_max) s_puls_max = puls;
  }
}

Trainingsstand training_stand(void) {
  Trainingsstand t = {
    .art = (uint8_t)s_art,
    .beginn = s_beginn,
    .dauer_s = s_sekunden,
    .puls_max = s_puls_max,
    .puls_mittel = s_puls_messungen ? (uint16_t)(s_puls_summe / s_puls_messungen) : 0,
  };

  const ArtInfo *info = art_info(s_art);
#ifdef KS_DEMO
  t.schritte = info->schritte ? s_sekunden * 2 : 0;
  t.meter = info->distanz ? s_sekunden * 2 : 0;
  t.kcal = s_sekunden / 6;
  return t;
#endif
  if (info->schritte) {
    const uint32_t jetzt = prv_metrik(HealthMetricStepCount);
    t.schritte = jetzt > s_schritte_anfang ? jetzt - s_schritte_anfang : 0;
  }
  if (info->distanz) {
    const uint32_t jetzt = prv_metrik(HealthMetricWalkedDistanceMeters);
    t.meter = jetzt > s_meter_anfang ? jetzt - s_meter_anfang : 0;
  }
  // KALORIEN GELTEN FUER JEDE ART. Sie haengen am Puls und an der Bewegung,
  // nicht an Schritten - beim Krafttraining sind sie die einzige Zahl neben
  // Zeit und Puls, die etwas sagt.
  const uint32_t kcal = prv_metrik(HealthMetricActiveKCalories);
  t.kcal = kcal > s_kcal_anfang ? kcal - s_kcal_anfang : 0;
  return t;
}

// --- Archiv ---

static void prv_ins_archiv(const Trainingsstand *t) {
  int naechst = persist_exists(PERSIST_ARCHIV_NAECHST)
      ? persist_read_int(PERSIST_ARCHIV_NAECHST) : 0;
  int anzahl = persist_exists(PERSIST_ARCHIV_ANZAHL)
      ? persist_read_int(PERSIST_ARCHIV_ANZAHL) : 0;

  persist_write_data(PERSIST_ARCHIV_BASIS + naechst, t, sizeof(*t));
  naechst = (naechst + 1) % KS_ARCHIV_MAX;
  if (anzahl < KS_ARCHIV_MAX) anzahl++;

  persist_write_int(PERSIST_ARCHIV_NAECHST, naechst);
  persist_write_int(PERSIST_ARCHIV_ANZAHL, anzahl);
}

Trainingsstand training_stoppe(void) {
  const Trainingsstand t = training_stand();
  s_zustand = LaufAus;
  puls_normal_messen();
  // EIN TRAINING UNTER EINER MINUTE IST KEINES. Ein Fehlgriff im Menü soll
  // das Archiv nicht mit Eintraegen fuellen, die niemand gemacht hat.
  if (t.dauer_s >= 60) prv_ins_archiv(&t);
  return t;
}

int training_archiv_anzahl(void) {
  return persist_exists(PERSIST_ARCHIV_ANZAHL) ? persist_read_int(PERSIST_ARCHIV_ANZAHL) : 0;
}

bool training_archiv_lesen(int index, Trainingsstand *aus) {
  const int anzahl = training_archiv_anzahl();
  if (index < 0 || index >= anzahl) return false;
  const int naechst = persist_exists(PERSIST_ARCHIV_NAECHST)
      ? persist_read_int(PERSIST_ARCHIV_NAECHST) : 0;
  // Rueckwaerts vom zuletzt geschriebenen Platz: index 0 ist das juengste.
  int platz = (naechst - 1 - index + 2 * KS_ARCHIV_MAX) % KS_ARCHIV_MAX;
  if (!persist_exists(PERSIST_ARCHIV_BASIS + platz)) return false;
  persist_read_data(PERSIST_ARCHIV_BASIS + platz, aus, sizeof(*aus));
  return true;
}
