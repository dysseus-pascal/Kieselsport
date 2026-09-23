#include "einstellungen.h"
#include "schluessel.h"

// Dieselben Grenzen wie in puls.c, reps.c und bahnen.c: was dort beim Lesen
// verworfen wuerde, soll hier gar nicht erst hinein.

void einstellungen_maxpuls(int32_t schlaege) {
  if (schlaege <= 100 || schlaege >= 240) return;
  persist_write_int(PERSIST_MAXPULS, schlaege);
}

void einstellungen_becken(int32_t meter) {
  if (meter < 5 || meter > 100) return;
  persist_write_int(PERSIST_BECKEN, meter);
}

void einstellungen_pausenziel(int32_t sekunden) {
  if (sekunden < 0 || sekunden > 3600) return;
  persist_write_int(PERSIST_PAUSENZIEL, sekunden);
}

void einstellungen_empfindlichkeit(int32_t stufe) {
  if (stufe < 1 || stufe > 3) return;
  persist_write_int(PERSIST_EMPFIND, stufe);
}
