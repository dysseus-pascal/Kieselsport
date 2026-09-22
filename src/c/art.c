#include "art.h"

// Die Reihenfolge ist die des Menüs UND die Zahl, die ans Telefon geht.
// Wer hier eine Zeile dazwischenschiebt, verschiebt jede gespeicherte
// Aufzeichnung um eine Art — aus Wandern wird Kraft, rückwirkend.
static const ArtInfo s_arten[ArtAnzahl] = {
  // Name              Gruppe  Schritte  Distanz  Erholung
  { "Laufen",          NULL,   true,     true,    false },
  // STRASSE UND GRAVEL SIND DASSELBE. Sie unterscheiden sich im Reifen, nicht
  // in dem, was diese Uhr davon sieht: Puls, Zeit, Kalorien. Was sich wirklich
  // anders anfuehlt, ist das Gelaende - und dafuer steht MTB daneben.
  //
  // Was beiden fehlt: Schritte sind am Lenker eine Zufallszahl (die Hand
  // wackelt), und ohne GPS gibt es keine Strecke. Bleiben Zeit, Puls und
  // Kalorien - wenig, aber wahr.
  { "Strasse/Gravel",  "Bike", false,    false,   false },
  { "Wandern",         NULL,   true,     true,    false },
  // KRAFT: weder Schritte noch Strecke. Dafuer ist hier die Pause zwischen
  // den Saetzen die Zahl, auf die man schaut.
  { "Kraft",           NULL,   false,    false,   true  },
  { "MTB",             "Bike", false,    false,   false },
  // YOGA: die Uhr zaehlt hier gar nichts Bewegtes, und das ist richtig so.
  // Was bleibt, ist der Puls - und der ist bei Yoga die ganze Aussage.
  { "Yoga",            NULL,   false,    false,   false },
};

const ArtInfo *art_info(Sportart art) {
  // Kein Test auf < 0: der Aufzaehlungstyp ist vorzeichenlos, und der
  // Uebersetzer weist eine Bedingung zurueck, die nie zutreffen kann.
  if (art >= ArtAnzahl) return &s_arten[0];
  return &s_arten[art];
}

const char *art_name(Sportart art) {
  if (art >= ArtAnzahl) return "?";
  return s_arten[art].name;
}

const char *art_gruppe(Sportart art) {
  if (art >= ArtAnzahl) return NULL;
  return s_arten[art].gruppe;
}
