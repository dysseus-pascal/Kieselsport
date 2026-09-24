#pragma once

// Die Persist-Schluessel - an EINER Stelle, weil App und Worker denselben
// Speicher teilen. Wer hier zwei Dinge auf dieselbe Nummer legt, ueberschreibt
// aus dem Worker heraus, was die App gerade gespeichert hat.
#define PERSIST_MAXPULS 1
#define PERSIST_EMPFIND 2
#define PERSIST_PAUSENZIEL 3
#define PERSIST_BECKEN 4
// Der Timeline-Pin: welche Art, wann. Die Uhr haelt sie, damit Konfigseite
// und Kiesel-Helper denselben Stand sehen (siehe einstellungen.h).
#define PERSIST_PIN_ART 5
#define PERSIST_PIN_ZEIT 6

// Die Nacht: an oder aus, und das Zeitfenster in Minuten seit Mitternacht.
// Gesetzt von Konfigseite oder Kiesel-Helper (einstellungen.h), gelesen vom
// Worker jede Minute.
#define PERSIST_NACHT_AN 7
#define PERSIST_NACHT_VON 8
#define PERSIST_NACHT_BIS 9

#define PERSIST_ARCHIV_ANZAHL 10
#define PERSIST_ARCHIV_NAECHST 11

// Der Startbefehl an den Worker: Art und Beginn. Die App schreibt sie, bevor
// sie den Worker startet; der liest und loescht sie.
#define PERSIST_START_ART 20
#define PERSIST_START_BEGINN 21

// Die Zusammenfassung, die auf das Telefon wartet (siehe wartend.h).
#define PERSIST_WARTET 30
#define PERSIST_WARTET_LISTE_A 31
#define PERSIST_WARTET_LISTE_B 32

// Die Pulskurve, die auf das Telefon wartet (siehe kurve.h): Anzahl,
// Beginn, wie weit sie schon drueben ist, und die Werte in Stuecken ab 50.
#define PERSIST_KURVE_ANZAHL 40
#define PERSIST_KURVE_BEGINN 41
#define PERSIST_KURVE_AB 42
#define PERSIST_KURVE_BASIS 50

// Die Nacht, die der Worker gerade misst (LAUF_*), und die fertige, die auf
// das Telefon wartet (OFFEN_*) - siehe nacht.h.
#define PERSIST_NACHT_LAUF_BEGINN 60
#define PERSIST_NACHT_LAUF_HRV 61
#define PERSIST_NACHT_OFFEN_BEGINN 63
#define PERSIST_NACHT_OFFEN_ENDE 64
#define PERSIST_NACHT_OFFEN_HRV 65
#define PERSIST_NACHT_OFFEN_AB 66

// Laeuft ein Training? Der Worker schreibt es beim Start und beim Ende. Seit
// er dauerhaft laeuft (fuer die Nacht), sagt "Worker laeuft" das nicht mehr.
#define PERSIST_LAEUFT 67

#define PERSIST_ARCHIV_BASIS 100
