#pragma once
#include "plattform.h"

// Die Sportarten - und was bei jeder davon überhaupt eine Aussage ist.
//
// NICHT JEDE KENNZAHL PASST ZU JEDER ART. Schritte auf dem Bike sind eine
// Zufallszahl, und eine Distanz kann die Uhr ohne GPS nur aus Schritten
// schätzen — beim Krafttraining ist beides sinnlos. Eine App, die überall
// dieselben vier Felder zeigt, behauptet Messungen, die es nicht gibt.
//
// Deshalb trägt jede Art mit, was sie kann. Der laufende Schirm zeigt nur das.
//
// NEUE ARTEN KOMMEN HINTEN DAZU, niemals dazwischen. Die Zahl ist das, was
// ans Telefon geht und dort in der Gesundheitsakte steht; eine eingeschobene
// Zeile machte aus jedem gespeicherten Wandern rückwirkend ein Krafttraining.
// Deshalb stehen MTB und Yoga am Ende und nicht neben Strasse/Gravel, wo MTB
// hingehörte.
//
// DIE EINZIGE AUSNAHME WAR 0.3.0. Die hatte Strasse und Gravel getrennt und
// dazwischen eine Zahl vergeben; sie stand zwanzig Minuten lang im Netz, und
// in dieser Zeit wurde nichts damit aufgezeichnet. Wer die Fassung doch
// benutzt hat, findet ein MTB als Gravel und ein Yoga als MTB wieder.

typedef enum {
  ArtLaufen = 0,
  ArtBike,      //< Strasse UND Gravel - dasselbe; hiess bis 0.2.0 "Velo"
  ArtWandern,
  ArtKraft,
  ArtMTB,
  ArtYoga,
  ArtSchwimmen,
  ArtAnzahl,
} Sportart;

// Was eine Art an Kennzahlen hergibt. Zeit und Puls können alle.
typedef struct {
  const char *name;
  const char *gruppe;  //< Obertitel im Menü, oder NULL
  bool schritte;       //< Schritte zählen sinnvoll?
  bool distanz;        //< Strecke aus Schritten ableitbar?
  bool reps;           //< Wiederholungen und Pausen zählen?
  bool bahnen;         //< Bahnen über den Kompass zählen?
} ArtInfo;

const ArtInfo *art_info(Sportart art);

// Der Name, auch für eine Art ausserhalb des Bereichs (dann "?").
const char *art_name(Sportart art);

// Der Obertitel im Menü - "Bike" bei den beiden, sonst NULL.
const char *art_gruppe(Sportart art);
