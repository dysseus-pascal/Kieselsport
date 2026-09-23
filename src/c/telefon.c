#include "telefon.h"
#include "puls.h"
#include "reps.h"
#include "bahnen.h"
#include "abschnitt.h"

// Der Postausgang traegt jetzt auch die Abschnittsliste - bis zu zwanzig
// Saetze oder Bahnen als Text. 512 reicht dafuer mit Luft; die Liste selbst
// bricht bei KS_LISTE_MAX ab.
#define INBOX_SIZE 256
#define OUTBOX_SIZE 512
#define KS_LISTE_MAX 300

// Ein zweiter Anlauf, falls der Postausgang gerade besetzt ist. Er fasst
// genau EINE Nachricht; kommt die Zusammenfassung zu dicht hinter etwas
// anderem, fiele sie still mit BUSY aus - und das Training wäre weg.
static AppTimer *s_nachfassen;
static Trainingsstand s_wartet;
static bool s_hat_wartende;
// Die Abschnittsliste gehoert zur wartenden Zusammenfassung, nicht zum
// laufenden Zaehler: der wird beim naechsten Start geleert, und dann
// schickte ein Nachfassen die Saetze des NEUEN Trainings mit dem alten.
static char s_liste[KS_LISTE_MAX];
// Ob die letzte Sendung die Zusammenfassung war - nur deren Bestaetigung
// darf die Wartende loeschen, nicht die einer Zustandsmeldung.
static bool s_letzte_war_zusammenfassung;

// DIE ZUSAMMENFASSUNG UEBERLEBT DAS SCHLIESSEN DER APP. Bis hierher lag sie
// nur im Speicher: war das Telefon nicht erreichbar oder hoerte dort gerade
// niemand zu, fasste die Uhr nach, solange die App offen war - und vergass
// sie beim Verlassen. Ein Training, das nie ankam, war damit weg. Jetzt
// liegt sie im Persist, bis das Telefon sie bestaetigt hat, und der
// naechste Start der App schickt sie noch einmal.
#define PERSIST_WARTET 30
#define PERSIST_WARTET_LISTE_A 31
#define PERSIST_WARTET_LISTE_B 32
// persist_write_string nimmt hoechstens 256 Zeichen; die Liste darf 300 sein.
#define PERSIST_HALB 200

static void prv_sende_jetzt(void);

static void prv_nachfassen(void *data) {
  s_nachfassen = NULL;
  if (s_hat_wartende) prv_sende_jetzt();
}

static void prv_wartende_merken(void) {
  persist_write_data(PERSIST_WARTET, &s_wartet, sizeof(s_wartet));
  char halb[PERSIST_HALB + 1];
  strncpy(halb, s_liste, PERSIST_HALB);
  halb[PERSIST_HALB] = 0;
  persist_write_string(PERSIST_WARTET_LISTE_A, halb);
  const size_t laenge = strlen(s_liste);
  if (laenge > PERSIST_HALB) {
    persist_write_string(PERSIST_WARTET_LISTE_B, s_liste + PERSIST_HALB);
  } else {
    persist_delete(PERSIST_WARTET_LISTE_B);
  }
}

static void prv_wartende_vergessen(void) {
  persist_delete(PERSIST_WARTET);
  persist_delete(PERSIST_WARTET_LISTE_A);
  persist_delete(PERSIST_WARTET_LISTE_B);
}

static bool prv_wartende_laden(void) {
  if (!persist_exists(PERSIST_WARTET)) return false;
  memset(&s_wartet, 0, sizeof(s_wartet));
  persist_read_data(PERSIST_WARTET, &s_wartet, sizeof(s_wartet));
  s_liste[0] = 0;
  if (persist_exists(PERSIST_WARTET_LISTE_A)) {
    persist_read_string(PERSIST_WARTET_LISTE_A, s_liste, PERSIST_HALB + 1);
  }
  if (persist_exists(PERSIST_WARTET_LISTE_B)) {
    const size_t bisher = strlen(s_liste);
    persist_read_string(PERSIST_WARTET_LISTE_B, s_liste + bisher,
                        (uint16_t)(sizeof(s_liste) - bisher));
  }
  return s_wartet.beginn > 0 && s_wartet.dauer_s > 0;
}

