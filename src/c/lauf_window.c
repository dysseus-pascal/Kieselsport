#include "lauf_window.h"
#include "telefon.h"
#include "botschaft.h"
#include "schluessel.h"
#include "puls.h"
#include "thema.h"

// Der laufende Schirm.
//
// EINE ZAHL IST GROSS, DIE ANDEREN SIND KLEIN. Beim Laufen schaut man im
// Vorbeigehen hin, mit schwingendem Arm; was dann lesbar sein muss, ist die
// Zeit und der Puls. Alles andere liest man in der Pause oder danach.
//
// GEBAUT WIE EIN TIMELINE-EINTRAG (siehe thema.h): oben die Uhrzeit, dann
// eine kleine Zeile, die grosse Zahl in LECO, darunter Titel und die kleinen
// Felder - und rechts die Leiste mit dem Herz und den Tasten-Hinweisen auf
// Tastenhoehe. Drinktervall und Flynformer sehen genauso aus.
//
// DREI SEHR VERSCHIEDENE SCHIRME. 200x228 in Farbe, 144x168 schwarzweiss,
// 260x260 rund - Letzteres schneidet die Ecken ab. Deshalb wird nichts fest
// gesetzt, sondern alles aus der Fensterbreite gerechnet, und auf der runden
// Uhr bekommt jede Zeile mehr Luft an den Seiten.
//
// DIE ZAHLEN KOMMEN VOM WORKER, jede Sekunde in fuenf Nachrichten. Dieser
// Schirm misst nichts selbst; er zeigt, was ankommt, und schickt Befehle.

// Alle zwei Sekunden das Abo erneuern: bleibt es aus, hoert der Worker auf
// zu senden (siehe botschaft.h).
#define ABO_MS 2000
// So lange gilt der erste Druck auf Unten, bevor der zweite verwirft.
#define VERWERFEN_S 3

static Window *s_fenster;
static Layer *s_flaeche;
static AppTimer *s_abo;
static AppTimer *s_zu;

// Was die App selbst weiss - und was der Worker ihr sagt.
static Sportart s_art;
static bool s_bereit;          //< gewaehlt, aber noch nicht gestartet
static bool s_speichert;       //< Speichern verlangt, warte auf den Worker
static bool s_gespeichert;     //< kurz zu sehen, dann zurueck ins Menue
static time_t s_verwerfen_bis; //< zweiter Druck auf Unten bis dahin
static time_t s_gestartet;     //< wann Select gedrueckt wurde

static struct {
  bool da;
  Laufzustand zustand;
  uint16_t sekunden;
  uint16_t puls;
  bool frisch;
  uint32_t schritte;
  uint32_t meter;
  uint32_t kcal;
  uint16_t saetze, reps, laufend;
  bool ruht;
  uint16_t ruhe_s, bahnen;
  bool kompass;
  int zone;
  uint32_t beginn;
} s;

// --- Abo beim Worker ---

static void prv_abo(void *data) {
  s_abo = NULL;
  if (!s_bereit) {
    AppWorkerMessage leer = { 0, 0, 0 };
    app_worker_send_message(BefehlAbo, &leer);
  }
  s_abo = app_timer_register(ABO_MS, prv_abo, NULL);
}

