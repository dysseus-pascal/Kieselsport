#pragma once
#include <pebble.h>

// Die Sportarten - und was bei jeder davon überhaupt eine Aussage ist.
//
// NICHT JEDE KENNZAHL PASST ZU JEDER ART. Schritte beim Velofahren sind eine
// Zufallszahl, und eine Distanz kann die Uhr ohne GPS nur aus Schritten
// schätzen — beim Krafttraining ist beides sinnlos. Eine App, die überall
// dieselben vier Felder zeigt, behauptet Messungen, die es nicht gibt.
//
// Deshalb trägt jede Art mit, was sie kann. Der laufende Schirm zeigt nur das.

typedef enum {
  ArtLaufen = 0,
  ArtVelo,
  ArtWandern,
  ArtKraft,
  ArtAnzahl,
} Sportart;

// Was eine Art an Kennzahlen hergibt. Zeit und Puls können alle.
typedef struct {
  const char *name;
  bool schritte;     //< Schritte zählen sinnvoll?
  bool distanz;      //< Strecke aus Schritten ableitbar?
  bool erholung;     //< Erholungszeit zwischen Sätzen anbieten?
} ArtInfo;

const ArtInfo *art_info(Sportart art);

// Der Name, auch für eine Art ausserhalb des Bereichs (dann "?").
const char *art_name(Sportart art);
