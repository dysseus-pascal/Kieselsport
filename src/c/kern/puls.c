#include "puls.h"

#include "schluessel.h"

#define MAXPULS_VORGABE 190

// Sekunden zwischen zwei Messungen während eines Trainings.
//
// EINE SEKUNDE, ALSO DAUERND. Der erste Entwurf nahm fünf: der Sensor misst
// optisch, jede Messung kostet Licht und damit Strom. Aber mit fünf Sekunden
// zwischen den Messungen liess der Sensor am Lenker und am Hang den Wert
// stehen, und die Zone stimmte nie. Eine Sekunde ist der Takt, in dem die
// eingebaute Workout-App misst - und ein Training ist die eine Stunde am
// Tag, in der der Akku dafür da ist.
#define TAKT_TRAINING_S 1
#define TAKT_SPARSAM_S 5

static uint16_t s_maximum = MAXPULS_VORGABE;
static bool s_sparsam = false;

void puls_setze_sparsam(bool sparsam) {
  if (sparsam == s_sparsam) return;
  s_sparsam = sparsam;
}

bool puls_sparsam(void) { return s_sparsam; }

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

// --- HRV ---
//
// RMSSD = Wurzel aus dem Mittel der quadrierten Differenzen aufeinander
// folgender Intervalle. Gezaehlt wird laufend: Summe und Anzahl, die Wurzel
// erst am Ende. Ein Ausreisser - ein Intervall, das um mehr als ein Drittel
// vom vorigen abweicht - ist ein verpasster Schlag, kein Herz, und faellt
// weg; sonst zoege ein einziger 1800er den Wert ins Absurde.
static bool s_hrv_an = false;
static uint16_t s_hrv_vorher = 0;
static uint32_t s_hrv_summe = 0;   // Summe der quadrierten Differenzen
static uint16_t s_hrv_anzahl = 0;

static void prv_hrv_intervall(uint16_t ppi) {
  if (ppi < 300 || ppi > 2000) return;
  if (s_hrv_vorher != 0) {
    const int d = (int)ppi - (int)s_hrv_vorher;
    const int grenze = (int)s_hrv_vorher / 3;
    if (d > -grenze && d < grenze) {
      if (s_hrv_summe < 0xF0000000u) {
        s_hrv_summe += (uint32_t)(d * d);
        if (s_hrv_anzahl < 0xFFFF) s_hrv_anzahl++;
      }
    }
  }
  s_hrv_vorher = ppi;
}

static uint32_t prv_wurzel(uint32_t x) {
  uint32_t r = 0, bit = 1u << 30;
  while (bit > x) bit >>= 2;
  while (bit) {
    if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
    bit >>= 2;
  }
  return r;
}

void puls_hrv_start(void) {
  s_hrv_vorher = 0;
  s_hrv_summe = 0;
  s_hrv_anzahl = 0;
  s_hrv_an = health_service_set_hrv_sample_period(1);
}

void puls_hrv_stop(void) {
  if (s_hrv_an) health_service_set_hrv_sample_period(0);
  s_hrv_an = false;
}

uint16_t puls_hrv_rmssd(void) {
  // Unter dreissig Intervallen ist es Rauschen, keine Messung.
  if (s_hrv_anzahl < 30) return 0;
  return (uint16_t)prv_wurzel(s_hrv_summe / s_hrv_anzahl);
}

uint16_t puls_hrv_anzahl(void) { return s_hrv_anzahl; }

static void prv_ereignis(HealthEventType ereignis, void *context) {
  // Der Sensor meldet sich bei jeder neuen Messung - auch wenn der Wert
  // derselbe ist. Das ist das verlaesslichere Zeichen fuer "frisch".
  if (ereignis == HealthEventHeartRateUpdate) s_frisch_seit = time(NULL);
  if (ereignis == HealthEventHRVUpdate && s_hrv_an) {
    const uint16_t ppi = health_service_peek_hrv_ppi_ms();
    if (ppi) prv_hrv_intervall(ppi);
  }
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
  health_service_set_heart_rate_sample_period(s_sparsam ? TAKT_SPARSAM_S : TAKT_TRAINING_S);
}

void puls_normal_messen(void) {
  // NULL heisst "zurück zur Vorgabe des Systems". Das ist die Zeile, die man
  // vergisst - und dann misst die Uhr noch am nächsten Morgen im
  // Sekundentakt, obwohl das Training seit zwölf Stunden vorbei ist.
  health_service_set_heart_rate_sample_period(0);
}
