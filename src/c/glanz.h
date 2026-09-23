#pragma once
#include <pebble.h>
#include "art.h"

// Der Hinweis im Startmenue, waehrend ein Training im Hintergrund laeuft.
//
// DAS BANNER AUF DEM ZIFFERBLATT GIBT ES FUER FREMDE APPS NICHT. Was die
// Uhr einer App laesst, ist die Zeile unter ihrem Namen im Startmenue - der
// App Glance. Dort steht "Laufen - seit 25 min", und die Uhr zaehlt die Zeit
// selbst weiter; ein Tippen oeffnet den laufenden Schirm.

// Beim Verlassen der App aufrufen: mit Training den Hinweis setzen, ohne
// ihn loeschen.
void glanz_setzen(bool laeuft, Sportart art, uint32_t beginn);
