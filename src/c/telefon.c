#include "telefon.h"
#include "einstellungen.h"
#include "wartend.h"
#include "kurve.h"
#include "nacht.h"

// Der Postausgang traegt auch die Abschnittsliste - bis zu zwanzig Saetze
// oder Bahnen als Text. 512 reicht dafuer mit Luft.
#define INBOX_SIZE 256
#define OUTBOX_SIZE 512

// Ein zweiter Anlauf, falls der Postausgang gerade besetzt ist. Er fasst
// genau EINE Nachricht; kommt die Zusammenfassung zu dicht hinter etwas
// anderem, fiele sie still mit BUSY aus - und das Training wäre weg.
static AppTimer *s_nachfassen;
static Trainingsstand s_wartet;
static char s_liste[KS_LISTE_MAX];
static bool s_hat_wartende;
// WAS ZULETZT HINAUSGING. Der Postausgang fasst genau eine Nachricht, und
// Bestaetigung wie Ablehnung sagen nicht, welche es war - nur die
// Bestaetigung der Zusammenfassung darf die Wartende loeschen, nur die eines
// Kurvenstuecks den Zeiger der Kurve vorruecken. Frueher standen hier drei
// Schalter; mit Nacht und Einzelmessung waeren es fuenf geworden.
typedef enum {
  SendungNichts = 0,
  SendungZusammenfassung,
  SendungZustand,
  SendungKurve,
  SendungEinst,
  SendungNacht,
  SendungHrv,
} Sendung;
static Sendung s_letzte;

static void prv_nacht_weiter(void);
static void prv_nacht_bestaetigt(void);

// Die offene Zustandsmeldung, bis das Telefon sie hat. Ein paar Anlaeufe,
// nicht endlos: die Zusammenfassung am Ende sagt ohnehin alles noch einmal.
static bool s_zustand_offen;
static Trainingsmeldung s_zustand_was;
static uint8_t s_zustand_art;
static uint32_t s_zustand_beginn;
static uint8_t s_zustand_versuche;
static AppTimer *s_zustand_timer;
#define ZUSTAND_VERSUCHE 6
#define ZUSTAND_ABSTAND_MS 1500

static void prv_zustand_senden(void *data);

// --- Die Pulskurve, stueckweise ---
//
// Erst wenn die Zusammenfassung bestaetigt ist: sie ist das Wichtige, und der
// Postausgang fasst genau eine Nachricht. Dann je Nachricht bis zu 300 Werte
// (fuenfzig Minuten), bis alles drueben ist. Jedes bestaetigte Stueck rueckt
// den Zeiger im Persist vor - geht die App dazwischen zu, geht es beim
// naechsten Oeffnen dort weiter.
#define KURVE_JE_NACHRICHT 300
static uint16_t s_kurve_unterwegs;
static AppTimer *s_kurve_timer;


static void prv_kurve_senden(void *data) {
  s_kurve_timer = NULL;
  if (s_hat_wartende) return;
  if (!kurve_wartet()) { prv_nacht_weiter(); return; }
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    s_kurve_timer = app_timer_register(700, prv_kurve_senden, NULL);
    return;
  }
  static uint8_t stueck[KURVE_JE_NACHRICHT];
  const uint16_t ab = kurve_ab();
  const uint16_t n = kurve_stueck(stueck, sizeof(stueck));
  if (n == 0) { kurve_vergessen(); return; }
  dict_write_int32(out, MESSAGE_KEY_BEGINN, (int32_t)kurve_beginn());
  dict_write_int32(out, MESSAGE_KEY_KURVE_ANZAHL, (int32_t)kurve_anzahl());
  dict_write_int32(out, MESSAGE_KEY_KURVE_AB, (int32_t)ab);
  dict_write_data(out, MESSAGE_KEY_KURVE, stueck, n);
  s_kurve_unterwegs = n;
  s_letzte = SendungKurve;
  app_message_outbox_send();
}

static void prv_sende_jetzt(void);

// --- Die Einstellungen ans Telefon ---
//
// Beim Start und nach jeder Aenderung. Hinter allem anderen: die
// Zusammenfassung und die Kurve gehen vor, die Meldung wartet, bis der
// Postausgang frei ist.
static bool s_einst_offen;
static AppTimer *s_einst_timer;
static uint8_t s_einst_versuche;