static void prv_zeit(char *aus, size_t platz, uint32_t sekunden) {
  const uint32_t h = sekunden / 3600;
  const uint32_t m = (sekunden / 60) % 60;
  const uint32_t sek = sekunden % 60;
  if (h > 0) snprintf(aus, platz, "%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)sek);
  else snprintf(aus, platz, "%u:%02u", (unsigned)m, (unsigned)sek);
}

// --- Zeichnen ---

static void prv_text(GContext *ctx, const char *text, const char *schrift, GRect wo,
                     GTextAlignment wie) {
  graphics_draw_text(ctx, text, fonts_get_system_font(schrift), wo,
                     GTextOverflowModeTrailingEllipsis, wie, NULL);
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const int16_t rand = KS_RAND;
  // Die Spalte links von der Leiste; auf der runden Uhr mit viel Luft, weil
  // der Kreis die Ecken nimmt.
  const int16_t breite = bounds.size.w - KS_LEISTE_B - rand - 4;
  int16_t y = PBL_IF_ROUND_ELSE(46, 18);
  char text[24];
  const char *oben = NULL, *mitte = NULL, *unten = NULL;
  const char *fuss = NULL;

  // Die Uhrzeit oben, klein und mittig: so faengt jeder Timeline-Eintrag an.
  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  char uhr[10];
  clock_copy_time_string(uhr, sizeof(uhr));
  prv_text(ctx, uhr, FONT_KEY_GOTHIC_14,
           GRect(0, PBL_IF_ROUND_ELSE(10, 0), bounds.size.w - KS_LEISTE_B, 16),
           GTextAlignmentCenter);

  const ArtInfo *info = art_info(s_art);
  const char *gross_schrift = KS_BREIT ? FONT_KEY_LECO_36_BOLD_NUMBERS
                                       : FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM;
  const int16_t gross_h = KS_BREIT ? 46 : 38;
  const char *titel_schrift = KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD;
  const int16_t titel_h = KS_BREIT ? 30 : 24;

  if (s_gespeichert) {
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, art_name(s_art), FONT_KEY_GOTHIC_14, GRect(rand, y, breite, 16), GTextAlignmentLeft);
    y += 14;
    graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
    prv_zeit(text, sizeof(text), s.sekunden);
    prv_text(ctx, text, gross_schrift, GRect(rand, y, breite, gross_h), GTextAlignmentLeft);
    y += gross_h;
    prv_text(ctx, "Gespeichert", titel_schrift, GRect(rand, y, breite, titel_h), GTextAlignmentLeft);
    y += titel_h;
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, "geht ans Telefon", KS_BREIT ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14,
             GRect(rand, y, breite, 22), GTextAlignmentLeft);
    thema_leiste(ctx, bounds, false, GColorWhite, NULL, NULL, NULL);
    return;
  }

  if (!s_bereit && !s.da) {
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, "Kieselsport", FONT_KEY_GOTHIC_14, GRect(rand, y, breite, 16), GTextAlignmentLeft);
    y += 14 + gross_h;
    graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
    prv_text(ctx, "Hole den Stand …", titel_schrift, GRect(rand, y, breite, titel_h), GTextAlignmentLeft);
    thema_leiste(ctx, bounds, false, GColorWhite, NULL, NULL, NULL);
    return;
  }

  // --- Die kleine Zeile: Art und Zustand ---
  char zeile[32];
  if (s_bereit) snprintf(zeile, sizeof(zeile), "%s · bereit", art_name(s_art));
  else if (s.zustand == LaufPause) snprintf(zeile, sizeof(zeile), "%s · Pause", art_name(s_art));
  else if (info->reps) {
    // Beim Kraft steht die Gesamtzeit hier oben: gross ist unten der Satz.
    char zeit[16];
    prv_zeit(zeit, sizeof(zeit), s.sekunden);
    snprintf(zeile, sizeof(zeile), "%s · %s", art_name(s_art), zeit);
  } else snprintf(zeile, sizeof(zeile), "%s", art_name(s_art));
  graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
  prv_text(ctx, zeile, FONT_KEY_GOTHIC_14, GRect(rand, y, breite, 16), GTextAlignmentLeft);
  y += 14;

  // --- Die grosse Zahl ---
  //
  // SIE IST NICHT IMMER DIE ZEIT. Beim Krafttraining schaut man waehrend
  // eines Satzes auf die Wiederholungen und danach auf die Pause - die
  // Gesamtzeit interessiert erst hinterher. Der Schirm zeigt deshalb, was
  // gerade gilt, und die Zeit rutscht in die kleine Zeile darueber.
  const char *gross_name = NULL;
  if (info->reps && !s_bereit && s.zustand != LaufPause) {
    if (s.ruht) {
      prv_zeit(text, sizeof(text), s.ruhe_s);
      gross_name = "Pause";
    } else {
      snprintf(text, sizeof(text), "%u", (unsigned)s.laufend);
      gross_name = "Wdh.";
    }
  } else {
    prv_zeit(text, sizeof(text), s_bereit ? 0 : s.sekunden);
  }
  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  // Mit Stunden wird die Zeit auf der schmalen Uhr zu breit fuer die Spalte.
  const bool lang = strlen(text) > 5;
  prv_text(ctx, text, (lang && !KS_BREIT) ? FONT_KEY_LECO_20_BOLD_NUMBERS : gross_schrift,
           GRect(rand, y + ((lang && !KS_BREIT) ? 8 : 0), breite, gross_h), GTextAlignmentLeft);
  if (gross_name) {
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, gross_name, FONT_KEY_GOTHIC_14, GRect(rand, y + gross_h - 20, breite, 16),
             GTextAlignmentRight);
  }
  y += gross_h;

  // --- Der Titel: Puls und Zone, mit dem Balken darunter ---
  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  if (s_bereit) {
    prv_text(ctx, "Select startet", titel_schrift, GRect(rand, y, breite, titel_h), GTextAlignmentLeft);
    y += titel_h;
  } else if (s.puls > 0) {
    // EIN ALTER WERT STEHT GRAU DA und traegt keine Zone. Der Sensor behaelt
    // den letzten guten Wert, wenn er am Lenker nichts Brauchbares misst -
    // eine halbe Stunde "75" in Schwarz saehe aus wie eine Messung.
    if (!s.frisch) {
      graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
      snprintf(text, sizeof(text), "%u · alt", (unsigned)s.puls);
    } else if (s.zone == 0) {
      snprintf(text, sizeof(text), "%u · < Zone 1", (unsigned)s.puls);
    } else {
      snprintf(text, sizeof(text), "%u · Zone %d", (unsigned)s.puls, s.zone);
    }
    prv_text(ctx, text, titel_schrift, GRect(rand, y, breite, titel_h), GTextAlignmentLeft);
    y += titel_h;

    // Der Balken: fuenf Felder, das erreichte gefuellt. Auf schwarzweissen
    // Uhren ist er die Zone, weil dort keine Farbe sie tragen kann.
    const int16_t fach = breite / KS_ZONEN;
    for (int z = 1; z <= KS_ZONEN; z++) {
      const GRect kasten = GRect(rand + (z - 1) * fach, y, fach - 2, 6);
      if (z <= s.zone) {
        graphics_context_set_fill_color(ctx, thema_zonenfarbe(z));
        graphics_fill_rect(ctx, kasten, 0, GCornerNone);
      } else {
        graphics_context_set_stroke_color(ctx, KS_FARBE_NEBEN);
        graphics_draw_rect(ctx, kasten);
      }
    }
    y += 12;
  } else {
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, "kein Puls", titel_schrift, GRect(rand, y, breite, titel_h), GTextAlignmentLeft);
    y += titel_h;
  }

  // --- Die kleinen Felder, nur was die Art hergibt ---
  //
  // EINE REIHE, SO VIELE SPALTEN WIE FELDER. Der erste Entwurf setzte zwei
  // je Reihe - auf flint fiel damit das dritte Feld unten aus dem Schirm.
  // 168 Punkte tragen keine zwei Reihen mehr, 228 schon; also richtet sich
  // die Aufteilung nach der Zahl der Felder und nicht nach einer Annahme.
  const char *namen[3];
  char werte[3][16];
  int felder = 0;

  if (info->reps) {
    namen[felder] = "Saetze";
    snprintf(werte[felder], sizeof(werte[0]), "%u", (unsigned)s.saetze);
    felder++;
    // NICHT NOCHMAL "Wdh.": das steht schon gross oben. Hier zaehlt das
    // Training zusammen, dort der laufende Satz.
    namen[felder] = "Gesamt";
    snprintf(werte[felder], sizeof(werte[0]), "%u", (unsigned)s.reps);
    felder++;
  }
  if (info->bahnen) {
    namen[felder] = "Bahnen";
    snprintf(werte[felder], sizeof(werte[0]), "%u", (unsigned)s.bahnen);
    felder++;
    namen[felder] = "m";
    snprintf(werte[felder], sizeof(werte[0]), "%u", (unsigned)s.meter);
    felder++;
  }
  if (info->schritte) {
    // Auf schmalen Spalten abgekuerzt: "Schrit..." sagt weniger als "Schr."
    namen[felder] = (breite / (info->distanz ? 3 : 2)) < 50 ? "Schr." : "Schritte";
    snprintf(werte[felder], sizeof(werte[0]), "%u", (unsigned)s.schritte);
    felder++;
  }
  if (info->distanz) {
    namen[felder] = "km";
    snprintf(werte[felder], sizeof(werte[0]), "%u.%02u", (unsigned)(s.meter / 1000),
             (unsigned)((s.meter % 1000) / 10));
    felder++;
  }
  namen[felder] = "kcal";
  snprintf(werte[felder], sizeof(werte[0]), "%u", (unsigned)s.kcal);
  felder++;

  const int16_t spaltenbreite = breite / felder;
  // Die Zahl schrumpft mit der Spalte: "1234" in 24 Punkt braucht gut
  // vierzig Punkte Breite, und drei Spalten auf 144 lassen keine vierzig.
  const char *zahlenschrift = spaltenbreite >= 62 ? FONT_KEY_GOTHIC_24_BOLD
                            : spaltenbreite >= 40 ? FONT_KEY_GOTHIC_18_BOLD
                                                  : FONT_KEY_GOTHIC_14_BOLD;

  for (int i = 0; i < felder; i++) {
    const GRect kasten = GRect(rand + i * spaltenbreite, y, spaltenbreite - 3, 40);
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, namen[i], FONT_KEY_GOTHIC_14,
             GRect(kasten.origin.x, kasten.origin.y, kasten.size.w, 16), GTextAlignmentLeft);
    graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
    prv_text(ctx, werte[i], zahlenschrift,
             GRect(kasten.origin.x, kasten.origin.y + 13, kasten.size.w, 28), GTextAlignmentLeft);
  }

  // --- Was die Tasten gerade tun ---
  //
  // DIE LEISTE SAGT ES, auf der Hoehe der Taste, statt es vorauszusetzen.
  // Drei Tasten mit drei Bedeutungen, die vom Zustand abhaengen - das merkt
  // sich niemand, und ein Fehlgriff kostete frueher ein Training. Was eine
  // Taste gerade nicht tut, steht auch nicht da.
  if (s_speichert) {
    fuss = "Speichere …";
  } else if (s_verwerfen_bis > time(NULL)) {
    mitte = "Weiter";
    unten = "Weg?";
    fuss = "Nochmal Unten: verwerfen";
  } else if (s.zustand == LaufPause) {
    oben = "Ende";
    mitte = "Weiter";
    unten = "Weg";
    fuss = "Ende speichert, Weg verwirft";
  } else if (s_bereit) {
    mitte = "Start";
  } else {
    mitte = "Pause";
    if (info->bahnen && !s.kompass) {
      // LIEBER SAGEN, DASS NICHT GEZAEHLT WIRD, als eine Null zeigen. Eine
      // Null bei den Bahnen sieht aus wie "du bist noch keine geschwommen".
      fuss = "Kompass nicht bereit";
    }
  }
  if (fuss) {
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, fuss, FONT_KEY_GOTHIC_14,
             GRect(rand, bounds.size.h - PBL_IF_ROUND_ELSE(40, 18), breite, 16), GTextAlignmentLeft);
  }

  const bool herz = !s_bereit && s.puls > 0 && s.frisch;
  thema_leiste(ctx, bounds, herz, thema_zonenfarbe(s.zone), oben, mitte, unten);
}

