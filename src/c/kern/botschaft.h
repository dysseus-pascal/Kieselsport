#pragma once
#include "plattform.h"

// Was App und Worker einander sagen.
//
// EINE NACHRICHT SIND DREI ZAHLEN ZU 16 BIT, mehr gibt der Weg nicht her.
// Deshalb geht der Stand in mehreren Stuecken hinueber, jede Sekunde neu,
// und die App setzt sie wieder zusammen. Was nicht hineinpasst, wird
// gepackt: Meter in Zehnern, Zustand und Art in einem Feld.

// --- App -> Worker ---
enum {
  BefehlAbo = 20,        //< "Ich schaue zu" - Stand schicken, und weiter jede Sekunde
  BefehlStart,           //< Start nach PERSIST_START_*, falls der Worker schon lief
  BefehlPause,           //< Pause und Weiter, im Wechsel
  BefehlSpeichern,       //< Training beenden und zum Telefon
  BefehlVerwerfen,       //< Training beenden und vergessen
};

// --- Worker -> App ---
enum {
  BotStand1 = 1,   //< data0 = zustand | art << 4, data1 = Sekunden, data2 = Puls | frisch << 15
  BotStand2,       //< data0 = Schritte, data1 = Meter / 10, data2 = kcal
  BotStand3,       //< data0 = Saetze, data1 = Wiederholungen gesamt, data2 = laufende | ruht << 15
  BotStand4,       //< data0 = Pause in s, data1 = Bahnen, data2 = Kompass bereit | Zone << 8
  BotStand5,       //< data0 = Beginn hoch, data1 = Beginn tief, data2 = Maximalpuls
  BotFertig,       //< gespeichert - die Zusammenfassung liegt im Persist (wartend.h)
  BotVerworfen,    //< beendet, ohne Zusammenfassung
};

// Nach so vielen Sekunden ohne neues Abo hoert der Worker auf zu senden:
// dann schaut niemand zu, und jede Nachricht kostete Strom fuer nichts.
#define KS_ABO_S 5

static inline uint16_t bot_kappe(uint32_t wert) {
  return wert > 0xFFFF ? 0xFFFF : (uint16_t)wert;
}
