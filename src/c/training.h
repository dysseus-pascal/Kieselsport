#pragma once
#include <pebble.h>
#include "art.h"

// Ein Training: was gerade läuft, und was davon übrig bleibt.
//
// DIE SUMMEN WERDEN NICHT MITGEZÄHLT, SONDERN AM ENDE ABGEZOGEN. Die Uhr
// führt Schritte, Distanz und Kalorien ohnehin über den ganzen Tag; ein
// Training ist die Differenz zwischen Anfang und Ende. Selbst zu zählen
// hiesse, dieselbe Messung ein zweites Mal zu machen - mit eigenen Fehlern.
//
// Ausnahme ist der Puls: der ist ein Augenblickswert und kein Zähler. Er wird
// aufsummiert, solange das Training läuft.

typedef enum {
  LaufAus = 0,
  LaufLaeuft,
  LaufPause,
} Laufzustand;

typedef struct {
  uint8_t art;
  uint32_t beginn;        //< Unix-Sekunden
  uint32_t dauer_s;       //< ohne Pausen
  uint32_t schritte;
  uint32_t meter;
  uint32_t kcal;
  uint16_t puls_mittel;
  uint16_t puls_max;
} Trainingsstand;

void training_init(void);
void training_beenden_ganz(void);

Laufzustand training_zustand(void);
Sportart training_art(void);

void training_starte(Sportart art);
void training_pause_umschalten(void);
// Beendet das Training und legt es ins Archiv. Gibt den Stand zurueck.
Trainingsstand training_stoppe(void);

// Der laufende Stand, jederzeit abfragbar (auch in der Pause).
Trainingsstand training_stand(void);

// Sekunde fuer Sekunde vom Zeitgeber gerufen.
void training_tick(void);

// Aktueller Puls, 0 wenn keiner zu haben ist.
uint16_t training_puls(void);

// --- Archiv ---

#define KS_ARCHIV_MAX 20

int training_archiv_anzahl(void);
// index 0 ist das juengste Training.
bool training_archiv_lesen(int index, Trainingsstand *aus);
