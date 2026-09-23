#!/bin/sh
# Den Wiederholungszaehler im Emulator nachmessen - ohne Hantelbank.
#
# WARUM DAS GEHT: "pebble emu-accel custom <datei>" spielt bis zu 255
# Messungen in den Emulator ein, eine Zeile "x,y,z" in Milli-g je Messung.
# Bei 25 Hz sind das zehn Sekunden. Eine Wiederholung ist nichts anderes als
# eine Schwingung - und die kann man schreiben, statt sie zu machen.
#
# Voraussetzung: ein Bau mit dem Schalter "sensor", sonst rechnet die App
# nicht, sondern erfindet:
#   tools/sync_kieselsport.sh "" sensor ArtKraft
#
# Aufruf: tools/kraft_probe.sh [<emery|flint|gabbro>]
set -e
UHR="${1:-emery}"
export PATH=$HOME/.local/bin:$PATH
ORDNER=$(mktemp -d)

# Zehn saubere Wiederholungen: ein g Schwerkraft, darauf eine Schwingung von
# 400 mg mit einer Sekunde Dauer. So bewegt sich ein Handgelenk beim Curl.
awk 'BEGIN {
  for (rep = 0; rep < 10; rep++)
    for (i = 0; i < 25; i++)
      printf "0,0,%d\n", 1000 + int(400 * sin(2 * 3.14159265 * i / 25) + 0.5)
}' > "$ORDNER/zehn.csv"

# Ruhe: das Rauschen einer liegenden Hand. Setzt die Grundlinie, bevor
# gemessen wird - sonst zieht ein voriger Versuch das Ergebnis.
awk 'BEGIN {
  for (i = 0; i < 250; i++)
    printf "0,0,%d\n", 1000 + int(25 * sin(2 * 3.14159265 * i / 7) + 0.5)
}' > "$ORDNER/ruhe.csv"

# Schuetteln: 12,5 Hz, jede Messung das andere Vorzeichen, 350 mg. Schneller
# geht es mit 25 Messungen je Sekunde nicht. Das darf NICHT zaehlen.
awk 'BEGIN {
  for (i = 0; i < 250; i++)
    printf "0,0,%d\n", (i % 2 == 1) ? 1350 : 650
}' > "$ORDNER/schuetteln.csv"

echo "== Probe auf $UHR =="
pebble install --emulator "$UHR" >/dev/null 2>&1 || {
  echo "Bau nicht aufgespielt - erst bauen, dann probieren"; exit 1; }
sleep 3

echo "-- Grundlinie setzen (Ruhe) --"
pebble emu-accel custom "$ORDNER/ruhe.csv" --emulator "$UHR" >/dev/null 2>&1
sleep 12

echo "-- zehn Wiederholungen --"
pebble emu-accel custom "$ORDNER/zehn.csv" --emulator "$UHR" >/dev/null 2>&1
sleep 14
echo "   erwartet: reps=10, danach saetze=1"

echo "-- Schuetteln, das keines sein darf --"
pebble emu-accel custom "$ORDNER/schuetteln.csv" --emulator "$UHR" >/dev/null 2>&1
sleep 14
echo "   erwartet: hoechstens eine Wiederholung dazu"

echo
echo "Das Ergebnis steht im App-Protokoll:"
echo "  pebble logs --emulator $UHR"
echo "und auf dem Schirm (Saetze / Gesamt)."
rm -rf "$ORDNER"
