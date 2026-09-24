// Die Konfigseite: Puls, Krafttraining, Schwimmen.
//
// DER MAXIMALPULS IST PERSÖNLICH. »220 minus Alter« ist eine Faustformel mit
// einer Streuung von gut zehn Schlägen nach oben wie unten; aus ihr die Zonen
// zu rechnen und sie dann als Messung zu zeigen, wäre eine Behauptung. Wer
// seinen Wert aus einem Test kennt, trägt ihn ein. Wer nicht, lässt die
// Vorgabe stehen und weiss wenigstens, woran er ist.

// FUENF SPRACHEN, ENGLISCH ALS RUECKFALL. Welche gilt, entscheidet index.js
// (siehe dort) und reicht sie als Zahl herein: 0 Englisch, 1 Deutsch,
// 2 Franzoesisch, 3 Italienisch, 4 Spanisch - dieselbe Reihenfolge wie die
// Spalten in src/c/strings_table.h. Die Werte der Auswahlfelder (value) sind
// in allen Sprachen dieselben: nur die Beschriftung wechselt, nie das, was
// an die Uhr geht.
var TEXT = [
  {
    intro: 'Heart rate zones are based on your maximum heart rate. If you ' +
           'know yours from a test, enter it — otherwise the rule of thumb ' +
           'for a forty-year-old stays in place.',
    puls: 'Heart rate',
    maxpuls: 'Maximum heart rate',
    maxpulsNote: 'Beats per minute. Zone 1 starts at 50 %, zone 5 at 90 %.',
    kraft: 'Strength training',
    kraftText: 'The watch counts reps from the movement of your wrist. That ' +
               'is an estimate: with curls and bench press it moves on every ' +
               'rep, with squats and the bar on your neck hardly at all. ' +
               'Rest times, on the other hand, it measures reliably.',
    pause: 'Rest',
    pauseNote: 'After this many seconds of rest the watch buzzes once. 0 = never.',
    empfind: 'Sensitivity',
    empfindNote: 'How clear a movement must be to count as a rep. If the ' +
                 'watch counts too many, make it less sensitive; too few, ' +
                 'more sensitive.',
    traege: 'low', normal: 'normal', fein: 'high',
    schwimmen: 'Swimming',
    schwimmenText: 'The watch counts laps with the compass: at every turn the ' +
                   'direction changes by 180 degrees. Only the pool length ' +
                   'turns that into a distance — the watch cannot know it.',
    becken: 'Pool length',
    beckenNote: 'Metres per lap.',
    pin: 'Timeline reminder',
    pinText: 'A “Workout” pin in the watch timeline, every day at the chosen ' +
             'time, with the action “Start now”. It is set while the watch ' +
             'app runs and covers the next seven days.',
    art: 'Sport',
    keinPin: 'no pin',
    arten: ['Running', 'Road/Gravel', 'Hiking', 'Strength', 'MTB', 'Yoga', 'Swimming'],
    zeit: 'Time',
    speichern: 'Save',
  },
  {
    intro: 'Die Pulszonen rechnen sich aus deinem Maximalpuls. Kennst du ihn aus ' +
           'einem Test, trag ihn ein — sonst bleibt die Faustformel für einen ' +
           'Vierzigjährigen stehen.',
    puls: 'Puls',
    maxpuls: 'Maximalpuls',
    maxpulsNote: 'Schläge je Minute. Zone 1 beginnt bei 50 %, Zone 5 bei 90 %.',
    kraft: 'Krafttraining',
    kraftText: 'Die Uhr zählt Wiederholungen aus der Bewegung des Handgelenks. ' +
               'Das ist eine Schätzung: bei Curls und Bankdrücken bewegt es sich ' +
               'mit jeder Wiederholung, bei Kniebeugen mit der Stange im Nacken ' +
               'kaum. Die Pause dagegen misst sie zuverlässig.',
    pause: 'Pause',
    pauseNote: 'Nach so vielen Sekunden Pause brummt die Uhr einmal. 0 = nie.',
    empfind: 'Empfindlichkeit',
    empfindNote: 'Wie deutlich eine Bewegung sein muss, um als Wiederholung zu ' +
                 'zählen. Zählt die Uhr zu viel, stell sie träger; zählt sie zu ' +
                 'wenig, feiner.',
    traege: 'träge', normal: 'normal', fein: 'fein',
    schwimmen: 'Schwimmen',
    schwimmenText: 'Bahnen zählt die Uhr über den Kompass: bei jeder Wende dreht sich ' +
                   'die Richtung um 180 Grad. Erst die Beckenlänge macht daraus eine ' +
                   'Strecke — die Uhr kann sie nicht wissen.',
    becken: 'Beckenlänge',
    beckenNote: 'Meter je Bahn.',
    pin: 'Erinnerung in der Timeline',
    pinText: 'Ein Pin »Training« in der Timeline der Uhr, jeden Tag zur ' +
             'gewählten Zeit, mit der Aktion »Jetzt starten«. Gesetzt wird er, ' +
             'wenn die Uhr-App läuft; er gilt für die nächsten sieben Tage.',
    art: 'Sportart',
    keinPin: 'kein Pin',
    arten: ['Laufen', 'Strasse/Gravel', 'Wandern', 'Kraft', 'MTB', 'Yoga', 'Schwimmen'],
    zeit: 'Uhrzeit',
    speichern: 'Speichern',
  },
  {
    intro: 'Les zones cardiaques se calculent à partir de ta fréquence ' +
           'cardiaque maximale. Si tu la connais grâce à un test, saisis-la — ' +
           'sinon, la règle empirique pour une personne de quarante ans reste ' +
           'en place.',
    puls: 'Pouls',
    maxpuls: 'Fréquence cardiaque max.',
    maxpulsNote: 'Battements par minute. La zone 1 commence à 50 %, la zone 5 à 90 %.',
    kraft: 'Musculation',
    kraftText: 'La montre compte les répétitions à partir du mouvement du ' +
               'poignet. C\u2019est une estimation : avec les curls et le ' +
               'développé couché, il bouge à chaque répétition, avec les squats ' +
               'barre sur la nuque presque pas. Le repos, en revanche, elle le ' +
               'mesure de façon fiable.',
    pause: 'Repos',
    pauseNote: 'Après autant de secondes de repos, la montre vibre une fois. 0 = jamais.',
    empfind: 'Sensibilité',
    empfindNote: 'À quel point un mouvement doit être net pour compter comme ' +
                 'répétition. Si la montre en compte trop, rends-la moins ' +
                 'sensible ; pas assez, plus sensible.',
    traege: 'faible', normal: 'normale', fein: 'élevée',
    schwimmen: 'Natation',
    schwimmenText: 'La montre compte les longueurs avec la boussole : à chaque ' +
                   'virage, la direction change de 180 degrés. Seule la longueur ' +
                   'du bassin en fait une distance — la montre ne peut pas la ' +
                   'connaître.',
    becken: 'Longueur du bassin',
    beckenNote: 'Mètres par longueur.',
    pin: 'Rappel dans la timeline',
    pinText: 'Un pin « Entraînement » dans la timeline de la montre, chaque jour ' +
             'à l\u2019heure choisie, avec l\u2019action « Démarrer ». Il est posé ' +
             'quand l\u2019app de la montre tourne et vaut pour les sept ' +
             'prochains jours.',
    art: 'Sport',
    keinPin: 'pas de pin',
    arten: ['Course', 'Route/Gravel', 'Randonnée', 'Muscu', 'VTT', 'Yoga', 'Natation'],
    zeit: 'Heure',
    speichern: 'Enregistrer',
  },
  {
    intro: 'Le zone cardiache si calcolano dalla tua frequenza cardiaca ' +
           'massima. Se la conosci da un test, inseriscila — altrimenti resta ' +
           'la regola empirica per una persona di quarant\u2019anni.',
    puls: 'Battito',
    maxpuls: 'Frequenza cardiaca max.',
    maxpulsNote: 'Battiti al minuto. La zona 1 inizia al 50 %, la zona 5 al 90 %.',
    kraft: 'Allenamento con i pesi',
    kraftText: 'L\u2019orologio conta le ripetizioni dal movimento del polso. ' +
               'È una stima: con curl e panca piana si muove a ogni ' +
               'ripetizione, con lo squat e il bilanciere sulle spalle quasi ' +
               'per niente. Il recupero invece lo misura in modo affidabile.',
    pause: 'Recupero',
    pauseNote: 'Dopo questi secondi di recupero l\u2019orologio vibra una volta. 0 = mai.',
    empfind: 'Sensibilità',
    empfindNote: 'Quanto deve essere netto un movimento per contare come ' +
                 'ripetizione. Se l\u2019orologio ne conta troppe, rendilo meno ' +
                 'sensibile; troppo poche, più sensibile.',
    traege: 'bassa', normal: 'normale', fein: 'alta',
    schwimmen: 'Nuoto',
    schwimmenText: 'L\u2019orologio conta le vasche con la bussola: a ogni ' +
                   'virata la direzione cambia di 180 gradi. Solo la lunghezza ' +
                   'della vasca la trasforma in distanza — l\u2019orologio non ' +
                   'può saperla.',
    becken: 'Lunghezza vasca',
    beckenNote: 'Metri per vasca.',
    pin: 'Promemoria nella timeline',
    pinText: 'Un pin «Allenamento» nella timeline dell\u2019orologio, ogni ' +
             'giorno all\u2019ora scelta, con l\u2019azione «Inizia ora». Viene ' +
             'impostato quando l\u2019app dell\u2019orologio è aperta e vale per ' +
             'i prossimi sette giorni.',
    art: 'Sport',
    keinPin: 'nessun pin',
    arten: ['Corsa', 'Strada/Gravel', 'Escursione', 'Pesi', 'MTB', 'Yoga', 'Nuoto'],
    zeit: 'Ora',
    speichern: 'Salva',
  },
  {
    intro: 'Las zonas de pulso se calculan a partir de tu pulso máximo. Si lo ' +
           'conoces por una prueba, introdúcelo; si no, se queda la regla ' +
           'general para alguien de cuarenta años.',
    puls: 'Pulso',
    maxpuls: 'Pulso máximo',
    maxpulsNote: 'Pulsaciones por minuto. La zona 1 empieza en el 50 %, la zona 5 en el 90 %.',
    kraft: 'Fuerza',
    kraftText: 'El reloj cuenta las repeticiones por el movimiento de la ' +
               'muñeca. Es una estimación: con curls y press de banca se mueve ' +
               'en cada repetición, con sentadillas y la barra en la nuca casi ' +
               'nada. El descanso, en cambio, lo mide con fiabilidad.',
    pause: 'Descanso',
    pauseNote: 'Tras estos segundos de descanso el reloj vibra una vez. 0 = nunca.',
    empfind: 'Sensibilidad',
    empfindNote: 'Lo claro que debe ser un movimiento para contar como ' +
                 'repetición. Si el reloj cuenta de más, bájala; si cuenta de ' +
                 'menos, súbela.',
    traege: 'baja', normal: 'normal', fein: 'alta',
    schwimmen: 'Natación',
    schwimmenText: 'El reloj cuenta los largos con la brújula: en cada viraje ' +
                   'la dirección cambia 180 grados. Solo la longitud de la ' +
                   'piscina lo convierte en distancia; el reloj no puede saberla.',
    becken: 'Longitud de piscina',
    beckenNote: 'Metros por largo.',
    pin: 'Recordatorio en la timeline',
    pinText: 'Un pin «Entrenamiento» en la timeline del reloj, cada día a la ' +
             'hora elegida, con la acción «Empezar ya». Se pone cuando la app ' +
             'del reloj está abierta y vale para los próximos siete días.',
    art: 'Deporte',
    keinPin: 'sin pin',
    arten: ['Correr', 'Ruta/Gravel', 'Senderismo', 'Fuerza', 'MTB', 'Yoga', 'Natación'],
    zeit: 'Hora',
    speichern: 'Guardar',
  },
];

