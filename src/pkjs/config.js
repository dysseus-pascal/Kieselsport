// Die Konfigseite: Puls, Krafttraining, Schwimmen.
//
// DER MAXIMALPULS IST PERSÖNLICH. »220 minus Alter« ist eine Faustformel mit
// einer Streuung von gut zehn Schlägen nach oben wie unten; aus ihr die Zonen
// zu rechnen und sie dann als Messung zu zeigen, wäre eine Behauptung. Wer
// seinen Wert aus einem Test kennt, trägt ihn ein. Wer nicht, lässt die
// Vorgabe stehen und weiss wenigstens, woran er ist.

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Kieselsport',
  },
  {
    type: 'text',
    defaultValue:
      'Die Pulszonen rechnen sich aus deinem Maximalpuls. Kennst du ihn aus ' +
      'einem Test, trag ihn ein — sonst bleibt die Faustformel für einen ' +
      'Vierzigjährigen stehen.',
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Puls',
      },
      {
        type: 'slider',
        messageKey: 'MAXPULS',
        defaultValue: 190,
        label: 'Maximalpuls',
        description: 'Schläge je Minute. Zone 1 beginnt bei 50 %, Zone 5 bei 90 %.',
        min: 120,
        max: 220,
        step: 1,
      },
    ],
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Krafttraining',
      },
      {
        type: 'text',
        defaultValue:
          'Die Uhr zählt Wiederholungen aus der Bewegung des Handgelenks. ' +
          'Das ist eine Schätzung: bei Curls und Bankdrücken bewegt es sich ' +
          'mit jeder Wiederholung, bei Kniebeugen mit der Stange im Nacken ' +
          'kaum. Die Pause dagegen misst sie zuverlässig.',
      },
      {
        type: 'slider',
        messageKey: 'PAUSENZIEL',
        defaultValue: 90,
        label: 'Pause',
        description: 'Nach so vielen Sekunden Pause brummt die Uhr einmal. 0 = nie.',
        min: 0,
        max: 300,
        step: 15,
      },
      {
        type: 'select',
        messageKey: 'EMPFIND',
        defaultValue: '2',
        label: 'Empfindlichkeit',
        description:
          'Wie deutlich eine Bewegung sein muss, um als Wiederholung zu ' +
          'zählen. Zählt die Uhr zu viel, stell sie träger; zählt sie zu ' +
          'wenig, feiner.',
        options: [
          { label: 'träge', value: '1' },
          { label: 'normal', value: '2' },
          { label: 'fein', value: '3' },
        ],
      },
    ],
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Schwimmen',
      },
      {
        type: 'text',
        defaultValue:
          'Bahnen zählt die Uhr über den Kompass: bei jeder Wende dreht sich ' +
          'die Richtung um 180 Grad. Erst die Beckenlänge macht daraus eine ' +
          'Strecke — die Uhr kann sie nicht wissen.',
      },
      {
        type: 'slider',
        messageKey: 'BECKEN',
        defaultValue: 25,
        label: 'Beckenlänge',
        description: 'Meter je Bahn.',
        min: 10,
        max: 50,
        step: 5,
      },
    ],
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Erinnerung in der Timeline',
      },
      {
        type: 'text',
        defaultValue:
          'Ein Pin »Training« in der Timeline der Uhr, jeden Tag zur ' +
          'gewählten Zeit, mit der Aktion »Jetzt starten«. Gesetzt wird er, ' +
          'wenn die Uhr-App läuft; er gilt für die nächsten sieben Tage.',
      },
      {
        type: 'select',
        messageKey: 'PIN_ART',
        defaultValue: '0',
        label: 'Sportart',
        options: [
          { label: 'kein Pin', value: '0' },
          { label: 'Laufen', value: '1' },
          { label: 'Strasse/Gravel', value: '2' },
          { label: 'Wandern', value: '3' },
          { label: 'Kraft', value: '4' },
          { label: 'MTB', value: '5' },
          { label: 'Yoga', value: '6' },
          { label: 'Schwimmen', value: '7' },
        ],
      },
      {
        type: 'input',
        messageKey: 'PIN_ZEIT',
        defaultValue: '18:00',
        label: 'Uhrzeit',
        attributes: { type: 'time' },
      },
    ],
  },
  {
    type: 'submit',
    defaultValue: 'Speichern',
  },
];
