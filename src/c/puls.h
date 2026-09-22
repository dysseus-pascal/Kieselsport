#pragma once
#include <pebble.h>

// Puls und Pulszonen.
//
// EINE ZONE IST EIN ANTEIL DES MAXIMALPULSES, und der ist persönlich. 220
// minus Alter ist eine Faustformel mit einer Streuung von gut zehn Schlägen;
// deshalb steht der Wert in den Einstellungen und nicht in einer Formel.
//
// DIE MESSRATE BLEIBT AKTIV, NACHDEM DIE APP ENDET. So steht es in der
// Dokumentation von health_service_set_heart_rate_sample_period, und es ist
// die teuerste Falle dieser App: wer sie beim Beenden nicht zurücksetzt, misst
// noch stundenlang im Sekundentakt weiter und saugt den Akku leer. Deshalb
// gibt es zu puls_dicht_messen() ein puls_normal_messen(), und beide stehen
// nebeneinander an jeder Stelle, an der ein Training beginnt oder endet.

#define KS_ZONEN 5

void puls_init(void);

// Der eingestellte Maximalpuls. Vorgabe, bis das Telefon einen schickt.
uint16_t puls_maximum(void);
void puls_setze_maximum(uint16_t schlaege);

// In welcher Zone ein Wert liegt: 0 = unter Zone 1, 1..5 = Zone.
int puls_zone(uint16_t bpm);

// Untergrenze einer Zone in Schlägen, für die Anzeige.
uint16_t puls_zonengrenze(int zone);

// Farbe der Zone. Auf schwarzweissen Uhren immer schwarz - dort trägt die
// Zahl die Aussage, nicht die Farbe.
GColor puls_zonenfarbe(int zone);

// Dichte Messung für die Dauer eines Trainings an- und wieder abschalten.
void puls_dicht_messen(void);
void puls_normal_messen(void);