// --- Brummen ---

// DER WORKER DARF NICHT BRUMMEN, die App schon. Er sagt, was faellig ist:
// beim Zonenwechsel hoch zwei kurze, runter eine - wer laeuft, soll sie
// unterscheiden koennen, ohne hinzusehen. Und beim Kraft einmal, wenn die
// Pause um ist.
static void prv_brummen(uint8_t was) {
  switch (was) {
    case BrummZoneHoch: {
      static const uint32_t hoch[] = { 60, 80, 60 };
      VibePattern muster = { .durations = hoch, .num_segments = 3 };
      vibes_enqueue_custom_pattern(muster);
      break;
    }
    case BrummZoneRunter:
      vibes_short_pulse();
      break;
    case BrummPauseUm: {
      static const uint32_t pause[] = { 80, 100, 80 };
      VibePattern muster = { .durations = pause, .num_segments = 3 };
      vibes_enqueue_custom_pattern(muster);
      break;
    }
    default:
      break;
  }
}

// --- Zum Worker und zurueck ---

static void prv_start(void) {
  const uint32_t beginn = (uint32_t)time(NULL);
  // ERST MELDEN, DANN STARTEN. Jede Sekunde, die das Telefon spaeter
  // anfaengt, fehlt der Strecke am Anfang.
  telefon_melde_zustand(ZustandStart, (uint8_t)s_art, beginn);

  // Die Bestellung fuer den Worker liegt im Persist: er liest sie beim
  // Hochfahren. Lief er schon (ein alter, muessiger), bekommt er sie gesagt.
  persist_write_int(PERSIST_START_ART, (int32_t)s_art);
  persist_write_int(PERSIST_START_BEGINN, (int32_t)beginn);
  const AppWorkerResult r = app_worker_launch();
  if (r == APP_WORKER_RESULT_ALREADY_RUNNING) {
    AppWorkerMessage leer = { 0, 0, 0 };
    app_worker_send_message(BefehlStart, &leer);
  } else if (r != APP_WORKER_RESULT_SUCCESS && r != APP_WORKER_RESULT_ASKING_CONFIRMATION) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Worker startet nicht: %d", (int)r);
  }

  s_bereit = false;
  s_gestartet = (time_t)beginn;
  memset(&s, 0, sizeof(s));
  s.beginn = beginn;
  s.zustand = LaufLaeuft;
  s.da = true;
  if (s_abo) app_timer_cancel(s_abo);
  s_abo = app_timer_register(300, prv_abo, NULL);
}

