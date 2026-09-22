#include "puls.h"

#define PERSIST_MAXPULS 1
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

GColor puls_zonenfarbe(int zone) {
#if defined(PBL_COLOR)
  switch (zone) {
    case 1: return GColorPictonBlue;
    case 2: return GColorJaegerGreen;
    case 3: return GColorLimerick;
    case 4: return GColorChromeYellow;
    case 5: return GColorFolly;
    default: return GColorLightGray;
  }
#else
  // SCHWARZWEISS: eine Farbe, die es nicht gibt, ist keine Auskunft. Auf
  // flint trägt die Ziffer neben dem Puls die Zone, und die Balkenlänge
  // darunter zeigt sie noch einmal.
  return GColorBlack;
#endif
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