module.exports = function (lang) {
  var t = TEXT[lang] || TEXT[0];
  var artOptionen = [{ label: t.keinPin, value: '0' }];
  for (var i = 0; i < t.arten.length; i++) {
    artOptionen.push({ label: t.arten[i], value: String(i + 1) });
  }
  return [
    {
      type: 'heading',
      defaultValue: 'Kieselsport',
    },
    {
      type: 'text',
      defaultValue: t.intro,
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.puls },
        {
          type: 'slider',
          messageKey: 'MAXPULS',
          defaultValue: 190,
          label: t.maxpuls,
          description: t.maxpulsNote,
          min: 120,
          max: 220,
          step: 1,
        },
      ],
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.kraft },
        { type: 'text', defaultValue: t.kraftText },
        {
          type: 'slider',
          messageKey: 'PAUSENZIEL',
          defaultValue: 90,
          label: t.pause,
          description: t.pauseNote,
          min: 0,
          max: 300,
          step: 15,
        },
        {
          type: 'select',
          messageKey: 'EMPFIND',
          defaultValue: '2',
          label: t.empfind,
          description: t.empfindNote,
          options: [
            { label: t.traege, value: '1' },
            { label: t.normal, value: '2' },
            { label: t.fein, value: '3' },
          ],
        },
      ],
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.schwimmen },
        { type: 'text', defaultValue: t.schwimmenText },
        {
          type: 'slider',
          messageKey: 'BECKEN',
          defaultValue: 25,
          label: t.becken,
          description: t.beckenNote,
          min: 10,
          max: 50,
          step: 5,
        },
      ],
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.pin },
        { type: 'text', defaultValue: t.pinText },
        {
          type: 'select',
          messageKey: 'PIN_ART',
          defaultValue: '0',
          label: t.art,
          options: artOptionen,
        },
        {
          type: 'input',
          messageKey: 'PIN_ZEIT',
          defaultValue: '18:00',
          label: t.zeit,
          attributes: { type: 'time' },
        },
      ],
    },
    {
      type: 'submit',
      defaultValue: t.speichern,
    },
  ];
};

// Die Sportarten je Sprache, fuer die Pins in index.js - eine Liste, damit
// Konfigseite und Pin dieselben Namen tragen.
module.exports.arten = function (lang) {
  return (TEXT[lang] || TEXT[0]).arten;
};
