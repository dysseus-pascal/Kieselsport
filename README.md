# Kieselsport

Trainingsaufzeichnung für die Pebble — **emery, flint und gabbro**.

Es gibt fertige Trainings-Apps für die Pebble, und gute. Diese hier hat einen
einzigen Grund: **sie trägt ihre Daten in dein Haus.** Auf Stop schickt die Uhr
eine Zusammenfassung; [Kiesel-Helper](https://github.com/dysseus-pascal/Kiesel-Helper)
hört mit und trägt sie in die Gesundheitsakte ein. Damit steht ein Lauf im
selben Wochenprofil wie Schlaf, Ruhepuls und Wasser — und in denselben
Zusammenhängen.

## Was sie zeigt

| | |
|---|---|
| **Zeit** | gross, in der Mitte — das ist die Zahl, die man im Laufen liest |
| **Puls** | daneben, dazu die Zone in Worten |
| **Zonenbalken** | fünf Felder, das erreichte gefüllt |
| **unten** | nur was die Sportart hergibt |

**Nicht jede Kennzahl passt zu jeder Art.** Schritte beim Velofahren sind eine
Zufallszahl — die Hand am Lenker wackelt — und eine Distanz kann die Uhr ohne
GPS nur aus Schritten schätzen; beim Krafttraining ist beides sinnlos. Jede Art
trägt deshalb mit, was sie kann:

| | Schritte | Distanz | Kalorien |
|---|---|---|---|
| Laufen | ja | ja | ja |
| Velo | — | — | ja |
| Wandern | ja | ja | ja |
| Kraft | — | — | ja |

Kalorien gelten überall: sie hängen am Puls und an der Bewegung, nicht an
Schritten.

## Der Zonenwechsel ist der Punkt

Steigt der Puls in die nächste Zone, brummt die Uhr **zweimal kurz**; fällt er
zurück, **einmal**. Wer läuft, soll das unterscheiden können, ohne hinzusehen.
Das ist die einzige Stelle, an der die App von sich aus etwas sagt — und der
eigentliche Grund, sie beim Sport zu tragen.

Die Zonen rechnen sich aus deinem **Maximalpuls**, und der steht in der
Konfigseite. »220 minus Alter« ist eine Faustformel mit einer Streuung von gut
zehn Schlägen nach oben wie unten; aus ihr Zonen zu rechnen und sie dann als
Messung zu zeigen, wäre eine Behauptung.

## Zwei Dinge, die leicht danebengehen

**Die Summen werden nicht mitgezählt, sondern abgezogen.** Die Uhr führt
Schritte, Distanz und Kalorien ohnehin über den ganzen Tag; ein Training ist
die Differenz zwischen Anfang und Ende. Selbst zu zählen hiesse, dieselbe
Messung ein zweites Mal zu machen — mit eigenen Fehlern.

**Die dichte Pulsmessung bleibt aktiv, nachdem die App endet.** So steht es in
der Dokumentation von `health_service_set_heart_rate_sample_period`, und es ist
die teuerste Falle dieser App: wer sie beim Beenden nicht zurücksetzt, misst
noch stundenlang im Fünfsekundentakt weiter und saugt den Akku leer. Deshalb
steht das Zurücksetzen an **jeder** Stelle, an der ein Training endet — beim
Stop, bei der Pause und beim Verlassen der App.

## Bedienung

| | |
|---|---|
| **Select** | Pause und weiter |
| **Zurück** | erst Pause, dann beenden |

Zurück beendet **nicht sofort**. Ein Druck auf die falsche Taste beim Laufen
dürfte sonst ein Training kosten.

Ein Training unter einer Minute wird verworfen — ein Fehlgriff im Menü soll das
Archiv nicht füllen.

## Was sie schickt

| Feld | Nummer | Bedeutung |
|---|---|---|
| `ART` | 10000 | 0 Laufen, 1 Velo, 2 Wandern, 3 Kraft |
| `BEGINN` | 10001 | Unix-Sekunden |
| `DAUER` | 10002 | Sekunden ohne Pausen |
| `SCHRITTE` | 10003 | |
| `METER` | 10004 | |
| `KCAL` | 10005 | |
| `PULS_MITTEL` | 10006 | |
| `PULS_MAX` | 10007 | |
| `MAXPULS` | 10008 | **hinein**, aus der Konfigseite |

Die Nummern ergeben sich aus der Reihenfolge der `messageKeys` in der
`package.json`, beginnend bei 10000. **Wer dort eine Zeile dazwischenschiebt,
verschiebt alle folgenden** — und Kiesel-Helper trägt danach still die falschen
Werte ins falsche Feld.

Dasselbe gilt für die Reihenfolge der Sportarten in `art.c`: eine eingeschobene
Zeile macht aus jedem gespeicherten Wandern rückwirkend ein Krafttraining.

**Kein `companionApp`-Eintrag in der `package.json`.** Ein Paketname dort
schaltet die Pebble-App auf PebbleKit2 um — und damit fällt der klassische
Nebenempfänger weg, über den Kiesel-Helper überhaupt erst mithört.

## Bauen

```bash
KIESELSPORT_SRC=<dieser Ordner> tools/sync_kieselsport.sh
```

Spiegelt nach `~/kieselsport` und baut dort — waf verträgt keine Pfade mit
Leerzeichen.

```bash
KIESELSPORT_SRC=<dieser Ordner> tools/sync_kieselsport.sh "" demo
```

Der Schalter `demo` erfindet Puls und Summen und springt gleich ins Training.
Ohne ihn lässt sich der laufende Schirm im Emulator nie ansehen: dort gibt es
weder einen Sensor noch einen Tastendruck von aussen.

## Was sie nicht tut

**Keine Karte, keine Route, kein Höhenprofil.** Die Pebble hat kein GPS; alles
davon käme ohnehin vom Telefon. Das ist ein eigenes Projekt und steht noch aus.

## Lizenz

[CC0 1.0](LICENSE) — gemeinfrei.
