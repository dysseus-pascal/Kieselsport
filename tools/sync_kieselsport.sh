#!/bin/sh
# Quellen nach ~/kieselsport spiegeln und bauen (waf vertraegt keine Pfade
# mit Leerzeichen). Aufruf: sync_kieselsport.sh [<Quellordner>] [<schalter>]
# Ohne Argument wird $KIESELSPORT_SRC verwendet.
#
# Schalter als zweites Argument:
#   sensor -DKS_DEMO -DKS_ECHT_SENSOR   wie demo, aber MIT der echten
#                              Sensorrechnung: Puls bleibt erfunden, Bahnen
#                              und Wiederholungen werden wirklich gezaehlt.
#                              Dazu speist man den Kompass ein:
#                              pebble emu-compass --heading <grad> --calibrated
#   demo   -DKS_DEMO   Beispieldaten einsetzen, damit sich die Anzeige im
#                      Emulator ansehen laesst. Ohne Telefon kommt sonst nie
#                      etwas an, und der Schirm bliebe auf "Keine Navigation".
# Nur in der WSL-KOPIE; die Windows-Quelle bleibt unberuehrt.
#
# Diese App BRAUCHT Clay: der Maximalpuls steht in einer Konfigseite, und
# ohne ihn sind die Pulszonen geraten.
export PATH=$HOME/.local/bin:$PATH
SRC="${1:-$KIESELSPORT_SRC}"
MODE="$2"
[ -d "$SRC" ] || { echo "Quellordner fehlt: '$SRC'"; exit 1; }
DST=$HOME/kieselsport
mkdir -p "$DST"
# waf erzeugt message_keys.auto.h aus package.json und merkt eine Aenderung
# daran nicht - deshalb aufraeumen, sobald sich package.json unterscheidet.
if ! cmp -s "$SRC/package.json" "$DST/package.json" 2>/dev/null; then
  (cd "$DST" && pebble clean >/dev/null 2>&1)
fi
rm -rf "$DST/src" "$DST/build"
cp -r "$SRC/src" "$DST/"
cp "$SRC/package.json" "$SRC/wscript" "$DST/"
if [ -d "$SRC/resources" ]; then
  rm -rf "$DST/resources"
  cp -r "$SRC/resources" "$DST/"
fi
cd "$DST" || exit 1

# Clay liegt nicht im Repo (node_modules gehoert nicht dorthin), sondern
# kommt einmalig per npm und bleibt dann im Baumverzeichnis liegen.
[ -d node_modules/@rebble/clay ] || npm install --no-audit --no-fund 2>&1 | tail -1

# Welche Sportart der Pruefbau gleich oeffnet. Kraft und Schwimmen haben
# eigene Schirme, die sich sonst im Emulator nie zeigen.
ART="${3:-ArtLaufen}"
if [ "$MODE" = sensor ]; then
  FLAGS="'-DKS_DEMO', '-DKS_ECHT_SENSOR'"
else
  FLAGS="'-DKS_DEMO'"
fi

case "$MODE" in
  demo|sensor)
    # Den Schalter NUR in der Kopie einsetzen. Uebernommen aus SupCycle, wo
    # dieselbe Stelle schon zweimal danebenging:
    #
    # 1. `ctx.load('pebble_sdk')` taugt NICHT als Anker - es steht in
    #    options(), configure() und build(). Trifft es options(), bricht waf
    #    mit "OptionsContext object has no attribute env" ab.
    # 2. `ctx.env` in configure() reicht auch nicht: pebble_sdk baut je
    #    Plattform eine eigene Umgebung, und der Schalter landete in keiner
    #    davon. Er wurde still verworfen, der Bau lief durch, und im Emulator
    #    stand "Keine Navigation" - als waere nie etwas angekommen.
    #
    # Richtig ist: in build(), ueber ALLE Umgebungen.
    ANKER="    build_worker = os.path.exists('worker_src')"
    {
      echo "    for _e in ctx.all_envs.values():"
      echo "        _e.append_value('CFLAGS', [${FLAGS}, '-DKS_DEMO_ART=${ART}'])"
      echo ""
      echo "$ANKER"
    } > /tmp/ks_patch.txt
    awk -v anchor="$ANKER" 'BEGIN{while((getline l < "/tmp/ks_patch.txt")>0) p=p l "\n"}
         $0==anchor{printf "%s", p; next} {print}' wscript > /tmp/ks_wscript
    mv /tmp/ks_wscript wscript
    grep -q "append_value('CFLAGS'" wscript || { echo "FEHLER: Schalter nicht eingesetzt"; exit 1; }
    echo "Pruefbau ${MODE}, Art ${ART}"
    ;;
  "") ;;
  *)  echo "Unbekannter Schalter: $MODE"; exit 1 ;;
esac

echo "Dateien in src/c: $(ls src/c | wc -l)"
pebble build 2>&1 | grep -iE 'error|warning: \.\./src|APP MEMORY|footprint in RAM|finished successfully|Build failed|Traceback'
