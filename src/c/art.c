#include "art.h"

// Die Reihenfolge ist die des Menüs UND die Zahl, die ans Telefon geht.
// Wer hier eine Zeile dazwischenschiebt, verschiebt jede gespeicherte
// Aufzeichnung um eine Art — aus Wandern wird Kraft, rückwirkend.
static const ArtInfo s_arten[ArtAnzahl] = {
  // Name        Schritte  Distanz  Erholung
  { "Laufen",    true,     true,    false },
  // VELO: Schritte sind hier eine Zufallszahl (die Hand am Lenker wackelt),
  // und ohne GPS gibt es keine Strecke. Bleiben Zeit, Puls und Kalorien -
  // wenig, aber wahr.
  { "Velo",      false,    false,   false },
  { "Wandern",   true,     true,    false },
  // KRAFT: weder Schritte noch Strecke. Dafuer ist hier die Pause zwischen
  // den Saetzen die Zahl, auf die man schaut.
  { "Kraft",     false,    false,   true  },
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
