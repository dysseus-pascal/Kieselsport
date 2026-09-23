#include "reps.h"
#include "abschnitt.h"

#include "schluessel.h"

// 25 Messungen je Sekunde, in Paketen zu zehn. Dichter braucht es nicht: eine
// Wiederholung dauert eine gute Sekunde, und jedes Paket kostet Strom.
#define TAKT 25
#define PAKET 10

// Ab wie viel Ruhe ein Satz als beendet gilt - und ab wann der naechste
// beginnt. Vier Sekunden, weil kurzes Absetzen mitten im Satz vorkommt.
#define RUHE_S 4
#define RUHE_SCHWELLE 55     //< milli-g, gemittelte Abweichung
#define AKTIV_SCHWELLE 130

// Kuerzer als das ist keine Wiederholung, laenger als das keine mehr.
#define MIN_ABSTAND 12       //< Messungen, also rund eine halbe Sekunde

// WIE LANGE DER AUSSCHLAG ANHALTEN MUSS, bevor er als Wiederholung zaehlt.
//
// NACHGEMESSEN, NICHT GESCHAETZT: im Emulator eingespeistes Schuetteln mit
// 12,5 Hz - jede Messung das andere Vorzeichen - ergab 21 Wiederholungen in
// zehn Sekunden. Die Sperrzeit allein deckelt eben nur auf zwei je Sekunde;
// sie sagt nichts darueber, ob die Bewegung eine Bewegung war.
//
// Drei Messungen sind 0,12 Sekunden. Eine gehobene Hantel steht laenger
// oben als das (gemessen: acht Messungen bei einer Wiederholung je
// Sekunde), ein Zittern nie: bei 12,5 Hz dauert jede Halbwelle eine
// einzige Messung.
#define HALTEN 3

static bool s_an;
static bool s_pausiert;
static uint8_t s_empfind = 2;
static uint16_t s_pausenziel = 90;

static int32_t s_basis;          //< gleitender Mittelwert der Staerke
static int32_t s_bewegung;       //< gleitender Mittelwert der Abweichung
static bool s_oben;              //< ueber der oberen Schwelle gewesen?
static uint8_t s_haelt;          //< Messungen in Folge ueber der Schwelle
static uint16_t s_seit_rep;      //< Messungen seit der letzten Wiederholung

static uint16_t s_reps;
static uint16_t s_saetze;
static uint16_t s_satz_beginn;
static uint16_t s_ruhe_zaehler;
static bool s_ruht;
static uint16_t s_ruhe_beginn;
static uint16_t s_ruhe_dauer;
static bool s_gebrummt;
static bool s_brumm_faellig;   //< die Pause ist um - die App soll brummen

static int32_t prv_schwelle(void) {
  // Fein zaehlt kleine Bewegungen mit, traege nur deutliche.
  switch (s_empfind) {
    case 1: return 260;
    case 3: return 110;
    default: return 180;
  }
}

/** Ganzzahlige Wurzel - der Uebersetzer bekommt hier keine Fliesskommazahl. */
static int32_t prv_wurzel(int32_t x) {
  if (x <= 0) return 0;
  int32_t w = x, letzte = 0;
  // Newton, sechs Runden reichen fuer alles, was ein Beschleunigungsmesser
  // liefert; die Abbruchbedingung faengt den Rest.
  for (int i = 0; i < 12 && w != letzte; i++) {
    letzte = w;
    w = (w + x / w) / 2;
  }
  return w;
}