static void prv_zu(void *data) {
  s_zu = NULL;
  if (s_fenster) window_stack_remove(s_fenster, true);
}

void lauf_window_nachricht(uint16_t typ, AppWorkerMessage *d) {
  if (!s_fenster) return;
  switch (typ) {
    case BotStand1:
      s.zustand = (Laufzustand)(d->data0 & 0x0F);
      s_art = (Sportart)(d->data0 >> 4);
      s.sekunden = d->data1;
      s.puls = d->data2 & 0x7FFF;
      s.frisch = (d->data2 & 0x8000) != 0;
      // EIN WORKER OHNE TRAINING: die App kam zurueck, aber es laeuft
      // nichts mehr. Dann gehoert er beendet und der Schirm zu.
      // Nicht in den ersten Sekunden nach dem Start: da kann der Worker
      // die Bestellung noch nicht gelesen haben.
      if (s.zustand == LaufAus && !s_bereit && !s_speichert && !s_gespeichert &&
          time(NULL) - s_gestartet > 3) {
        app_worker_kill();
        if (!s_zu) s_zu = app_timer_register(10, prv_zu, NULL);
        return;
      }
      s.da = true;
      break;
    case BotStand2:
      s.schritte = d->data0;
      s.meter = (uint32_t)d->data1 * 10;
      s.kcal = d->data2;
      break;
    case BotStand3:
      s.saetze = d->data0;
      s.reps = d->data1;
      s.laufend = d->data2 & 0x7FFF;
      s.ruht = (d->data2 & 0x8000) != 0;
      break;
    case BotStand4:
      s.ruhe_s = d->data0;
      s.bahnen = d->data1;
      s.kompass = (d->data2 & 0x0F) != 0;
      s.zone = d->data2 >> 8;
      prv_brummen((d->data2 >> 4) & 0x0F);
      break;
    case BotStand5:
      s.beginn = ((uint32_t)d->data0 << 16) | d->data1;
      break;
    case BotFertig:
      // Der Worker hat die Zusammenfassung in den Persist gelegt; von hier
      // aus geht sie ans Telefon - und faellt nicht mehr weg.
      s_speichert = false;
      s_gespeichert = true;
      app_worker_kill();
      telefon_nachsenden();
      if (!s_zu) s_zu = app_timer_register(1800, prv_zu, NULL);
      break;
    case BotVerworfen:
      app_worker_kill();
      telefon_melde_zustand(ZustandStop, (uint8_t)s_art, s.beginn);
      s_speichert = false;
      if (!s_zu) s_zu = app_timer_register(10, prv_zu, NULL);
      break;
    default:
      return;
  }
  if (s_flaeche) layer_mark_dirty(s_flaeche);
}

