// Alle Texte der Uhr-App, eine Zeile je Text.
//
// ACHTUNG, ZWEI DINGE SIND ABSICHT:
//  1. KEIN #pragma once und keine Include-Waechter. Diese Datei wird MEHRFACH
//     eingebunden (X-Makro): einmal fuer die Aufzaehlung der Schluessel in
//     strings.h und einmal fuer die Tabelle in strings.c. Ein Waechter wuerde
//     die zweite Einbindung verschlucken und eine leere Tabelle erzeugen.
//  2. Endung .h, obwohl es kein gewoehnlicher Header ist: Build-Umgebungen,
//     die nur .c und .h in ihren Baum kopieren, faenden sie sonst nicht.
//
//   STR(schluessel, maxbytes, en, de, fr, it, es)
//
//   maxbytes  Groesse des Zielpuffers in BYTES, 0 wenn der Text in keinen
//             festen Puffer kopiert wird. Umlaute und Akzente zaehlen als
//             zwei Bytes, "·" und "…" als zwei bzw. drei. Bei Vorlagen mit %s
//             muss auch das Ergebnis mit dem laengsten Artnamen passen.
//   en        Englisch. Spalte 0 und zugleich der Rueckfall.
//
// KURZ HALTEN. Die schmalste Spalte ist die auf flint: 144 Punkte minus
// Leiste und Rand, gut hundert Punkte - in Gothic 14 um die siebzehn Zeichen,
// im fetten Titel keine dreizehn. Was laenger ist, endet in "…". Franzoesisch
// und Spanisch sind oft laenger als Deutsch; dort lieber ein Wort weniger.
//
// Nicht hier und absichtlich nicht uebersetzt: "Kieselsport" (der Name),
// die Einheiten km, m, kcal und "HRV ms" (Fachbegriff).
//
// Die Texte der Timeline-Pins und der Konfigseite stehen NICHT hier, sondern
// in src/pkjs: sie werden auf dem Telefon gebaut.

// ---- Sportarten (Menue, Kopfzeile, Glance) --------------------------------
// Die Reihenfolge der Arten steht in kern/art.c, nicht hier.
STR(STR_ART_LAUFEN,     0,  "Running",      "Laufen",         "Course",       "Corsa",         "Correr")
STR(STR_ART_BIKE,       0,  "Road/Gravel",  "Strasse/Gravel", "Route/Gravel", "Strada/Gravel", "Ruta/Gravel")
STR(STR_ART_WANDERN,    0,  "Hiking",       "Wandern",        "Randonnée",    "Escursione",    "Senderismo")
STR(STR_ART_KRAFT,      0,  "Strength",     "Kraft",          "Muscu",        "Pesi",          "Fuerza")
STR(STR_ART_MTB,        0,  "MTB",          "MTB",            "VTT",          "MTB",           "MTB")
STR(STR_ART_YOGA,       0,  "Yoga",         "Yoga",           "Yoga",         "Yoga",          "Yoga")
STR(STR_ART_SCHWIMMEN,  0,  "Swimming",     "Schwimmen",      "Natation",     "Nuoto",         "Natación")
// Obertitel im Menue unter Strasse/Gravel und MTB.
STR(STR_GRUPPE_BIKE,    0,  "Bike",         "Bike",           "Vélo",         "Bici",          "Bici")

// ---- Laufender Schirm: die kleine Zeile (Puffer zeile[40]) ----------------
STR(STR_ZEILE_BEREIT,   40, "%s · ready",   "%s · bereit",    "%s · prêt",    "%s · pronto",   "%s · listo")
STR(STR_ZEILE_PAUSE,    40, "%s · paused",  "%s · Pause",     "%s · pause",   "%s · pausa",    "%s · pausa")

// Neben der grossen Zahl beim Krafttraining.
STR(STR_GROSS_PAUSE,    0,  "Rest",         "Pause",          "Repos",        "Recupero",      "Descanso")
STR(STR_GROSS_WDH,      0,  "Reps",         "Wdh.",           "Rép.",         "Rip.",          "Rep.")

