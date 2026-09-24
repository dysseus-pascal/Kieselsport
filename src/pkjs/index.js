// Die Telefonseite: sie hält die Konfigseite und sonst nichts.
//
// SIE LEITET DIE TRAININGSDATEN NICHT WEITER, und das ist kein Versäumnis.
// Die Pebble-App reicht jede eingehende AppMessage auch an klassische
// Companion-Apps weiter (appMessageToMultipleCompanions); Kiesel-Helper hört
// also ohnehin mit und trägt ein, was in die Gesundheitsakte gehört. Hier
// dasselbe noch einmal zu tun, hiesse zwei Wege zu pflegen, die
// auseinanderlaufen können.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

// --- Der Pin "Training" in der Timeline ---
//
// EIN PIN JE TAG, SIEBEN TAGE VORAUS, mit der Aktion "Jetzt starten": die
// oeffnet die Uhr-App mit der Art als Launch-Code, und der Schirm steht
// bereit. Gesetzt wird, sobald die Uhr-App laeuft - nur dann laeuft dieser
// Code. Wer die Uhr-App eine Woche nicht oeffnet, hat danach keine Pins mehr;
// das ist der Preis dafuer, dass kein Server dazwischen steht.
//
// Erst die App-eigene Schnittstelle (Pebble.insertTimelinePin, neue
// Pebble-App), sonst die Rebble-REST-API mit dem Timeline-Token.
var API_URL = 'https://timeline-api.rebble.io/v1/user/pins/';
var ARTEN = ['Laufen', 'Strasse/Gravel', 'Wandern', 'Kraft', 'MTB', 'Yoga', 'Schwimmen'];

function pad(n) { return (n < 10 ? '0' : '') + n; }

function pinEinstellungen() {
  var e = clay.getSettings(localStorage.getItem('clay-settings') || '{}', false) || {};
  var art = parseInt(e.PIN_ART, 10) || 0;
  var zeit = (e.PIN_ZEIT || '18:00').split(':');
  return { art: art, stunde: parseInt(zeit[0], 10) || 0, minute: parseInt(zeit[1], 10) || 0 };
}

function bauePin(art, wann) {
  var y = wann.getFullYear(), m = wann.getMonth() + 1, d = wann.getDate();
  return {
    id: 'kieselsport-' + y + pad(m) + pad(d),
    time: wann.toISOString(),
    layout: {
      type: 'genericPin',
      title: 'Training: ' + ARTEN[art - 1],
      subtitle: 'Kieselsport',
      tinyIcon: 'system://images/ACTIVITY',
    },
    actions: [{ title: 'Jetzt starten', type: 'openWatchApp', launchCode: art }],
  };
}

function setzeLokal(pin, cb) {
  if (typeof Pebble.insertTimelinePin !== 'function') { cb(false); return; }
  var fertig = false;
  var ende = function (ok) { if (!fertig) { fertig = true; cb(ok); } };
  setTimeout(function () { ende(false); }, 5000);
  Pebble.insertTimelinePin(pin, function () { ende(true); }, function () { ende(false); });
}

function setzeRest(pin, token, cb) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () { cb(this.status >= 200 && this.status < 300); };
  xhr.onerror = function () { cb(false); };
  xhr.open('PUT', API_URL + pin.id);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.setRequestHeader('X-User-Token', token);
  xhr.send(JSON.stringify(pin));
}

function setzePins() {
  var e = pinEinstellungen();
  if (e.art < 1 || e.art > ARTEN.length) { console.log('timeline: kein Pin gewuenscht'); return; }
  var pins = [];
  var jetzt = new Date();
  for (var i = 0; i < 7; i++) {
    var wann = new Date(jetzt.getFullYear(), jetzt.getMonth(), jetzt.getDate() + i, e.stunde, e.minute, 0);
    if (wann.getTime() < jetzt.getTime() - 3600000) continue;   // heute schon vorbei
    pins.push(bauePin(e.art, wann));
  }
  var schicke = function (einfuegen) {
    (function naechster() {
      var pin = pins.shift();
      if (!pin) { console.log('timeline: fertig'); return; }
      einfuegen(pin, function (ok) {
        console.log('timeline: ' + pin.id + ' -> ' + (ok ? 'ok' : 'fehlgeschlagen'));
        naechster();
      });
    })();
  };
  if (typeof Pebble.insertTimelinePin === 'function') {
    schicke(setzeLokal);
  } else if (typeof Pebble.getTimelineToken === 'function') {
    Pebble.getTimelineToken(function (token) {
      schicke(function (pin, cb) { setzeRest(pin, token, cb); });
    }, function () { console.log('timeline: kein Token'); });
  }
}

