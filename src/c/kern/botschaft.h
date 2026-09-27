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
  BotStand3,       //< data0 = Saetze (Yoga: HRV in ms), data1 = Wiederholungen gesamt (Yoga: Intervalle), data2 = laufende | ruht << 15
  BotStand4,       //< data0 = Pause in s, data1 = Bahnen, data2 = Kompass bereit | sparsam << 1 | Brummen << 4 | Zone << 8
  BotStand5,       //< data0 = Beginn hoch, data1 = Beginn tief, data2 = Maximalpuls
  BotFertig,       //< gespeichert - die Zusammenfassung liegt im Persist (wartend.h)
  BotVerworfen,    //< beendet, ohne Zusammenfassung
  BotMinute,       //< ein Stueck der letzten Minute, siehe unten
};

// DIE LETZTE MINUTE fuer die Pulskurve der App. Der Worker merkt sich den
// Puls jeder Trainingssekunde auf dem Platz Sekunde % 60; die App fuehrt
// dieselbe Liste aus BotStand1 weiter. Nur wer die App eben erst oeffnet,
// hat die Minute davor nicht - die schickt der Worker nach, in Stuecken zu
// fuenf Werten: data0 = Platz | Wert << 8, data1 und data2 je zwei Werte.
// 0 heisst: kein frischer Puls, die Kurve hat dort eine Luecke.
#define KS_MINUTE_S 60
#define KS_MINUTE_JE 5

// Was zu brummen ist - im Feld "Brummen" von BotStand4. DER WORKER DARF
// NICHT BRUMMEN, die App schon; also sagt er ihr, was faellig ist. Schaut
// gerade niemand zu, holt er die App fuer einen Zonenwechsel nach vorn.
enum {
  BrummNichts = 0,
  BrummZoneHoch = 1,   //< zwei kurze
  BrummZoneRunter = 2, //< eine
  BrummPauseUm = 3,    //< Kraft: die Pause ist um
};

// Nach so vielen Sekunden ohne neues Abo hoert der Worker auf zu senden:
// dann schaut niemand zu, und jede Nachricht kostete Strom fuer nichts.
#define KS_ABO_S 5

static inline uint16_t bot_kappe(uint32_t wert) {
  return wert > 0xFFFF ? 0xFFFF : (uint16_t)wert;
}