// ---- Laufender Schirm: der Titel (fett; Puffer text[24]) ------------------
STR(STR_SELECT_STARTET, 0,  "Select starts", "Select startet", "Select démarre", "Select avvia", "Select inicia")
STR(STR_PULS_ALT,       24, "%u · old",     "%u · alt",       "%u · ancien",  "%u · vecchio",  "%u · viejo")
STR(STR_PULS_UNTER,     24, "%u · < Zone 1", "%u · < Zone 1", "%u · < Zone 1", "%u · < Zona 1", "%u · < Zona 1")
STR(STR_PULS_ZONE,      24, "%u · Zone %d", "%u · Zone %d",   "%u · Zone %d", "%u · Zona %d",  "%u · Zona %d")
STR(STR_KEIN_PULS,      0,  "no pulse",     "kein Puls",      "pas de pouls", "polso assente", "sin pulso")
STR(STR_HOLE_STAND,     0,  "Loading …",    "Hole den Stand …", "Un instant …", "Un attimo …", "Cargando …")

// ---- Laufender Schirm: die kleinen Felder ---------------------------------
// Drei Spalten auf flint sind gut dreissig Punkte breit: sechs Zeichen.
STR(STR_FELD_SAETZE,    0,  "Sets",         "Saetze",         "Séries",       "Serie",         "Series")
STR(STR_FELD_GESAMT,    0,  "Total",        "Gesamt",         "Total",        "Totale",        "Total")
STR(STR_FELD_BAHNEN,    0,  "Laps",         "Bahnen",         "Long.",        "Vasche",        "Largos")
STR(STR_FELD_SCHR_KURZ, 0,  "Steps",        "Schr.",          "Pas",          "Passi",         "Pasos")
STR(STR_FELD_SCHRITTE,  0,  "Steps",        "Schritte",       "Pas",          "Passi",         "Pasos")

// ---- Laufender Schirm: die Fusszeile --------------------------------------
STR(STR_FUSS_SPEICHERE, 0,  "Saving …",     "Speichere …",    "Sauvegarde …", "Salvataggio …", "Guardando …")
STR(STR_FUSS_NOCHMAL,   0,  "Down again: discard", "Nochmal Unten: verwerfen", "Encore Bas : effacer", "Ancora Giù: scarta", "Otra vez Abajo: borrar")
STR(STR_FUSS_PAUSE,     0,  "Up saves, Down discards", "Oben speichert, Unten verwirft", "Haut garde, Bas efface", "Su salva, Giù scarta", "Arriba guarda, Abajo borra")
STR(STR_FUSS_KEIN_TEL,  0,  "No phone: no distance", "Kein Telefon: keine Strecke", "Sans tél. : pas de distance", "Senza telefono, niente km", "Sin teléfono: sin distancia")
STR(STR_FUSS_AKKU,      0,  "Low battery: HR every 5 s", "Akku schwach: Puls alle 5 s", "Batterie faible : pouls/5 s", "Batt. bassa: polso ogni 5 s", "Batería baja: pulso cada 5 s")
STR(STR_FUSS_KOMPASS,   0,  "Compass not ready", "Kompass nicht bereit", "Boussole pas prête", "Bussola non pronta", "Brújula no lista")

// ---- Zusammenfassung nach dem Speichern -----------------------------------
STR(STR_GESPEICHERT,    0,  "Saved",        "Gespeichert",    "Enregistré",   "Salvato",       "Guardado")
STR(STR_TEL_AUFGEGEBEN, 0,  "Phone unreachable,\nsent on next launch", "Telefon nicht erreichbar,\ngeht beim nächsten Öffnen", "Téléphone absent,\nenvoi à la réouverture", "Telefono assente,\ninvio alla riapertura", "Sin teléfono,\nse envía al reabrir")
STR(STR_TEL_WARTET,     0,  "sending to phone …", "geht ans Telefon …", "envoi au téléphone …", "invio al telefono …", "enviando al teléfono …")
STR(STR_TEL_ANGEKOMMEN, 0,  "received by phone", "beim Telefon angekommen", "reçu par le téléphone", "ricevuto dal telefono", "recibido en el teléfono")

// ---- App-Glance (Untertitel im Startmenue) --------------------------------
// STR_GLANZ_SEIT ist nur der Kopf; die Dauer haengt glanz.c als Vorlage des
// Systems an ("25 min"). Deshalb endet er auf ein Leerzeichen.
STR(STR_GLANZ_SEIT,     64, "%s in background, for ", "%s im Hintergrund, seit ", "%s en arrière-plan, depuis ", "%s in background, da ", "%s en segundo plano, desde hace ")
STR(STR_GLANZ_HINTERGRUND, 48, "%s in background", "%s im Hintergrund", "%s en arrière-plan", "%s in background", "%s en segundo plano")
