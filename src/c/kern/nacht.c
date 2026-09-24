#include "nacht.h"
#include "schluessel.h"
#include "puls.h"

// --- Einstellungen, fuer beide ---

bool nacht_an(void) {
  // Vorgabe: an. Wer Kieselsport hat, will seinen Schlaf - Herzintervall hat
  // auch nicht gefragt.
  return persist_exists(PERSIST_NACHT_AN) ? persist_read_bool(PERSIST_NACHT_AN) : true;
}

static int prv_minute(uint32_t schluessel, int vorgabe) {
  if (!persist_exists(schluessel)) return vorgabe;
  const int m = persist_read_int(schluessel);
  return (m >= 0 && m < 24 * 60) ? m : vorgabe;
}

int nacht_von(void) { return prv_minute(PERSIST_NACHT_VON, KS_NACHT_VON_VORGABE); }
int nacht_bis(void) { return prv_minute(PERSIST_NACHT_BIS, KS_NACHT_BIS_VORGABE); }

bool nacht_im_fenster(int m) {
  const int von = nacht_von(), bis = nacht_bis();
  if (von == bis) return false;
  // UEBER MITTERNACHT ist der Normalfall: 22:00 bis 08:00 heisst "ab 22 oder
  // vor 8", nicht "zwischen 8 und 22".
  return von < bis ? (m >= von && m < bis) : (m >= von || m < bis);
}

#ifdef KS_WORKER
// --- Worker: messen ---

static time_t s_beginn;          //< 0 = keine Nacht im Lauf
static NachtFenster s_fenster[KS_NACHT_FENSTER_MAX];
static uint8_t s_fenster_n;
static bool s_misst;
static uint8_t s_rest_min;
static uint16_t s_mess_minute;
static time_t s_mess_seit;

static void prv_wiederherstellen(void) {
  // NACH EINEM NEUSTART DES WORKERS - Akkuwechsel, Update, ein Training
  // dazwischen - geht die Nacht weiter, statt neu zu beginnen.
  if (s_beginn || !persist_exists(PERSIST_NACHT_LAUF_BEGINN)) return;
  s_beginn = (time_t)persist_read_int(PERSIST_NACHT_LAUF_BEGINN);
  const int gelesen = persist_exists(PERSIST_NACHT_LAUF_HRV)
      ? persist_read_data(PERSIST_NACHT_LAUF_HRV, s_fenster, sizeof(s_fenster)) : 0;
  s_fenster_n = gelesen > 0 ? (uint8_t)(gelesen / (int)sizeof(NachtFenster)) : 0;
}

static void prv_fenster_start(uint16_t minute) {
  puls_beobachten();
  puls_hrv_start();
  puls_dicht_messen();
  s_misst = true;
  s_rest_min = KS_NACHT_FENSTER_MIN;
  s_mess_minute = minute;
  s_mess_seit = time(NULL);
}

static void prv_messung_aus(void) {
  puls_hrv_stop();
  puls_normal_messen();
  puls_ignorieren();
  s_misst = false;
}

static void prv_fenster_ende(void) {
  const uint16_t rmssd = puls_hrv_rmssd();
  const time_t jetzt = time(NULL);
  HealthValue puls = 0;
  if (health_service_metric_averaged_accessible(HealthMetricHeartRateBPM, s_mess_seit, jetzt,
                                                HealthServiceTimeScopeOnce)
      & HealthServiceAccessibilityMaskAvailable) {
    puls = health_service_aggregate_averaged(HealthMetricHeartRateBPM, s_mess_seit, jetzt,
                                             HealthAggregationAvg, HealthServiceTimeScopeOnce);
  }
  prv_messung_aus();
  if (s_fenster_n >= KS_NACHT_FENSTER_MAX) return;
  NachtFenster *f = &s_fenster[s_fenster_n++];
  f->minute = s_mess_minute;
  f->rmssd = rmssd > 255 ? 255 : (uint8_t)rmssd;
  f->puls = (puls > 0 && puls < 256) ? (uint8_t)puls : 0;
  persist_write_data(PERSIST_NACHT_LAUF_HRV, s_fenster, s_fenster_n * sizeof(NachtFenster));
}

bool nacht_misst(void) { return s_misst; }

void nacht_unterbrechen(void) {
  if (s_misst) prv_messung_aus();
}

