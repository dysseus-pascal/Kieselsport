#include "telefon.h"
#include "einstellungen.h"
#include "wartend.h"
#include "kurve.h"

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
// Ob die letzte Sendung die Zusammenfassung war - nur deren Bestaetigung
// darf die Wartende loeschen, nicht die einer Zustandsmeldung.
static bool s_letzte_war_zusammenfassung;

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
static bool s_letzte_war_kurve;
static uint16_t s_kurve_unterwegs;
static AppTimer *s_kurve_timer;

// Die letzte Nachricht war die Einstellungsmeldung (siehe unten).
static bool s_letzte_war_einst;

static void prv_kurve_senden(void *data) {
  s_kurve_timer = NULL;
  if (s_hat_wartende || !kurve_wartet()) return;
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
  s_letzte_war_zusammenfassung = false;
  s_letzte_war_kurve = true;
  s_letzte_war_einst = false;
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
  s_letzte_war_zusammenfassung = false;
  s_letzte_war_kurve = false;
  s_letzte_war_einst = true;
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

  // Der neue Stand an beide Seiten - Konfigseite und Kiesel-Helper.
  if (max || becken || ziel || empf || pin_art || pin_zeit) prv_einst_vormerken(300);
}

static void prv_abgelehnt(DictionaryIterator *iter, AppMessageResult grund, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht abgelehnt: %d", (int)grund);
  if (s_letzte_war_einst) {
    s_letzte_war_einst = false;
    if (s_einst_offen && !s_einst_timer) s_einst_timer = app_timer_register(2000, prv_einst_senden, NULL);
    return;
  }
  if (s_hat_wartende && !s_nachfassen) {
    s_nachfassen = app_timer_register(2000, prv_nachfassen, NULL);
  }
  if (!s_letzte_war_zusammenfassung && s_zustand_offen && !s_zustand_timer) {
    s_zustand_timer = app_timer_register(ZUSTAND_ABSTAND_MS, prv_zustand_senden, NULL);
  }
  if (s_letzte_war_kurve && !s_kurve_timer) {
    s_letzte_war_kurve = false;
    s_kurve_timer = app_timer_register(2000, prv_kurve_senden, NULL);
  }
}

static void prv_angekommen(DictionaryIterator *iter, void *context) {
  if (s_letzte_war_einst) {
    s_letzte_war_einst = false;
    s_einst_offen = false;
    return;
  }
  if (s_letzte_war_kurve) {
    s_letzte_war_kurve = false;
    kurve_bestaetigt(s_kurve_unterwegs);
    APP_LOG(APP_LOG_LEVEL_INFO, "Kurve: %u Werte bestaetigt", (unsigned)s_kurve_unterwegs);
    if (kurve_wartet() && !s_kurve_timer) {
      s_kurve_timer = app_timer_register(150, prv_kurve_senden, NULL);
    }
    return;
  }
  if (!s_letzte_war_zusammenfassung) {
    s_zustand_offen = false;
    return;
  }
  s_hat_wartende = false;
  wartend_vergessen();
  APP_LOG(APP_LOG_LEVEL_INFO, "Zusammenfassung bestaetigt");
  // Jetzt die Kurve hinterher.
  if (kurve_wartet() && !s_kurve_timer) {
    s_kurve_timer = app_timer_register(150, prv_kurve_senden, NULL);
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
  s_letzte_war_zusammenfassung = true;
  s_letzte_war_einst = false;
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
  s_letzte_war_zusammenfassung = false;
  s_letzte_war_einst = false;
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
  // Die Einstellungen der Uhr, damit Konfigseite und Kiesel-Helper sie
  // kennen - nach allem, was dringender ist.
  prv_einst_vormerken(2500);
}
