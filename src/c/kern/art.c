#include "art.h"

// Die Reihenfolge ist die des Menüs UND die Zahl, die ans Telefon geht.
// Wer hier eine Zeile dazwischenschiebt, verschiebt jede gespeicherte
// Aufzeichnung um eine Art — aus Wandern wird Kraft, rückwirkend.
static const ArtInfo s_arten[ArtAnzahl] = {
  // Schritte  Distanz  Reps   Bahnen
  { true,     true,    false, false },   // Laufen
  // STRASSE UND GRAVEL SIND DASSELBE. Sie unterscheiden sich im Reifen, nicht
  // in dem, was diese Uhr davon sieht: Puls, Zeit, Kalorien. Was sich wirklich
  // anders anfuehlt, ist das Gelaende - und dafuer steht MTB daneben.
  //
  // Was beiden fehlt: Schritte sind am Lenker eine Zufallszahl (die Hand
  // wackelt), und ohne GPS gibt es keine Strecke. Bleiben Zeit, Puls und
  // Kalorien - wenig, aber wahr.
  { false,    false,   false, false },   // Strasse/Gravel
  { true,     true,    false, false },   // Wandern
  // KRAFT: weder Schritte noch Strecke. Dafuer zaehlt die Uhr hier
  // Wiederholungen und misst die Pause zwischen den Saetzen - und die Pause
  // ist die Zahl, auf die man schaut.
  { false,    false,   true,  false },   // Kraft
  { false,    false,   false, false },   // MTB
  // YOGA: die Uhr zaehlt hier gar nichts Bewegtes, und das ist richtig so.
  // Was bleibt, ist der Puls - und der ist bei Yoga die ganze Aussage.
  { false,    false,   false, false },   // Yoga
  // SCHWIMMEN: Schritte und Schrittdistanz sind im Wasser Unsinn. Die
  // Strecke entsteht stattdessen aus gezaehlten Bahnen mal Beckenlaenge -
  // die einzige Distanz in dieser App, die ohne GPS auskommt.
  { false,    false,   false, true  },   // Schwimmen
};

const ArtInfo *art_info(Sportart art) {
  // Kein Test auf < 0: der Aufzaehlungstyp ist vorzeichenlos, und der
  // Uebersetzer weist eine Bedingung zurueck, die nie zutreffen kann.
  if (art >= ArtAnzahl) return &s_arten[0];
  return &s_arten[art];
}

#ifndef KS_WORKER
// NAMEN IN DER SPRACHE DER UHR - nur in der App. Die Texttabelle liegt in
// src/c/strings.c, und die bekommt der Worker nicht: er zeigt nichts an, und
// jeder Text kostete ihn Platz im knappen Worker-Speicher. Dieselbe
// Reihenfolge wie s_arten oben.
#include "../strings.h"

static const StringId s_namen[ArtAnzahl] = {
  STR_ART_LAUFEN, STR_ART_BIKE, STR_ART_WANDERN, STR_ART_KRAFT,
  STR_ART_MTB, STR_ART_YOGA, STR_ART_SCHWIMMEN,
};

const char *art_name(Sportart art) {
  if (art >= ArtAnzahl) return "?";
  return S(s_namen[art]);
}

const char *art_gruppe(Sportart art) {
  // Strasse/Gravel und MTB sind beide Velo - das sagt der Obertitel.
  if (art == ArtBike || art == ArtMTB) return S(STR_GRUPPE_BIKE);
  return NULL;
}
#endif
