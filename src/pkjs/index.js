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

Pebble.addEventListener('ready', function () {
  console.log('Kieselsport bereit');
});

Pebble.addEventListener('appmessage', function (e) {
  // Nur fürs Logbuch: was hinausgeht, ist an anderer Stelle schon versorgt.
  console.log('Training beendet: ' + JSON.stringify(e.payload));
});
