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

// --- Die Sprache ---
//
// DIE SPRACHE DES TELEFONS, NICHT DIE DER UHR. Die Schwester-Apps lassen sich
// die Uhrsprache per MESSAGE_KEY_LANG melden; Kieselsport hat diesen
// Schluessel nicht, und ihn nachzuruesten hiesse eine neue Nachricht der Uhr,
// die Kiesel-Helper mitliest (es hoert jede AppMessage mit) und einordnen
// muesste. navigator.language dagegen steht sofort bereit - auch beim
// allerersten Oeffnen der Konfigseite, bevor die Uhr je etwas geschickt hat -
// und fast immer spricht das Telefon dieselbe Sprache wie die Uhr. Fehlt es
// (manche JS-Umgebungen der Pebble-App haben kein navigator), gilt Englisch.
//
// 0 Englisch, 1 Deutsch, 2 Franzoesisch, 3 Italienisch, 4 Spanisch - wie die
// Spalten in src/c/strings_table.h.
function sprache() {
  var code = '';
  try {
    code = String((typeof navigator !== 'undefined' && navigator &&
                   (navigator.language || (navigator.languages && navigator.languages[0]))) || '');
  } catch (x) { code = ''; }
  code = code.substring(0, 2).toLowerCase();
  var i = ['en', 'de', 'fr', 'it', 'es'].indexOf(code);
  return i < 0 ? 0 : i;
}

var SPRACHE = sprache();
var clay = new Clay(clayConfig(SPRACHE));

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
var ARTEN = clayConfig.arten(SPRACHE);
// Titel und Aktion des Pins je Sprache (Reihenfolge wie oben). Der
// Untertitel bleibt "Kieselsport" - ein Name, nichts zu uebersetzen.
var PIN_TEXT = [
  { titel: 'Workout: ', starten: 'Start now' },
  { titel: 'Training: ', starten: 'Jetzt starten' },
  { titel: 'Entraînement : ', starten: 'Démarrer' },
  { titel: 'Allenamento: ', starten: 'Inizia ora' },
  { titel: 'Entrenamiento: ', starten: 'Empezar ya' },
];

function pad(n) { return (n < 10 ? '0' : '') + n; }

function pinEinstellungen() {
  var e = clay.getSettings(localStorage.getItem('clay-settings') || '{}', false) || {};
  var art = parseInt(e.PIN_ART, 10) || 0;
  var zeit = (e.PIN_ZEIT || '18:00').split(':');
  return { art: art, stunde: parseInt(zeit[0], 10) || 0, minute: parseInt(zeit[1], 10) || 0 };
}

function bauePin(art, wann) {
  var pinText = PIN_TEXT[SPRACHE] || PIN_TEXT[0];
  var y = wann.getFullYear(), m = wann.getMonth() + 1, d = wann.getDate();
  return {
    id: 'kieselsport-' + y + pad(m) + pad(d),
    time: wann.toISOString(),
    layout: {
      type: 'genericPin',
      title: pinText.titel + ARTEN[art - 1],
      subtitle: 'Kieselsport',
      tinyIcon: 'system://images/ACTIVITY',
    },
    actions: [{ title: pinText.starten, type: 'openWatchApp', launchCode: art }],
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

// Die Nacht: auf der Uhr Minuten seit Mitternacht, auf der Konfigseite
// "HH:MM". Umrechnen in beide Richtungen.
function hhmm(minuten, vorgabe) {
  var m = parseInt(minuten, 10);
  if (isNaN(m) || m < 0 || m >= 1440) return vorgabe;
  return pad(Math.floor(m / 60)) + ':' + pad(m % 60);
}

function minutenAus(text, vorgabe) {
  var teile = String(text || '').split(':');
  if (teile.length !== 2) return vorgabe;
  var m = parseInt(teile[0], 10) * 60 + parseInt(teile[1], 10);
  return isNaN(m) ? vorgabe : m;
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
    NACHT_AN: e.NACHT_AN === false ? 0 : 1,
    NACHT_VON: minutenAus(e.NACHT_VON, 22 * 60),
    NACHT_BIS: minutenAus(e.NACHT_BIS, 8 * 60),
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
  if (p.NACHT_AN !== undefined) e.NACHT_AN = ganz(p.NACHT_AN, 1) !== 0;
  if (p.NACHT_VON !== undefined) e.NACHT_VON = hhmm(p.NACHT_VON, '22:00');
  if (p.NACHT_BIS !== undefined) e.NACHT_BIS = hhmm(p.NACHT_BIS, '08:00');
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
  // Die Nacht und die Kurve kommen in Stuecken zu hunderten Bytes - dafuer
  // genuegt eine Zeile.
  if (p.NACHT_MINUTEN !== undefined) { console.log('Nacht: ab ' + p.NACHT_AB + ' von ' + p.NACHT_ANZAHL); return; }
  if (p.KURVE !== undefined) { console.log('Kurve: ab ' + p.KURVE_AB); return; }
  console.log('Training: ' + JSON.stringify(p));
});