static void prv_daten(AccelData *daten, uint32_t anzahl) {
  if (!s_an || s_pausiert) return;
  for (uint32_t i = 0; i < anzahl; i++) {
    if (daten[i].did_vibrate) continue;  //< das Brummen ist keine Bewegung
    const int32_t x = daten[i].x, y = daten[i].y, z = daten[i].z;
    const int32_t staerke = prv_wurzel(x * x + y * y + z * z);

    // Der gleitende Mittelwert IST die Schwerkraft samt Koerperhaltung. Ihn
    // abzuziehen macht aus "wo ist unten" ein "was bewegt sich".
    if (s_basis == 0) s_basis = staerke;
    s_basis += (staerke - s_basis) / 16;
    const int32_t abw = staerke - s_basis;
    const int32_t betrag = abw < 0 ? -abw : abw;
    s_bewegung += (betrag - s_bewegung) / 8;

    if (s_seit_rep < 0xFFFF) s_seit_rep++;

    // Ein Schmitt-Trigger, keine blosse Schwelle: gezaehlt wird erst, wenn
    // die Bewegung vorher unten WAR. Sonst zaehlte ein Zittern an der
    // Schwelle zwanzig Wiederholungen in einer Sekunde.
    const int32_t schwelle = prv_schwelle();
    if (abw > schwelle) {
      if (s_haelt < 0xFF) s_haelt++;
    } else {
      s_haelt = 0;
    }
    if (!s_oben && s_haelt >= HALTEN) {
      s_oben = true;
      if (s_seit_rep >= MIN_ABSTAND) {
        s_reps++;
        s_seit_rep = 0;
        if (s_ruht) {
          // Bewegung in der Pause heisst: der naechste Satz hat begonnen.
          s_ruht = false;
          s_gebrummt = false;
        }
      }
    } else if (s_oben && abw < -schwelle) {
      s_oben = false;
    }
  }
}

void reps_start(void) {
  s_an = true;
  s_pausiert = false;
  s_basis = 0;
  s_bewegung = 0;
  s_oben = false;
  s_haelt = 0;
  s_seit_rep = MIN_ABSTAND;
  s_reps = 0;
  s_saetze = 0;
  s_satz_beginn = 0;
  s_ruhe_zaehler = 0;
  s_ruht = false;
  s_ruhe_beginn = 0;
  s_ruhe_dauer = 0;
  s_gebrummt = false;

  if (persist_exists(PERSIST_EMPFIND)) s_empfind = (uint8_t)persist_read_int(PERSIST_EMPFIND);
  if (persist_exists(PERSIST_PAUSENZIEL)) {
    s_pausenziel = (uint16_t)persist_read_int(PERSIST_PAUSENZIEL);
  }

  accel_data_service_subscribe(PAKET, prv_daten);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
}

void reps_stoppe(uint16_t sekunde) {
  if (!s_an) return;
  // DEN OFFENEN SATZ NOCH ABSCHLIESSEN. Wer nach der letzten Wiederholung
  // sofort auf Stop drueckt, haette ihn sonst umsonst gemacht.
  if (!s_ruht && s_reps >= 2) {
    abschnitt_hinzu(s_satz_beginn, s_reps,
                    sekunde > s_satz_beginn ? (uint16_t)(sekunde - s_satz_beginn) : 0);
    s_saetze++;
    s_reps = 0;
  }
  accel_data_service_unsubscribe();
  s_an = false;
}

void reps_pause(bool an) { s_pausiert = an; }

#ifdef KS_DEMO
static uint16_t s_demo;
#endif

