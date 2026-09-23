#pragma once
#include "plattform.h"

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

// Die Farbe der Zone steht NICHT hier, sondern in der App (lauf_window.c):
// der Worker kennt keine Farben, und dieser Kern wird fuer beide uebersetzt.

// Dichte Messung für die Dauer eines Trainings an- und wieder abschalten.
void puls_dicht_messen(void);
void puls_normal_messen(void);

// SPARSAM BEI SCHWACHEM AKKU. Unter KS_AKKU_SPARSAM Prozent misst die Uhr im
// Training alle fünf Sekunden statt jede - eine Stunde Sekundentakt kostet
// spürbar, und ein Training mit leerer Uhr ist keines. Der Worker liest den
// Akku beim Start und jede Minute und sagt es hier.
#define KS_AKKU_SPARSAM 20
void puls_setze_sparsam(bool sparsam);
bool puls_sparsam(void);

// --- HRV, für Yoga ---
//
// BEI YOGA IST DER PULS NICHT DIE AUSSAGE, die Herzratenvariabilität ist es.
// Die Uhr misst dann die Abstände zwischen zwei Schlägen (PPI) und rechnet
// daraus den RMSSD, wie Herzintervall es nachts tut. Nur solange ein Training
// läuft: HRV misst der Sensor nur auf ausdrückliche Bitte.
void puls_hrv_start(void);
void puls_hrv_stop(void);
// RMSSD in Millisekunden aus dem laufenden Training, 0 wenn zu wenig Schläge.
uint16_t puls_hrv_rmssd(void);
// Wie viele Intervalle bisher zählten.
uint16_t puls_hrv_anzahl(void);

// Den Sensor beobachten, solange ein Training läuft - damit sich sagen
// lässt, ob ein Wert FRISCH ist oder nur der letzte gute von vorhin.
//
// DER SENSOR BEHÄLT DEN LETZTEN GUTEN WERT. Am Lenker, bei kalter Haut oder
// lockerem Band misst er schlecht, und dann liefert HeartRateBPM keinen
// neuen Wert, sondern weiter den alten - eine halbe Stunde lang »75«, obwohl
// man bergauf tritt. Frisch heisst: der Sensor hat sich in den letzten
// KS_PULS_ALT_S Sekunden gemeldet oder der Wert hat sich geändert.
#define KS_PULS_ALT_S 90

void puls_beobachten(void);
void puls_ignorieren(void);

// Der aktuelle Wert vom Sensor, 0 wenn keiner zu haben ist.
uint16_t puls_lesen(void);
// Ob der zuletzt gelesene Wert frisch ist.
bool puls_frisch(void);