static void prv_einst_senden(void *data) {
  s_einst_timer = NULL;
  if (!s_einst_offen) return;
  if (s_einst_versuche >= 6) { s_einst_offen = false; return; }
  s_einst_versuche++;
  DictionaryIterator *out;
  if (s_hat_wartende || app_message_outbox_begin(&out) != APP_MSG_OK) {
    s_einst_timer = app_timer_register(1500, prv_einst_senden, NULL);
    return;
  }
  einstellungen_melden(out);
  s_letzte = SendungEinst;
  app_message_outbox_send();
}

static void prv_einst_vormerken(uint32_t ms) {
  s_einst_offen = true;
  s_einst_versuche = 0;
  if (s_einst_timer) app_timer_cancel(s_einst_timer);
  s_einst_timer = app_timer_register(ms, prv_einst_senden, NULL);
}

static void prv_nachfassen(void *data) {
  s_nachfassen = NULL;
  if (s_hat_wartende) prv_sende_jetzt();
}

/** Clay schickt Zahlen mal als Zahl, mal als Zeichenkette - beides nehmen. */
static int32_t prv_zahl(Tuple *t) {
  return t->type == TUPLE_CSTRING ? atoi(t->value->cstring) : t->value->int32;
}

/** Minuten seit Mitternacht - aus einer Zahl oder aus "HH:MM". */
static int32_t prv_minuten(Tuple *t) {
  if (t->type != TUPLE_CSTRING) return t->value->int32;
  const char *z = t->value->cstring;
  const char *doppel = strchr(z, ':');
  if (!doppel) return -1;
  return atoi(z) * 60 + atoi(doppel + 1);
}

static void prv_inbox(DictionaryIterator *iter, void *context) {
  Tuple *max = dict_find(iter, MESSAGE_KEY_MAXPULS);
  if (max) einstellungen_maxpuls(prv_zahl(max));

  Tuple *becken = dict_find(iter, MESSAGE_KEY_BECKEN);
  if (becken) einstellungen_becken(prv_zahl(becken));

  Tuple *ziel = dict_find(iter, MESSAGE_KEY_PAUSENZIEL);
  if (ziel) einstellungen_pausenziel(prv_zahl(ziel));

  Tuple *empf = dict_find(iter, MESSAGE_KEY_EMPFIND);
  if (empf) einstellungen_empfindlichkeit(prv_zahl(empf));

  Tuple *pin_art = dict_find(iter, MESSAGE_KEY_PIN_ART);
  if (pin_art) einstellungen_pin_art(prv_zahl(pin_art));

  Tuple *pin_zeit = dict_find(iter, MESSAGE_KEY_PIN_ZEIT);
  if (pin_zeit && pin_zeit->type == TUPLE_CSTRING) einstellungen_pin_zeit(pin_zeit->value->cstring);

  Tuple *nacht_an = dict_find(iter, MESSAGE_KEY_NACHT_AN);
  if (nacht_an) einstellungen_nacht_an(prv_zahl(nacht_an) != 0);

  // Beginn und Ende: von Kiesel-Helper als Minuten, von der Konfigseite als
  // "HH:MM" - beides nehmen.
  Tuple *nacht_von = dict_find(iter, MESSAGE_KEY_NACHT_VON);
  if (nacht_von) einstellungen_nacht_von(prv_minuten(nacht_von));
  Tuple *nacht_bis = dict_find(iter, MESSAGE_KEY_NACHT_BIS);
  if (nacht_bis) einstellungen_nacht_bis(prv_minuten(nacht_bis));

  // Der neue Stand an beide Seiten - Konfigseite und Kiesel-Helper.
  if (max || becken || ziel || empf || pin_art || pin_zeit || nacht_an || nacht_von || nacht_bis) {
    prv_einst_vormerken(300);
  }
}

static void prv_nacht_senden(void *data);
static void prv_hrv_senden(void *data);
static AppTimer *s_nacht_timer;
static AppTimer *s_hrv_timer;

