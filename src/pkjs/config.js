// Die Konfigseite: genau eine Einstellung.
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
    type: 'submit',
    defaultValue: 'Speichern',
  },
];
