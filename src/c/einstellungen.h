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
