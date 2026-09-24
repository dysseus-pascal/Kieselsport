#pragma once
#include <pebble.h>

// Die Einstellungen aus der Konfigseite, in den Persist geschrieben.
//
// SIE KOMMEN IN DER APP AN UND WERDEN IM WORKER GEBRAUCHT. Der Worker liest
// sie beim Start eines Trainings aus dem Persist; die App schreibt sie nur
// hinein. Deshalb stehen die Setzer hier und nicht in den Modulen des Kerns,
// die die App gar nicht mehr enthaelt.

void einstellungen_maxpuls(int32_t schlaege);
void einstellungen_becken(int32_t meter);
void einstellungen_pausenziel(int32_t sekunden);
void einstellungen_empfindlichkeit(int32_t stufe);
void einstellungen_pin_art(int32_t art);
void einstellungen_pin_zeit(const char *hhmm);
void einstellungen_nacht_an(bool an);
void einstellungen_nacht_von(int32_t minuten);   //< seit Mitternacht
void einstellungen_nacht_bis(int32_t minuten);

// ALLE EINSTELLUNGEN IN EINE NACHRICHT - fuer das Telefon.
//
// DIE UHR IST DIE EINE STELLE, AN DER SIE GELTEN. Geaendert werden sie auf
// der Konfigseite der Pebble-App oder in Kiesel-Helper; beide schicken an die
// Uhr, und die Uhr meldet danach, was gilt. Ohne diese Meldung zeigte jede
// Seite ihren eigenen, womoeglich alten Stand.
void einstellungen_melden(DictionaryIterator *out);