static void prv_abgelehnt(DictionaryIterator *iter, AppMessageResult grund, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht abgelehnt: %d", (int)grund);
  const Sendung war = s_letzte;
  s_letzte = SendungNichts;
  switch (war) {
    case SendungEinst:
      if (s_einst_offen && !s_einst_timer) s_einst_timer = app_timer_register(2000, prv_einst_senden, NULL);
      return;
    case SendungKurve:
      if (!s_kurve_timer) s_kurve_timer = app_timer_register(2000, prv_kurve_senden, NULL);
      break;
    case SendungZustand:
      if (s_zustand_offen && !s_zustand_timer) {
        s_zustand_timer = app_timer_register(ZUSTAND_ABSTAND_MS, prv_zustand_senden, NULL);
      }
      break;
    case SendungNacht:
      // Nicht endlos draengeln: ist das Telefon weg, geht die Nacht beim
      // naechsten Oeffnen der App weiter, ab dem letzten bestaetigten Stueck.
      if (!s_nacht_timer) s_nacht_timer = app_timer_register(3000, prv_nacht_senden, NULL);
      break;
    case SendungHrv:
      if (!s_hrv_timer) s_hrv_timer = app_timer_register(2000, prv_hrv_senden, NULL);
      break;
    default:
      break;
  }
  if (s_hat_wartende && !s_nachfassen) {
    s_nachfassen = app_timer_register(2000, prv_nachfassen, NULL);
  }
}

static void prv_hrv_bestaetigt(void);

static void prv_angekommen(DictionaryIterator *iter, void *context) {
  const Sendung war = s_letzte;
  s_letzte = SendungNichts;
  switch (war) {
    case SendungEinst:
      s_einst_offen = false;
      return;
    case SendungKurve:
      kurve_bestaetigt(s_kurve_unterwegs);
      APP_LOG(APP_LOG_LEVEL_INFO, "Kurve: %u Werte bestaetigt", (unsigned)s_kurve_unterwegs);
      if (kurve_wartet()) {
        if (!s_kurve_timer) s_kurve_timer = app_timer_register(150, prv_kurve_senden, NULL);
      } else {
        prv_nacht_weiter();
      }
      return;
    case SendungZustand:
      s_zustand_offen = false;
      return;
    case SendungNacht:
      prv_nacht_bestaetigt();
      return;
    case SendungHrv:
      prv_hrv_bestaetigt();
      return;
    case SendungZusammenfassung:
      break;
    default:
      return;
  }
  s_hat_wartende = false;
  wartend_vergessen();
  APP_LOG(APP_LOG_LEVEL_INFO, "Zusammenfassung bestaetigt");
  // Jetzt die Kurve hinterher - oder, ohne Kurve, die Nacht.
  if (kurve_wartet()) {
    if (!s_kurve_timer) s_kurve_timer = app_timer_register(150, prv_kurve_senden, NULL);
  } else {
    prv_nacht_weiter();
  }
}

static void prv_sende_jetzt(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    if (!s_nachfassen) s_nachfassen = app_timer_register(700, prv_nachfassen, NULL);
    return;
  }
  dict_write_int32(out, MESSAGE_KEY_ART, (int32_t)s_wartet.art);
  dict_write_int32(out, MESSAGE_KEY_BEGINN, (int32_t)s_wartet.beginn);
  dict_write_int32(out, MESSAGE_KEY_DAUER, (int32_t)s_wartet.dauer_s);
  dict_write_int32(out, MESSAGE_KEY_SCHRITTE, (int32_t)s_wartet.schritte);
  dict_write_int32(out, MESSAGE_KEY_METER, (int32_t)s_wartet.meter);
  dict_write_int32(out, MESSAGE_KEY_KCAL, (int32_t)s_wartet.kcal);
  dict_write_int32(out, MESSAGE_KEY_PULS_MITTEL, (int32_t)s_wartet.puls_mittel);
  dict_write_int32(out, MESSAGE_KEY_PULS_MAX, (int32_t)s_wartet.puls_max);
  dict_write_int32(out, MESSAGE_KEY_SAETZE, (int32_t)s_wartet.saetze);
  dict_write_int32(out, MESSAGE_KEY_REPS, (int32_t)s_wartet.reps);
  dict_write_int32(out, MESSAGE_KEY_BAHNEN, (int32_t)s_wartet.bahnen);
  // Nur bei Yoga - und nur, wenn genug Schlaege fuer eine Zahl da waren.
  if (s_wartet.hrv_ms > 0) {
    dict_write_int32(out, MESSAGE_KEY_HRV, (int32_t)s_wartet.hrv_ms);
  }
  // DIE LISTE IST DAS, WAS DEN TAG SPAETER ERKLAERT. "4 Saetze" sagt wenig,
  // "12/10/8/8 mit 90 Sekunden dazwischen" sagt alles - und auf dem Telefon
  // wird jeder Abschnitt ein eigener Eintrag in der Gesundheitsakte.
  if (s_liste[0]) {
    dict_write_cstring(out, MESSAGE_KEY_ABSCHNITTE, s_liste);
  }
  // DAS ENDE STEHT IN DERSELBEN NACHRICHT. Eine eigene Stopmeldung daneben
  // straeubte sich mit dieser um den Postausgang - der fasst genau EINE
  // Nachricht, und die zweite fiele mit BUSY aus.
  dict_write_int32(out, MESSAGE_KEY_ZUSTAND, (int32_t)ZustandStop);
  s_letzte = SendungZusammenfassung;
  app_message_outbox_send();
}

