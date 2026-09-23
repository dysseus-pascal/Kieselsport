#include "puls.h"

#include "schluessel.h"

#define MAXPULS_VORGABE 190

// Sekunden zwischen zwei Messungen während eines Trainings.
//
// EINE SEKUNDE WÄRE MÖGLICH UND FALSCH. Der Sensor misst optisch; jede
// Messung kostet Licht und damit Strom, und ein Puls ändert sich nicht im
// Sekundentakt. Fünf Sekunden sind dicht genug, um einen Zonenwechsel zu
// bemerken, bevor er vorbei ist.
#define TAKT_TRAINING_S 5

static uint16_t s_maximum = MAXPULS_VORGABE;

// Die Grenzen in Prozent des Maximums. Die übliche Fünferteilung; sie ist
// Konvention, keine Physiologie, aber jeder Trainingsplan spricht in ihr.
static const uint8_t s_prozent[KS_ZONEN] = { 50, 60, 70, 80, 90 };

void puls_init(void) {
  if (persist_exists(PERSIST_MAXPULS)) {
    const int gelesen = persist_read_int(PERSIST_MAXPULS);
    if (gelesen > 100 && gelesen < 240) s_maximum = (uint16_t)gelesen;
  }
}

uint16_t puls_maximum(void) {
  return s_maximum;
}

void puls_setze_maximum(uint16_t schlaege) {
  if (schlaege <= 100 || schlaege >= 240) return;
  s_maximum = schlaege;
  persist_write_int(PERSIST_MAXPULS, schlaege);
}

uint16_t puls_zonengrenze(int zone) {
  if (zone < 1 || zone > KS_ZONEN) return 0;
  return (uint16_t)((uint32_t)s_maximum * s_prozent[zone - 1] / 100);
}

int puls_zone(uint16_t bpm) {
  if (bpm == 0) return 0;
  for (int z = KS_ZONEN; z >= 1; z--) {
    if (bpm >= puls_zonengrenze(z)) return z;
  }
  return 0;
}

// --- Frisch oder alt ---

static time_t s_frisch_seit = 0;   // letzte Meldung des Sensors oder Wertwechsel
static uint16_t s_letzter = 0;

static void prv_ereignis(HealthEventType ereignis, void *context) {
  // Der Sensor meldet sich bei jeder neuen Messung - auch wenn der Wert
  // derselbe ist. Das ist das verlaesslichere Zeichen fuer "frisch".
  if (ereignis == HealthEventHeartRateUpdate) s_frisch_seit = time(NULL);
}

void puls_beobachten(void) {
  s_frisch_seit = 0;
  s_letzter = 0;
  health_service_events_subscribe(prv_ereignis, NULL);
}

void puls_ignorieren(void) {
  health_service_events_unsubscribe();
}

uint16_t puls_lesen(void) {
  // MIT &, NICHT MIT ==. Die Maske kann neben "verfuegbar" weitere Bits
  // tragen; ein strenger Vergleich hielte den Puls dann fuer nicht da.
  const time_t jetzt = time(NULL);
  if (!(health_service_metric_accessible(HealthMetricHeartRateBPM, jetzt, jetzt)
        & HealthServiceAccessibilityMaskAvailable)) {
    return 0;
  }
  const HealthValue wert = health_service_peek_current_value(HealthMetricHeartRateBPM);
  const uint16_t bpm = wert > 0 ? (uint16_t)wert : 0;
  // Ein Wert, der sich aendert, ist frisch - auch auf einer Firmware, die
  // das Ereignis nicht schickt. So faellt kein Puls weg, der da ist.
  if (bpm != s_letzter) {
    s_letzter = bpm;
    s_frisch_seit = jetzt;
  }
  return bpm;
}

bool puls_frisch(void) {
  if (s_letzter == 0 || s_frisch_seit == 0) return false;
  return (time(NULL) - s_frisch_seit) <= KS_PULS_ALT_S;
}

void puls_dicht_messen(void) {
  health_service_set_heart_rate_sample_period(TAKT_TRAINING_S);
}

void puls_normal_messen(void) {
  // NULL heisst "zurück zur Vorgabe des Systems". Das ist die Zeile, die man
  // vergisst - und dann misst die Uhr noch am nächsten Morgen im
  // Fünfsekundentakt, obwohl das Training seit zwölf Stunden vorbei ist.
  health_service_set_heart_rate_sample_period(0);
}
