#pragma once
#include <pebble.h>
#include "training.h"

// Der Draht zum Telefon.
//
// ZWEI RICHTUNGEN, ZWEI ZWECKE. Vom Telefon kommt die Einstellung
// (Maximalpuls, Becken, Pause, Empfindlichkeit) aus der Konfigseite. Zur Uhr
// hinaus geht die Zusammenfassung eines Trainings - und die hört NICHT nur
// die eigene pkjs-Seite: die Pebble-App reicht jede eingehende Nachricht auch
// an klassische Companion-Apps weiter. Kiesel-Helper trägt sie von dort in
// die Gesundheitsakte ein, ohne dass diese App etwas davon wissen muss.
//
// KEIN companionApp-Eintrag in der package.json. Ein Paketname dort schaltet
// die Pebble-App auf PebbleKit2 um, und damit fällt genau dieser
// Nebenempfänger weg.
//
// NUR DIE APP REDET MIT DEM TELEFON. Der Worker kann es nicht (kein
// AppMessage im Hintergrund); er legt die Zusammenfassung in den Persist,
// und telefon_nachsenden() holt sie von dort.

void telefon_init(void);

// Die wartende Zusammenfassung aus dem Persist schicken, falls eine da ist.
// Fasst nach, bis das Telefon sie bestaetigt.
void telefon_nachsenden(void);

// Was gerade geschieht - damit das Telefon weiss, wann es mitschreiben soll.
//
// DIE UHR HAT KEIN GPS. Die Strecke kann nur das Telefon aufzeichnen, und das
// muss dafuer wissen, dass ein Training laeuft. Ohne diese Meldung erfuehre es
// erst am Ende davon - und haette nichts aufgezeichnet.
typedef enum {
  ZustandStop = 0,
  ZustandStart = 1,
  ZustandPause = 2,
  ZustandWeiter = 3,
} Trainingsmeldung;

void telefon_melde_zustand(Trainingsmeldung was, uint8_t art, uint32_t beginn);