void reps_tick(uint16_t sekunde) {
#ifdef KS_DEMO
  s_demo = sekunde;
#endif
  if (!s_an || s_pausiert) return;

  if (s_bewegung < RUHE_SCHWELLE) {
    if (s_ruhe_zaehler < 0xFFFF) s_ruhe_zaehler++;
  } else if (s_bewegung > AKTIV_SCHWELLE) {
    s_ruhe_zaehler = 0;
  }

  if (!s_ruht && s_ruhe_zaehler >= RUHE_S) {
    // EIN SATZ AB ZWEI WIEDERHOLUNGEN. Eine einzelne gezaehlte Bewegung ist
    // meistens ein Griff zur Flasche.
    if (s_reps >= 2) {
      const uint16_t ende = sekunde > RUHE_S ? (uint16_t)(sekunde - RUHE_S) : sekunde;
      abschnitt_hinzu(s_satz_beginn, s_reps,
                      ende > s_satz_beginn ? (uint16_t)(ende - s_satz_beginn) : 0);
      s_saetze++;
    }
    s_ruht = true;
    s_ruhe_beginn = sekunde;
    s_ruhe_dauer = 0;
    s_gebrummt = false;
    s_reps = 0;
  }

  s_ruhe_dauer = s_ruht && sekunde > s_ruhe_beginn ? (uint16_t)(sekunde - s_ruhe_beginn) : 0;

  if (s_ruht && !s_gebrummt && s_pausenziel > 0 &&
      sekunde >= s_ruhe_beginn + s_pausenziel) {
    // EINMAL BRUMMEN, WENN DIE PAUSE UM IST. Das ist der eine Dienst, den die
    // Uhr hier leistet und das Telefon nicht: man hat die Hantel in der Hand
    // und schaut nirgendwo hin. Brummen kann nur die App - der Worker darf
    // nicht -, also wird es hier nur vorgemerkt.
    s_brumm_faellig = true;
    s_gebrummt = true;
  }

#ifdef KS_ECHT_SENSOR
  APP_LOG(APP_LOG_LEVEL_INFO, "Kraft s=%u reps=%u saetze=%u bewegung=%d ruht=%d basis=%d",
          (unsigned)sekunde, (unsigned)s_reps, (unsigned)s_saetze,
          (int)s_bewegung, (int)s_ruht, (int)s_basis);
#endif

  // Ein Satz beginnt mit seiner ERSTEN Wiederholung, nicht mit dem Druck
  // auf Start: dazwischen liegt das Hinlaufen zur Bank.
  if (!s_ruht && s_reps == 1) s_satz_beginn = sekunde;
}

// NUR IM PRUEFBAU. Der Emulator hat keinen bewegten Arm; ohne erfundene
// Zahlen laesst sich der Kraft-Schirm nie ansehen. Vierzig Sekunden Satz,
// zwanzig Sekunden Pause, immer im Wechsel.
#ifdef KS_DEMO
#define DEMO_ZYKLUS 60
#define DEMO_SATZ 40
static bool prv_demo_ruht(void) { return (s_demo % DEMO_ZYKLUS) >= DEMO_SATZ; }
#endif

uint16_t reps_laufend(void) {
#if defined(KS_DEMO) && !defined(KS_ECHT_SENSOR)
  return prv_demo_ruht() ? 0 : (uint16_t)((s_demo % DEMO_ZYKLUS) / 3);
#else
  return s_reps;
#endif
}

bool reps_ruht(void) {
#if defined(KS_DEMO) && !defined(KS_ECHT_SENSOR)
  return prv_demo_ruht();
#else
  return s_ruht;
#endif
}

uint16_t reps_ruhe_s(void) {
#if defined(KS_DEMO) && !defined(KS_ECHT_SENSOR)
  return prv_demo_ruht() ? (uint16_t)((s_demo % DEMO_ZYKLUS) - DEMO_SATZ) : 0;
#else
  return s_ruhe_dauer;
#endif
}

uint16_t reps_saetze(void) {
#if defined(KS_DEMO) && !defined(KS_ECHT_SENSOR)
  return (uint16_t)(s_demo / DEMO_ZYKLUS);
#else
  return s_saetze;
#endif
}

bool reps_brumm_holen(void) {
  const bool war = s_brumm_faellig;
  s_brumm_faellig = false;
  return war;
}

void reps_empfindlichkeit_setze(uint8_t stufe) {
  if (stufe < 1 || stufe > 3) return;
  s_empfind = stufe;
  persist_write_int(PERSIST_EMPFIND, stufe);
}
uint8_t reps_empfindlichkeit(void) { return s_empfind; }

void reps_pausenziel_setze(uint16_t sekunden) {
  s_pausenziel = sekunden;
  persist_write_int(PERSIST_PAUSENZIEL, sekunden);
}
uint16_t reps_pausenziel(void) { return s_pausenziel; }
