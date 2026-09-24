#include "einstellungen.h"
#include "schluessel.h"
#include "nacht.h"

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

void einstellungen_pin_art(int32_t art) {
  if (art < 0 || art > 7) return;
  persist_write_int(PERSIST_PIN_ART, art);
}

void einstellungen_pin_zeit(const char *hhmm) {
  if (!hhmm) return;
  // Nur "H:MM" oder "HH:MM" - was sonst kommt, ist keine Uhrzeit.
  const size_t n = strlen(hhmm);
  if (n < 4 || n > 5 || hhmm[n - 3] != ':') return;
  persist_write_string(PERSIST_PIN_ZEIT, hhmm);
}

void einstellungen_nacht_an(bool an) {
  persist_write_bool(PERSIST_NACHT_AN, an);
}

void einstellungen_nacht_von(int32_t minuten) {
  if (minuten < 0 || minuten >= 24 * 60) return;
  persist_write_int(PERSIST_NACHT_VON, minuten);
}

void einstellungen_nacht_bis(int32_t minuten) {
  if (minuten < 0 || minuten >= 24 * 60) return;
  persist_write_int(PERSIST_NACHT_BIS, minuten);
}

static int32_t prv_lies(uint32_t schluessel, int32_t vorgabe) {
  return persist_exists(schluessel) ? persist_read_int(schluessel) : vorgabe;
}

void einstellungen_melden(DictionaryIterator *out) {
  // Die Vorgaben sind die der Konfigseite und der Module, die sie lesen.
  dict_write_int32(out, MESSAGE_KEY_MAXPULS, prv_lies(PERSIST_MAXPULS, 190));
  dict_write_int32(out, MESSAGE_KEY_BECKEN, prv_lies(PERSIST_BECKEN, 25));
  dict_write_int32(out, MESSAGE_KEY_PAUSENZIEL, prv_lies(PERSIST_PAUSENZIEL, 90));
  dict_write_int32(out, MESSAGE_KEY_EMPFIND, prv_lies(PERSIST_EMPFIND, 2));
  dict_write_int32(out, MESSAGE_KEY_PIN_ART, prv_lies(PERSIST_PIN_ART, 0));
  char zeit[8] = "18:00";
  if (persist_exists(PERSIST_PIN_ZEIT)) persist_read_string(PERSIST_PIN_ZEIT, zeit, sizeof(zeit));
  dict_write_cstring(out, MESSAGE_KEY_PIN_ZEIT, zeit);
  dict_write_int32(out, MESSAGE_KEY_NACHT_AN, nacht_an() ? 1 : 0);
  dict_write_int32(out, MESSAGE_KEY_NACHT_VON, nacht_von());
  dict_write_int32(out, MESSAGE_KEY_NACHT_BIS, nacht_bis());
}
