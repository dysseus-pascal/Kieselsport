#pragma once
#include <pebble.h>
#include "art.h"

// Der laufende Schirm - die Anzeige zu dem, was der Worker misst.
//
// DIE BEDIENUNG:
//   Select   Start - dann Pause und Weiter im Wechsel
//   Oben     in der Pause: speichern
//   Unten    in der Pause: verwerfen (zweimal, damit kein Fehlgriff ein
//            Training kostet)
//   Zurueck  die App verlassen; das Training laeuft im Hintergrund weiter,
//            und das Startmenue zeigt es unter Kieselsport an

// Mit einer gewaehlten Art: der Schirm steht bereit, Select startet.
void lauf_window_zeige(Sportart art);
// Der Worker laeuft schon (die App wurde neu geoeffnet): gleich hinein.
void lauf_window_zeige_laufend(void);
// Eine Nachricht vom Worker - Stand, fertig, verworfen.
void lauf_window_nachricht(uint16_t typ, AppWorkerMessage *daten);

// Fuer den Glance beim Verlassen: laeuft etwas, und was?
bool lauf_window_laeuft(void);
Sportart lauf_window_art(void);
uint32_t lauf_window_beginn(void);
