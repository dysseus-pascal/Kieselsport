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

#define PERSIST_ARCHIV_BASIS 100