// --- Bedienung ---

static void prv_select(ClickRecognizerRef anlass, void *context) {
  if (s_speichert || s_gespeichert) return;
  if (s_bereit) {
    prv_start();
  } else {
    AppWorkerMessage leer = { 0, 0, 0 };
    app_worker_send_message(BefehlPause, &leer);
    // Die Meldung ans Telefon gleich, nicht erst nach der Antwort des
    // Workers: die Aufzeichnung soll mit dem Druck anhalten.
    const bool wird_pause = s.zustand == LaufLaeuft;
    telefon_melde_zustand(wird_pause ? ZustandPause : ZustandWeiter, (uint8_t)s_art, s.beginn);
    s.zustand = wird_pause ? LaufPause : LaufLaeuft;
    s_verwerfen_bis = 0;
  }
  layer_mark_dirty(s_flaeche);
}

static void prv_oben(ClickRecognizerRef anlass, void *context) {
  // SPEICHERN NUR AUS DER PAUSE. Ein Druck beim Laufen soll nichts tun -
  // die Hand am Aermel trifft die obere Taste leicht.
  if (s_bereit || s_speichert || s_gespeichert || s.zustand != LaufPause) return;
  AppWorkerMessage leer = { 0, 0, 0 };
  s_speichert = true;
  s_verwerfen_bis = 0;
  app_worker_send_message(BefehlSpeichern, &leer);
  layer_mark_dirty(s_flaeche);
}

