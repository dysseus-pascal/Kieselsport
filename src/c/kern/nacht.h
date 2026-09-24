#pragma once
#include "plattform.h"

// Die Nacht: Schlaf und HRV, gemessen im Worker.
//
// WARUM HIER UND NICHT IN EINER EIGENEN APP: die Uhr erlaubt genau einen
// Hintergrund-Worker, und Kieselsport hat ihn - fuer das Training. Nachts
// trainiert niemand; also misst derselbe Worker in dieser Zeit den Schlaf.
// Herzintervall, das dafuer nur einzelne Wecker hatte, ist darin aufgegangen.
//
// WAS DER WORKER TUT, UND WAS NICHT. Bewegung und Puls je Minute zeichnet
// die Uhr ohnehin auf (HealthMinuteData); die liest morgens die App. Was die
// Uhr NICHT von sich aus misst, sind die Schlagabstaende - die HRV. Dafuer
// misst der Worker im Zeitfenster alle 30 Minuten fuenf Minuten lang die
// Intervalle und merkt sich den RMSSD. Dauernd zu messen kostete zu viel
// Akku, und fuenf Minuten sind die uebliche Laenge einer Kurzzeit-HRV.
//
// AUSGEWERTET WIRD AUF DEM TELEFON. Schlaf oder wach, Tiefschlaf und REM
// rechnet Kiesel-Helper aus den Minuten und den Fenstern - dort laesst sich
// ein Verfahren pruefen und nachstellen, auf der Uhr nicht.

#define KS_NACHT_TAKT_MIN 30        //< alle so viele Minuten ein HRV-Fenster
#define KS_NACHT_FENSTER_MIN 5      //< so lange misst es
#define KS_NACHT_FENSTER_MAX 24     //< zwoelf Stunden
#define KS_NACHT_MINUTEN_MAX 720    //< laenger wird keine Nacht geschickt
#define KS_NACHT_VON_VORGABE (22 * 60)
#define KS_NACHT_BIS_VORGABE (8 * 60)

// Ein HRV-Fenster, so wie es ans Telefon geht (4 Byte).
typedef struct __attribute__((packed)) {
  uint16_t minute;   //< ab Beginn der Nacht
  uint8_t rmssd;     //< ms, gekappt bei 255; 0 = zu wenig Intervalle
  uint8_t puls;      //< mittlerer Puls im Fenster, 0 = keiner
} NachtFenster;

// Die Einstellungen - aus dem Persist, mit Vorgaben.
bool nacht_an(void);
int nacht_von(void);   //< Minuten seit Mitternacht
int nacht_bis(void);
// Liegt diese Minute des Tages im Fenster? Ueber Mitternacht gedacht.
bool nacht_im_fenster(int minute_des_tages);

#ifdef KS_WORKER
// --- Worker ---
// Jede Minute, solange kein Training laeuft. Rueckgabe: true, wenn eben eine
// Nacht fertig geworden ist - dann holt der Worker die App, die sie schickt.
bool nacht_minute(time_t jetzt);
// Jede Sekunde waehrend eines HRV-Fensters (sonst nicht noetig).
bool nacht_misst(void);
// Ein Training beginnt: ein laufendes Fenster abbrechen.
void nacht_unterbrechen(void);
#else
// --- App ---
// Wartet eine fertige Nacht auf das Telefon?
bool nacht_wartet(void);
time_t nacht_beginn(void);
uint16_t nacht_anzahl(void);      //< Minuten
uint16_t nacht_ab(void);          //< so viele sind schon drueben
// Die Fenster der wartenden Nacht. Rueckgabe: Bytes.
uint16_t nacht_fenster(uint8_t *aus, uint16_t max);
// Die Minuten ab `ab`, hoechstens `anzahl`, je zwei Byte [bewegung, puls]
// (siehe Protokoll). Rueckgabe: wie viele Minuten.
uint16_t nacht_minuten(uint8_t *aus, uint16_t ab, uint16_t anzahl);
void nacht_bestaetigt(uint16_t minuten);
void nacht_vergessen(void);
#endif
