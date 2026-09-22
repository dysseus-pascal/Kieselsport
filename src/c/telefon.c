#include "telefon.h"
#include "puls.h"

// Zehn Zahlen plus Kopf. 256 lässt Luft für eine Einstellung mehr.
#define INBOX_SIZE 256
#define OUTBOX_SIZE 256

// Ein zweiter Anlauf, falls der Postausgang gerade besetzt ist. Er fasst
// genau EINE Nachricht; kommt die Zusammenfassung zu dicht hinter etwas
// anderem, fiele sie still mit BUSY aus - und das Training wäre weg.
static AppTimer *s_nachfassen;
static Trainingsstand s_wartet;
static bool s_hat_wartende;

static void prv_sende_jetzt(void);

static void prv_nachfassen(void *data) {
  s_nachfassen = NULL;
  if (s_hat_wartende) prv_sende_jetzt();
}

static void prv_inbox(DictionaryIterator *iter, void *context) {
  Tuple *max = dict_find(iter, MESSAGE_KEY_MAXPULS);
  if (max) {
    const int32_t wert = max->type == TUPLE_CSTRING
        ? atoi(max->value->cstring) : max->value->int32;
    puls_setze_maximum((uint16_t)wert);
  }
}

static void prv_abgelehnt(DictionaryIterator *iter, AppMessageResult grund, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht abgelehnt: %d", (int)grund);
  if (s_hat_wartende && !s_nachfassen) {
    s_nachfassen = app_timer_register(2000, prv_nachfassen, NULL);
  }
}

static void prv_angekommen(DictionaryIterator *iter, void *context) {
  s_hat_wartende = false;
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
  // DAS ENDE STEHT IN DERSELBEN NACHRICHT. Eine eigene Stopmeldung daneben
  // straeubte sich mit dieser um den Postausgang - der fasst genau EINE
  // Nachricht, und die zweite fiele mit BUSY aus.
  dict_write_int32(out, MESSAGE_KEY_ZUSTAND, (int32_t)ZustandStop);
  app_message_outbox_send();
}

void telefon_sende(const Trainingsstand *t) {
  s_wartet = *t;
  s_hat_wartende = true;
  prv_sende_jetzt();
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
  app_message_outbox_send();
}

void telefon_init(void) {
  app_message_register_inbox_received(prv_inbox);
  app_message_register_outbox_failed(prv_abgelehnt);
  app_message_register_outbox_sent(prv_angekommen);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);
}