bool telefon_wartet(void) { return s_hat_wartende || kurve_wartet(); }

void telefon_nachsenden(void) {
  if (!wartend_laden(&s_wartet, s_liste, sizeof(s_liste))) return;
  s_hat_wartende = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "Zusammenfassung geht ans Telefon");
  // Mit etwas Abstand, damit die Verbindung zum Telefon erst steht.
  if (!s_nachfassen) s_nachfassen = app_timer_register(600, prv_nachfassen, NULL);
}

/**
 * Eine kurze Zustandsmeldung - mit ein paar Anlaeufen.
 *
 * DER START MUSS ANKOMMEN, sonst fehlt die Strecke: das Telefon zeichnet
 * nur auf, wenn es weiss, dass ein Training laeuft. Der erste Entwurf
 * schickte die Meldung genau einmal; war die Verbindung in dieser Sekunde
 * gerade besetzt, gab es ein Wandern ohne Karte, und niemand wusste, warum.
 * Jetzt fasst sie nach - ein paarmal, nicht endlos: die Zusammenfassung am
 * Ende sagt alles noch einmal, und die fasst so lange nach, bis sie da ist.
 */
static void prv_zustand_senden(void *data) {
  s_zustand_timer = NULL;
  if (!s_zustand_offen) return;
  if (s_zustand_versuche >= ZUSTAND_VERSUCHE) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Zustandsmeldung aufgegeben");
    s_zustand_offen = false;
    return;
  }
  s_zustand_versuche++;
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    s_zustand_timer = app_timer_register(ZUSTAND_ABSTAND_MS, prv_zustand_senden, NULL);
    return;
  }
  dict_write_int32(out, MESSAGE_KEY_ZUSTAND, (int32_t)s_zustand_was);
  dict_write_int32(out, MESSAGE_KEY_ART, (int32_t)s_zustand_art);
  // DER BEGINN MUSS SCHON HIER MIT. Das Telefon legt die Spurdatei unter
  // diesem Zeitpunkt ab; erfuehre es ihn erst mit der Zusammenfassung,
  // haette es die Punkte unter einem anderen Namen gesammelt.
  dict_write_int32(out, MESSAGE_KEY_BEGINN, (int32_t)s_zustand_beginn);
  s_letzte = SendungZustand;
  app_message_outbox_send();
}

void telefon_melde_zustand(Trainingsmeldung was, uint8_t art, uint32_t beginn) {
  // Eine neue Meldung ersetzt die offene: was jetzt gilt, zaehlt.
  s_zustand_offen = true;
  s_zustand_was = was;
  s_zustand_art = art;
  s_zustand_beginn = beginn;
  s_zustand_versuche = 0;
  if (s_zustand_timer) { app_timer_cancel(s_zustand_timer); s_zustand_timer = NULL; }
  prv_zustand_senden(NULL);
}

// --- Die Nacht, stueckweise ---
//
// NACH ALLEM ANDEREN: Zusammenfassung und Kurve eines Trainings gehen vor -
// die Nacht wartet ohnehin schon Stunden. Je Nachricht 150 Minuten (300
// Byte); dazu im ersten Stueck die HRV-Fenster. Jedes bestaetigte Stueck
// rueckt den Zeiger im Persist vor (nacht.h), wie bei der Kurve.
#define NACHT_JE_NACHRICHT 150
static uint16_t s_nacht_unterwegs;
static void (*s_nacht_fertig)(void);