static void prv_unten(ClickRecognizerRef anlass, void *context) {
  if (s_bereit || s_speichert || s_gespeichert || s.zustand != LaufPause) return;
  const time_t jetzt = time(NULL);
  if (s_verwerfen_bis > jetzt) {
    // ZWEIMAL, WEIL ES DAS TRAINING KOSTET. Der erste Druck fragt, der
    // zweite tut es - und nur, wenn er gleich kommt.
    AppWorkerMessage leer = { 0, 0, 0 };
    s_verwerfen_bis = 0;
    app_worker_send_message(BefehlVerwerfen, &leer);
  } else {
    s_verwerfen_bis = jetzt + VERWERFEN_S;
  }
  layer_mark_dirty(s_flaeche);
}

static void prv_zurueck(ClickRecognizerRef anlass, void *context) {
  if (s_bereit || s_gespeichert) {
    window_stack_pop(true);
    return;
  }
  if (s_speichert) return;
  // IN DEN HINTERGRUND, NICHT BEENDEN. Der Worker laeuft weiter; die App
  // geht zu, und im Startmenue steht, dass gezaehlt wird. Beendet wird nur
  // aus der Pause, mit Oben oder Unten.
  window_stack_pop_all(true);
}

static void prv_tasten(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_UP, prv_oben);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_unten);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_zurueck);
}

// --- Fenster ---

static void prv_laden(Window *fenster) {
  Layer *wurzel = window_get_root_layer(fenster);
  const GRect bounds = layer_get_bounds(wurzel);

  s_flaeche = layer_create(bounds);
  layer_set_update_proc(s_flaeche, prv_zeichne);
  layer_add_child(wurzel, s_flaeche);
}

static void prv_entladen(Window *fenster) {
  if (s_abo) { app_timer_cancel(s_abo); s_abo = NULL; }
  if (s_zu) { app_timer_cancel(s_zu); s_zu = NULL; }
  layer_destroy(s_flaeche);
  s_flaeche = NULL;
  window_destroy(s_fenster);
  s_fenster = NULL;
}

static void prv_oeffnen(void) {
  s_speichert = false;
  s_gespeichert = false;
  s_verwerfen_bis = 0;
  memset(&s, 0, sizeof(s));

  s_fenster = window_create();
  window_set_background_color(s_fenster, KS_FARBE_GRUND);
  window_set_click_config_provider(s_fenster, prv_tasten);
  window_set_window_handlers(s_fenster, (WindowHandlers) {
    .load = prv_laden,
    .unload = prv_entladen,
  });
  window_stack_push(s_fenster, true);
}

#ifdef KS_DEMO
static void prv_demo_start(void *data) {
  if (s_fenster && s_bereit) prv_start();
}
#endif

void lauf_window_zeige(Sportart art) {
  s_art = art;
  s_bereit = true;
  s_gestartet = 0;
  prv_oeffnen();
#ifdef KS_DEMO
  // Im Pruefbau gleich starten: dort drueckt niemand Select.
  app_timer_register(500, prv_demo_start, NULL);
#endif
}

void lauf_window_zeige_laufend(void) {
  s_art = ArtLaufen;
  s_bereit = false;
  s_gestartet = 0;
  prv_oeffnen();
  // Sofort fragen, nicht erst nach zwei Sekunden: der Schirm stuende sonst
  // leer, waehrend man schon hinschaut.
  s_abo = app_timer_register(50, prv_abo, NULL);
}

bool lauf_window_laeuft(void) {
  // Ohne Stand vom Worker gilt: was gestartet wurde, laeuft - der Worker
  // sagt es sonst selbst, wenn nicht.
  return !s_bereit && !s_gespeichert && (!s.da || s.zustand != LaufAus);
}


Sportart lauf_window_art(void) { return s_art; }
uint32_t lauf_window_beginn(void) { return s.beginn; }