/** Clay schickt Zahlen mal als Zahl, mal als Zeichenkette - beides nehmen. */
static int32_t prv_zahl(Tuple *t) {
  return t->type == TUPLE_CSTRING ? atoi(t->value->cstring) : t->value->int32;
}

static void prv_inbox(DictionaryIterator *iter, void *context) {
  Tuple *max = dict_find(iter, MESSAGE_KEY_MAXPULS);
  if (max) puls_setze_maximum((uint16_t)prv_zahl(max));

  Tuple *becken = dict_find(iter, MESSAGE_KEY_BECKEN);
  if (becken) bahnen_becken_setze((uint16_t)prv_zahl(becken));

  Tuple *ziel = dict_find(iter, MESSAGE_KEY_PAUSENZIEL);
  if (ziel) reps_pausenziel_setze((uint16_t)prv_zahl(ziel));

  Tuple *empf = dict_find(iter, MESSAGE_KEY_EMPFIND);
  if (empf) reps_empfindlichkeit_setze((uint8_t)prv_zahl(empf));
}

static void prv_abgelehnt(DictionaryIterator *iter, AppMessageResult grund, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht abgelehnt: %d", (int)grund);
  if (s_hat_wartende && !s_nachfassen) {
    s_nachfassen = app_timer_register(2000, prv_nachfassen, NULL);
  }
}

static void prv_angekommen(DictionaryIterator *iter, void *context) {
  if (!s_letzte_war_zusammenfassung) return;
  s_hat_wartende = false;
  prv_wartende_vergessen();
  APP_LOG(APP_LOG_LEVEL_INFO, "Zusammenfassung bestaetigt");
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
  app_message_outbox_send();
}

void telefon_sende(const Trainingsstand *t) {
  s_wartet = *t;
  s_liste[0] = 0;
  if (abschnitt_anzahl() > 0) abschnitt_als_text(s_liste, sizeof(s_liste));
  s_hat_wartende = true;
  prv_wartende_merken();
  prv_sende_jetzt();
}

bool telefon_wartet(void) {
  return s_hat_wartende;
}

/**
 * Eine kurze Zustandsmeldung - ohne Nachfassen.
 *
 * Sie darf ausfallen: geht der Start verloren, fehlt die Strecke, und das ist
 * aergerlich, aber nicht schlimm. Die Zusammenfassung am Ende ist die
 * Nachricht, die ankommen MUSS - die faellt deshalb nicht in denselben
 * Postausgang, sondern wartet und fasst nach.
 */
void telefon_melde_zustand(Trainingsmeldung was, uint8_t art, uint32_t beginn) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int32(out, MESSAGE_KEY_ZUSTAND, (int32_t)was);
  dict_write_int32(out, MESSAGE_KEY_ART, (int32_t)art);
  // DER BEGINN MUSS SCHON HIER MIT. Das Telefon legt die Spurdatei unter
  // diesem Zeitpunkt ab; erfuehre es ihn erst mit der Zusammenfassung,
  // haette es die Punkte unter einem anderen Namen gesammelt.
  dict_write_int32(out, MESSAGE_KEY_BEGINN, (int32_t)beginn);
  s_letzte_war_zusammenfassung = false;
  app_message_outbox_send();
}

void telefon_init(void) {
  app_message_register_inbox_received(prv_inbox);
  app_message_register_outbox_failed(prv_abgelehnt);
  app_message_register_outbox_sent(prv_angekommen);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);

  // Liegt noch eine unbestaetigte Zusammenfassung da, geht sie jetzt - mit
  // etwas Abstand, damit die Verbindung zum Telefon erst steht.
  if (prv_wartende_laden()) {
    s_hat_wartende = true;
    APP_LOG(APP_LOG_LEVEL_INFO, "Unbestaetigte Zusammenfassung - schicke erneut");
    if (!s_nachfassen) s_nachfassen = app_timer_register(1500, prv_nachfassen, NULL);
  }
}
