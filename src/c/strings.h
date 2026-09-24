#pragma once
#include <pebble.h>

// Texte der Oberflaeche. Die Uhr gibt die Sprache vor
// (Settings -> Display -> Language); die App folgt ihr, es gibt keinen eigenen
// Sprachschalter. Alle Texte stehen in strings_table.h, eine Zeile je Text.
// Dasselbe Muster wie in den Schwester-Apps (Drinktervall).
//
// Rueckfall ist ENGLISCH: die Uhr kennt weitere eingebaute Sprachen, fuer die
// wir keine Spalte haben (Nederlands, Portugues, Polski ...). Eine deutsche
// Oberflaeche auf einer polnischen Uhr waere schlechter als eine englische.
//
// NUR DIE APP, NICHT DER WORKER. Der Worker zeigt nichts an und bekommt
// weder diese Datei noch strings.c; im Kern (art.c) steht der Zugriff
// deshalb hinter #ifndef KS_WORKER.

typedef enum {
  STRINGS_EN = 0,   //< Spalte 0, zugleich der Rueckfall
  STRINGS_DE,
  STRINGS_FR,
  STRINGS_IT,
  STRINGS_ES,
  STRINGS_LANG_COUNT,
} StringLang;

typedef enum {
#define STR(id, maxbytes, en, de, fr, it, es) id,
#include "strings_table.h"
#undef STR
  STR_COUNT,
} StringId;

// Liefert den Text in der aktuellen Sprache. Nie NULL: ein unbekannter
// Schluessel und eine fehlende Spalte fallen auf Englisch zurueck, eine
// fehlende englische Spalte auf den leeren String.
const char *S(StringId id);

// Sprache neu von der Uhr lesen. i18n_get_system_locale() ist ein Syscall und
// es gibt kein Ereignis fuer einen Sprachwechsel - also einmal beim Start.
void strings_refresh(void);
