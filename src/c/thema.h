#pragma once
#include <pebble.h>

// Das Timeline-Design: weisser Grund, schwarze Schrift, rechts eine farbige
// Leiste mit dem Herz oben und den Tasten-Hinweisen auf Tastenhoehe - so,
// wie Drinktervall und Flynformer aussehen, damit die drei Apps auf einer
// Uhr wie EINE Familie wirken.
//
// WARUM HELL: das Pebble-Display leuchtet nicht, es reflektiert. Beim Laufen
// in der Sonne ist Schwarz auf Weiss das Einzige, was man im Vorbeischwingen
// noch liest.
//
//  GRUND        Hintergrund aller Schirme, TEXT die Schrift darauf, NEBEN
//               die Beschriftung der kleinen Felder.
//  LEISTE       Die Seitenleiste rechts, AUF_LEISTE die Schrift darin. Auf
//               Schwarzweiss schwarz mit weisser Schrift - ein weisses Band
//               verschwaende im weissen Grund.
//  Die Zonen    tragen ihre eigenen Farben (thema_zonenfarbe); sie fuellen
//               das Herz in der Leiste und den Balken unter dem Puls.
//
// Alle Farben aus der 64er-Palette: ImperialPurple #550055, DarkGray #555555.
// Violett, weil Blau (Drinktervall) und Orange (Flynformer) vergeben sind und
// keine Pulszone violett ist - das Herz hebt sich in jeder Zone davon ab.
#define KS_FARBE_GRUND       GColorWhite
#define KS_FARBE_TEXT        GColorBlack
#define KS_FARBE_NEBEN       PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)
#define KS_FARBE_LEISTE      PBL_IF_COLOR_ELSE(GColorImperialPurple, GColorBlack)
#define KS_FARBE_AUF_LEISTE  GColorWhite

// Die Masse, je Schirm. Auf der runden Uhr ist die Leiste breiter, weil der
// Kreis den groessten Teil davon abschneidet: sichtbar bleiben etwa 9 Punkte
// mehr als die Haelfte, und dort muessen Herz und Hinweise stehen.
#define KS_LEISTE_B   PBL_IF_ROUND_ELSE(51, (PBL_DISPLAY_WIDTH >= 180 ? 34 : 30))
#define KS_LEISTE_DX  PBL_IF_ROUND_ELSE(9, 0)
#define KS_RAND       PBL_IF_ROUND_ELSE(38, 9)
#define KS_BREIT      (PBL_DISPLAY_WIDTH >= 180)

// Die Farbe einer Pulszone (1..5); 0 und alles andere: Grau.
GColor thema_zonenfarbe(int zone);

// Was neben einer Taste steht: ein Zeichen, kein Wort. Die Leiste ist dreissig
// Punkte breit, da passt "Speichern" nicht hinein - eine Diskette schon, und
// die versteht man auch im Vorbeischwingen.
typedef enum {
  SymbolKeins = 0,   //< die Taste tut gerade nichts
  SymbolStart,       //< Dreieck: los
  SymbolPause,       //< zwei Balken
  SymbolSpeichern,   //< Diskette
  SymbolLoeschen,    //< Abfalleimer
  SymbolLoeschenFrage, //< Abfalleimer mit Fragezeichen: nochmal druecken
  SymbolWahl,        //< Winkel nach rechts: auswaehlen
} Symbol;

// Die Leiste rechts ueber die volle Hoehe von `b`: das Herz oben - hohl,
// solange kein Puls da ist, sonst in `herz` gefuellt - und darunter bis zu
// drei Zeichen (Oben, Select, Unten) auf der Hoehe der Tasten. SymbolKeins
// laesst eine Taste stumm: was nichts tut, wird nicht angeschrieben.
void thema_leiste(GContext *ctx, GRect b, bool herz_voll, GColor herz,
                  Symbol oben, Symbol mitte, Symbol unten);
