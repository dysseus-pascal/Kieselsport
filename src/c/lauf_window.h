#pragma once
#include <pebble.h>
#include "art.h"

// Der laufende Schirm. Er gehoert dem Training, nicht der Uhr: solange er
// steht, laeuft die Aufzeichnung.
void lauf_window_zeige(Sportart art);
