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
//
// NEUE ARTEN KOMMEN HINTEN DAZU, niemals dazwischen. Die Zahl ist das, was
// ans Telefon geht und dort in der Gesundheitsakte steht; eine eingeschobene
// Zeile machte aus jedem gespeicherten Wandern rückwirkend ein Krafttraining.
// Deshalb stehen Gravel und MTB am Ende und nicht neben Strasse, wo sie
// hingehörten.

typedef enum {
  ArtLaufen = 0,
  ArtStrasse,   //< hiess bis 0.3.0 schlicht "Velo"
  ArtWandern,
  ArtKraft,
  ArtGravel,
  ArtMTB,
  ArtYoga,
  ArtAnzahl,
} Sportart;

// Was eine Art an Kennzahlen hergibt. Zeit und Puls können alle.
typedef struct {
  const char *name;
  const char *gruppe;  //< Obertitel im Menü, oder NULL
  bool schritte;       //< Schritte zählen sinnvoll?
  bool distanz;        //< Strecke aus Schritten ableitbar?
  bool erholung;       //< Erholungszeit zwischen Sätzen anbieten?
} ArtInfo;

const ArtInfo *art_info(Sportart art);

// Der Name, auch für eine Art ausserhalb des Bereichs (dann "?").
const char *art_name(Sportart art);

// Der Obertitel im Menü - "Velo" bei den dreien, sonst NULL.
const char *art_gruppe(Sportart art);