bool nacht_minute(time_t jetzt) {
  prv_wiederherstellen();
  const struct tm *t = localtime(&jetzt);
  const int m = t->tm_hour * 60 + t->tm_min;
  const time_t minute = jetzt - jetzt % 60;

  if (nacht_an() && nacht_im_fenster(m)) {
    if (!s_beginn) {
      s_beginn = minute;
      s_fenster_n = 0;
      persist_write_int(PERSIST_NACHT_LAUF_BEGINN, (int32_t)s_beginn);
      persist_delete(PERSIST_NACHT_LAUF_HRV);
      APP_LOG(APP_LOG_LEVEL_INFO, "Nacht beginnt");
    }
    const uint16_t seit = (uint16_t)((minute - s_beginn) / 60);
    if (s_misst) {
      if (--s_rest_min == 0) prv_fenster_ende();
    } else if (seit % KS_NACHT_TAKT_MIN == 0 && seit < KS_NACHT_MINUTEN_MAX) {
      prv_fenster_start(seit);
    }
    return false;
  }

  if (!s_beginn) return false;
  // DIE NACHT IST UM. Unter einer Stunde war es keine - ein Fenster, das eben
  // erst anfing, weil jemand die Zeiten verstellt hat.
  if (s_misst) prv_fenster_ende();
  bool fertig = false;
  if (minute - s_beginn >= 60 * 60) {
    persist_write_int(PERSIST_NACHT_OFFEN_BEGINN, (int32_t)s_beginn);
    persist_write_int(PERSIST_NACHT_OFFEN_ENDE, (int32_t)minute);
    if (s_fenster_n > 0) {
      persist_write_data(PERSIST_NACHT_OFFEN_HRV, s_fenster, s_fenster_n * sizeof(NachtFenster));
    } else {
      persist_delete(PERSIST_NACHT_OFFEN_HRV);
    }
    persist_write_int(PERSIST_NACHT_OFFEN_AB, 0);
    fertig = true;
    APP_LOG(APP_LOG_LEVEL_INFO, "Nacht fertig, %d Fenster", (int)s_fenster_n);
  }
  s_beginn = 0;
  s_fenster_n = 0;
  persist_delete(PERSIST_NACHT_LAUF_BEGINN);
  persist_delete(PERSIST_NACHT_LAUF_HRV);
  return fertig;
}

#else
// --- App: lesen und schicken ---

bool nacht_wartet(void) {
  return persist_exists(PERSIST_NACHT_OFFEN_BEGINN) && persist_exists(PERSIST_NACHT_OFFEN_ENDE)
      && nacht_ab() < nacht_anzahl();
}

time_t nacht_beginn(void) {
  return persist_exists(PERSIST_NACHT_OFFEN_BEGINN)
      ? (time_t)persist_read_int(PERSIST_NACHT_OFFEN_BEGINN) : 0;
}

uint16_t nacht_anzahl(void) {
  if (!persist_exists(PERSIST_NACHT_OFFEN_ENDE)) return 0;
  const int32_t dauer = persist_read_int(PERSIST_NACHT_OFFEN_ENDE) - (int32_t)nacht_beginn();
  if (dauer <= 0) return 0;
  const int32_t n = dauer / 60;
  return (uint16_t)(n > KS_NACHT_MINUTEN_MAX ? KS_NACHT_MINUTEN_MAX : n);
}

uint16_t nacht_ab(void) {
  return persist_exists(PERSIST_NACHT_OFFEN_AB) ? (uint16_t)persist_read_int(PERSIST_NACHT_OFFEN_AB) : 0;
}

uint16_t nacht_fenster(uint8_t *aus, uint16_t max) {
  if (!persist_exists(PERSIST_NACHT_OFFEN_HRV)) return 0;
  const int n = persist_read_data(PERSIST_NACHT_OFFEN_HRV, aus, max);
  return n > 0 ? (uint16_t)n : 0;
}

static uint8_t prv_bewegung(uint16_t vmc) {
  // Die Wurzel staucht: im Schlaf liegt vmc bei wenigen Dutzend, beim Gehen
  // bei Tausenden. Linear in ein Byte gepresst, waere der Schlaf eine Null.
  uint32_t x = (uint32_t)vmc * 16, r = 0, bit = 1u << 30;
  while (bit > x) bit >>= 2;
  while (bit) {
    if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
    bit >>= 2;
  }
  return r > 254 ? 254 : (uint8_t)r;
}

uint16_t nacht_minuten(uint8_t *aus, uint16_t ab, uint16_t anzahl) {
  static HealthMinuteData puffer[60];
  const time_t von = nacht_beginn() + (time_t)ab * 60;
  // Erst alles als "keine Daten" - was die Uhr nicht hat, bleibt so.
  for (uint16_t i = 0; i < anzahl; i++) { aus[2 * i] = 255; aus[2 * i + 1] = 0; }
  uint16_t n = 0;
  while (n < anzahl) {
    const uint16_t block = (uint16_t)(anzahl - n > 60 ? 60 : anzahl - n);
    time_t a = von + (time_t)n * 60;
    time_t b = a + (time_t)block * 60;
    const time_t gewollt = a;
    const uint32_t bekommen = health_service_get_minute_history(puffer, block, &a, &b);
    // Die Uhr liefert ab dem ersten Datensatz, den sie hat - der kann
    // spaeter liegen als verlangt.
    const int versatz = a > gewollt ? (int)((a - gewollt) / 60) : 0;
    for (uint32_t i = 0; i < bekommen; i++) {
      const int k = n + versatz + (int)i;
      if (k >= anzahl) break;
      if (puffer[i].is_invalid) continue;
      aus[2 * k] = prv_bewegung(puffer[i].vmc);
      aus[2 * k + 1] = puffer[i].heart_rate_bpm;
    }
    n = (uint16_t)(n + block);
  }
  return anzahl;
}

void nacht_bestaetigt(uint16_t minuten) {
  const uint16_t neu = (uint16_t)(nacht_ab() + minuten);
  if (neu >= nacht_anzahl()) nacht_vergessen();
  else persist_write_int(PERSIST_NACHT_OFFEN_AB, neu);
}

void nacht_vergessen(void) {
  persist_delete(PERSIST_NACHT_OFFEN_BEGINN);
  persist_delete(PERSIST_NACHT_OFFEN_ENDE);
  persist_delete(PERSIST_NACHT_OFFEN_HRV);
  persist_delete(PERSIST_NACHT_OFFEN_AB);
}
#endif
