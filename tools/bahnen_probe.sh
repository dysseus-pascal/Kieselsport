#!/bin/sh
# Den Bahnenzaehler von Kieselsport im Emulator durchspielen.
#
# WARUM DAS UEBERHAUPT GEHT: pebble emu-compass speist eine Richtung ein. Eine
# Bahn ist nichts anderes als ein Richtungswechsel um 180 Grad - und den kann
# man schicken, statt ihn zu schwimmen.
#
# WARUM IN SCHRITTEN UND NICHT IN EINEM SPRUNG: die App mittelt die Richtung
# ueber mehrere Messungen (ein Zug dreht das Handgelenk, eine Wende dreht den
# Menschen). Ein einzelner eingespeister Wert bewegt den Mittelwert um ein
# Sechstel - zu wenig, um die Schwelle zu reissen. Eine echte Wende dauert
# eine Sekunde und liefert dabei ein Dutzend Messungen; genau das wird hier
# nachgestellt.
#
# Aufruf: bahnentest.sh <emery|flint|gabbro>
set -e
UHR="${1:-emery}"
export PATH=$HOME/.local/bin:$PATH

kompass() {
  pebble emu-compass --heading "$1" --calibrated --emulator "$UHR" >/dev/null 2>&1 || true
}

# Eine Wende: in sechs Schritten herum, dann ein paar Messungen ruhig halten -
# so sieht es aus, wenn jemand abstoesst und wieder geradeaus schwimmt.
wende() {
  for grad in $2 $3 $4 $5 $6 $7; do kompass "$grad"; done
  kompass "$7"; kompass "$7"; kompass "$7"
  echo "  -> Richtung jetzt $7 Grad ($1)"
}

schuss() {
  pebble screenshot --emulator "$UHR" "$HOME/bahnen_$1.png" --no-open >/dev/null 2>&1
  echo "  Bild: bahnen_$1.png"
}

echo "== Einschwingen: zehn Sekunden dieselbe Richtung =="
for i in 1 2 3 4 5 6 7 8 9 10; do kompass 0; sleep 0.4; done
sleep 4
schuss 0_start

echo "== Wende 1: nach Sueden =="
wende "hin" 30 60 90 120 150 180
sleep 6
schuss 1_erste

echo "== Wende 2: zurueck nach Norden =="
wende "zurueck" 150 120 90 60 30 0
sleep 6
schuss 2_zweite

echo "== Wende 3: wieder nach Sueden =="
wende "hin" 30 60 90 120 150 180
sleep 6
schuss 3_dritte

echo "== Ein Schlenker, der KEINE Wende sein darf (60 Grad) =="
wende "schlenker" 170 160 150 140 130 120
sleep 6
wende "zurueck zur Bahn" 130 140 150 160 170 180
sleep 6
schuss 4_schlenker

echo "fertig"
