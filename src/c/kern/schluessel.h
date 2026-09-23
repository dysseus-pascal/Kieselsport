#pragma once

// Die Persist-Schluessel - an EINER Stelle, weil App und Worker denselben
// Speicher teilen. Wer hier zwei Dinge auf dieselbe Nummer legt, ueberschreibt
// aus dem Worker heraus, was die App gerade gespeichert hat.
#define PERSIST_MAXPULS 1
#define PERSIST_EMPFIND 2
#define PERSIST_PAUSENZIEL 3
#define PERSIST_BECKEN 4

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

#define PERSIST_ARCHIV_BASIS 100