static void prv_nacht_senden(void *data) {
  s_nacht_timer = NULL;
  if (s_hat_wartende || kurve_wartet()) return;
  if (!nacht_wartet()) {
    if (s_nacht_fertig) s_nacht_fertig();
    return;
  }
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    s_nacht_timer = app_timer_register(700, prv_nacht_senden, NULL);
    return;
  }
  static uint8_t minuten[NACHT_JE_NACHRICHT * 2];
  const uint16_t ab = nacht_ab();
  const uint16_t anzahl = nacht_anzahl();
  uint16_t n = (uint16_t)(anzahl - ab);
  if (n > NACHT_JE_NACHRICHT) n = NACHT_JE_NACHRICHT;
  nacht_minuten(minuten, ab, n);
  dict_write_int32(out, MESSAGE_KEY_NACHT_BEGINN, (int32_t)nacht_beginn());
  dict_write_int32(out, MESSAGE_KEY_NACHT_ANZAHL, (int32_t)anzahl);
  dict_write_int32(out, MESSAGE_KEY_NACHT_AB, (int32_t)ab);
  dict_write_data(out, MESSAGE_KEY_NACHT_MINUTEN, minuten, (uint16_t)(n * 2));
  if (ab == 0) {
    static uint8_t fenster[KS_NACHT_FENSTER_MAX * sizeof(NachtFenster)];
    const uint16_t f = nacht_fenster(fenster, sizeof(fenster));
    if (f > 0) dict_write_data(out, MESSAGE_KEY_NACHT_HRV, fenster, f);
  }
  s_nacht_unterwegs = n;
  s_letzte = SendungNacht;
  app_message_outbox_send();
}

// Das naechste Stueck anstossen - nach Training und Kurve, oder nach einem
// bestaetigten Stueck.
static void prv_nacht_weiter(void) {
  if (!s_nacht_timer) s_nacht_timer = app_timer_register(150, prv_nacht_senden, NULL);
}

static void prv_nacht_bestaetigt(void) {
  nacht_bestaetigt(s_nacht_unterwegs);
  APP_LOG(APP_LOG_LEVEL_INFO, "Nacht: %u Minuten bestaetigt", (unsigned)s_nacht_unterwegs);
  s_nacht_unterwegs = 0;
  prv_nacht_weiter();
}

bool telefon_nacht_wartet(void) { return nacht_wartet(); }

void telefon_bei_nacht_fertig(void (*fertig)(void)) { s_nacht_fertig = fertig; }

// --- Eine einzelne HRV-Messung ---
static bool s_hrv_offen;
static uint16_t s_hrv_wert;
static time_t s_hrv_zeit;
static uint8_t s_hrv_versuche;

static void prv_hrv_senden(void *data) {
  s_hrv_timer = NULL;
  if (!s_hrv_offen) return;
  if (s_hrv_versuche >= 10) { s_hrv_offen = false; return; }
  s_hrv_versuche++;
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    s_hrv_timer = app_timer_register(1500, prv_hrv_senden, NULL);
    return;
  }
  dict_write_int32(out, MESSAGE_KEY_HRV, (int32_t)s_hrv_wert);
  dict_write_int32(out, MESSAGE_KEY_HRV_ZEIT, (int32_t)s_hrv_zeit);
  s_letzte = SendungHrv;
  app_message_outbox_send();
}

static void prv_hrv_bestaetigt(void) {
  s_hrv_offen = false;
}

void telefon_hrv(uint16_t rmssd, time_t wann) {
  s_hrv_offen = true;
  s_hrv_wert = rmssd;
  s_hrv_zeit = wann;
  s_hrv_versuche = 0;
  if (s_hrv_timer) { app_timer_cancel(s_hrv_timer); s_hrv_timer = NULL; }
  prv_hrv_senden(NULL);
}

bool telefon_hrv_offen(void) { return s_hrv_offen; }

void telefon_init(void) {
  app_message_register_inbox_received(prv_inbox);
  app_message_register_outbox_failed(prv_abgelehnt);
  app_message_register_outbox_sent(prv_angekommen);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);

  // Liegt noch eine unbestaetigte Zusammenfassung da - vom letzten Mal, als
  // das Telefon nicht zuhoerte -, geht sie jetzt. Und eine liegengebliebene
  // Kurve gleich danach.
  telefon_nachsenden();
  if (!s_hat_wartende && kurve_wartet()) {
    s_kurve_timer = app_timer_register(800, prv_kurve_senden, NULL);
  }
  // Eine wartende Nacht - hinter der Zusammenfassung und der Kurve, die
  // stossen sie an, wenn sie fertig sind.
  if (!s_hat_wartende && !kurve_wartet() && nacht_wartet()) {
    s_nacht_timer = app_timer_register(1200, prv_nacht_senden, NULL);
  }
  // Die Einstellungen der Uhr, damit Konfigseite und Kiesel-Helper sie
  // kennen - nach allem, was dringender ist.
  prv_einst_vormerken(2500);
}