// --- Die Einstellungen: die Uhr haelt sie ---
//
// GEAENDERT WIRD AN ZWEI STELLEN - auf der Konfigseite hier und in
// Kiesel-Helper. Beide schicken an die Uhr, und die Uhr meldet danach, was
// gilt (beim Start und nach jeder Aenderung). Diese Meldung wird hier in die
// Konfigseite uebernommen, damit sie beim naechsten Oeffnen den Stand der Uhr
// zeigt und nicht den eigenen alten.
//
// AUSSER EINE AENDERUNG VON HIER KAM NIE AN (Uhr-App zu, Verbindung weg):
// dann ist sie vorgemerkt und geht zuerst an die Uhr, statt von deren altem
// Stand ueberschrieben zu werden.
var VORGEMERKT = 'kieselsport_vorgemerkt';

function vorgemerkt() { return localStorage.getItem(VORGEMERKT) === '1'; }
function vormerken(ja) {
  if (ja) localStorage.setItem(VORGEMERKT, '1'); else localStorage.removeItem(VORGEMERKT);
}

function gespeichert() {
  try { return JSON.parse(localStorage.getItem('clay-settings') || '{}') || {}; } catch (x) { return {}; }
}

function ganz(v, vorgabe) {
  var n = parseInt(v, 10);
  return isNaN(n) ? vorgabe : n;
}

function schickeEinstellungen() {
  var e = gespeichert();
  var nachricht = {
    MAXPULS: ganz(e.MAXPULS, 190),
    PAUSENZIEL: ganz(e.PAUSENZIEL, 90),
    EMPFIND: ganz(e.EMPFIND, 2),
    BECKEN: ganz(e.BECKEN, 25),
    PIN_ART: ganz(e.PIN_ART, 0),
    PIN_ZEIT: String(e.PIN_ZEIT || '18:00'),
  };
  Pebble.sendAppMessage(nachricht, function () {
    console.log('einstellungen: an die Uhr');
    vormerken(false);
  }, function () {
    console.log('einstellungen: Uhr nicht erreicht, bleibt vorgemerkt');
  });
}

function uebernehmeVonUhr(p) {
  var e = gespeichert();
  var pinVorher = String(e.PIN_ART) + '|' + String(e.PIN_ZEIT);
  e.MAXPULS = ganz(p.MAXPULS, 190);
  if (p.PAUSENZIEL !== undefined) e.PAUSENZIEL = ganz(p.PAUSENZIEL, 90);
  if (p.BECKEN !== undefined) e.BECKEN = ganz(p.BECKEN, 25);
  if (p.EMPFIND !== undefined) e.EMPFIND = String(p.EMPFIND);
  if (p.PIN_ART !== undefined) e.PIN_ART = String(p.PIN_ART);
  if (p.PIN_ZEIT !== undefined) e.PIN_ZEIT = String(p.PIN_ZEIT);
  localStorage.setItem('clay-settings', JSON.stringify(e));
  console.log('einstellungen: Stand der Uhr uebernommen');
  if (String(e.PIN_ART) + '|' + String(e.PIN_ZEIT) !== pinVorher) setzePins();
}

Pebble.addEventListener('ready', function () {
  console.log('Kieselsport bereit');
  setzePins();
});

Pebble.addEventListener('webviewclosed', function (e) {
  // Nach der Konfigseite gleich neu setzen: Art oder Zeit koennen sich
  // geaendert haben. Clay hat die Einstellungen da schon gespeichert und
  // geschickt; die eigene Sendung merkt sich nur, ob sie ankam.
  if (!e || !e.response) return;
  vormerken(true);
  setTimeout(function () {
    schickeEinstellungen();
    setzePins();
  }, 500);
});

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload || {};
  if (p.MAXPULS !== undefined && p.ART === undefined) {
    // Die Meldung der Einstellungen (eine Zusammenfassung traegt ART).
    if (vorgemerkt()) schickeEinstellungen();
    else uebernehmeVonUhr(p);
    return;
  }
  // Nur fürs Logbuch: was hinausgeht, ist an anderer Stelle schon versorgt.
  console.log('Training: ' + JSON.stringify(p));
});
