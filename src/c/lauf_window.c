#include "lauf_window.h"
#include "telefon.h"
#include "botschaft.h"
#include "schluessel.h"
#include "puls.h"

// Der laufende Schirm.
//
// EINE ZAHL IST GROSS, DIE ANDEREN SIND KLEIN. Beim Laufen schaut man im
// Vorbeigehen hin, mit schwingendem Arm; was dann lesbar sein muss, ist die
// Zeit und der Puls. Alles andere liest man in der Pause oder danach.
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
static TextLayer *s_kopf;
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

// --- Farben ---

static GColor prv_zonenfarbe(int zone) {
#if defined(PBL_COLOR)
  switch (zone) {
    case 1: return GColorPictonBlue;
    case 2: return GColorJaegerGreen;
    case 3: return GColorLimerick;
    case 4: return GColorChromeYellow;
    case 5: return GColorFolly;
    default: return GColorLightGray;
  }
#else
  // SCHWARZWEISS: eine Farbe, die es nicht gibt, ist keine Auskunft. Auf
  // flint traegt die Ziffer neben dem Puls die Zone, und die Balkenlaenge
  // darunter zeigt sie noch einmal.
  return GColorBlack;
#endif
}

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

static void prv_hinweis(GContext *ctx, const GRect *bounds, int16_t rand, int16_t breite,
                        const char *oben, const char *unten) {
  // Der Hinweis liegt UNTEN und ueber allem: auf flint reichen die Felder
  // bis dorthin, und ein weisser Grund haelt ihn lesbar.
  const int16_t h = PBL_IF_ROUND_ELSE(38, 34);
  const GRect kasten = GRect(0, bounds->size.h - h - PBL_IF_ROUND_ELSE(22, 2), bounds->size.w, h + 4);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, kasten, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  if (oben) {
    graphics_draw_text(ctx, oben, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(rand, kasten.origin.y, breite, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
  if (unten) {
    graphics_draw_text(ctx, unten, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(rand - 4, kasten.origin.y + 16, breite + 8, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const int16_t rand = PBL_IF_ROUND_ELSE(bounds.size.w / 6, 8);
  const int16_t breite = bounds.size.w - 2 * rand;
  // Auf der runden Uhr tiefer anfangen: der Block ist rund 150 Punkte hoch,
  // auf 260 Punkten stuende er sonst oben und liesse das untere Drittel
  // leer - und genau dort ist der Kreis am breitesten.
  int16_t y = PBL_IF_ROUND_ELSE(52, 4);
  char text[24];

  graphics_context_set_text_color(ctx, GColorBlack);

  if (s_gespeichert) {
    graphics_draw_text(ctx, "Gespeichert", fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                       GRect(rand, bounds.size.h / 2 - 30, breite, 34),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "geht ans Telefon", fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(rand, bounds.size.h / 2 + 6, breite, 24),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  if (!s_bereit && !s.da) {
    graphics_draw_text(ctx, "Hole den Stand …", fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(rand, bounds.size.h / 2 - 12, breite, 24),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  const ArtInfo *info = art_info(s_art);

  // --- Die grosse Zahl ---
  //
  // SIE IST NICHT IMMER DIE ZEIT. Beim Krafttraining schaut man waehrend
  // eines Satzes auf die Wiederholungen und danach auf die Pause - die
  // Gesamtzeit interessiert erst hinterher. Der Schirm zeigt deshalb, was
  // gerade gilt, und die Zeit rutscht daneben.
  const char *gross_name = NULL;
  if (info->reps && !s_bereit) {
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
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS),
                     GRect(rand, y, breite, 44),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += 42;

  if (gross_name) {
    char zeit[16];
    prv_zeit(zeit, sizeof(zeit), s.sekunden);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, gross_name, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(rand, y - 4, breite / 2, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, zeit, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(rand + breite / 2, y - 4, breite / 2, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    graphics_context_set_text_color(ctx, GColorBlack);
    y += 14;
  }

  // --- Puls mit Zonenbalken ---
  if (s_bereit) {
    graphics_draw_text(ctx, "Select startet", fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(rand, y, breite, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    y += 26;
  } else if (s.puls > 0) {
    // EIN ALTER WERT STEHT GRAU DA und traegt keine Zone. Der Sensor behaelt
    // den letzten guten Wert, wenn er am Lenker nichts Brauchbares misst -
    // eine halbe Stunde "75" in Schwarz saehe aus wie eine Messung.
    graphics_context_set_text_color(ctx, s.frisch ? GColorBlack : GColorDarkGray);
    snprintf(text, sizeof(text), "%u", (unsigned)s.puls);
    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS),
                       GRect(rand, y, breite / 2, 32),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    if (!s.frisch) snprintf(text, sizeof(text), "alt");
    else if (s.zone == 0) snprintf(text, sizeof(text), "< Zone 1");
    else snprintf(text, sizeof(text), "Zone %d", s.zone);
    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(rand + breite / 2, y + 8, breite / 2, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    graphics_context_set_text_color(ctx, GColorBlack);
    y += 34;

    // Der Balken: fuenf Felder, das erreichte gefuellt. Auf schwarzweissen
    // Uhren ist er die Zone, weil dort keine Farbe sie tragen kann.
    const int16_t fach = breite / KS_ZONEN;
    for (int z = 1; z <= KS_ZONEN; z++) {
      const GRect kasten = GRect(rand + (z - 1) * fach, y, fach - 2, 6);
      if (z <= s.zone) {
        graphics_context_set_fill_color(ctx, prv_zonenfarbe(z));
        graphics_fill_rect(ctx, kasten, 0, GCornerNone);
      } else {
        graphics_context_set_stroke_color(ctx, GColorDarkGray);
        graphics_draw_rect(ctx, kasten);
      }
    }
    y += 12;
  } else {
    graphics_draw_text(ctx, "kein Puls", fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(rand, y, breite, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    y += 26;
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
  GFont zahlenschrift = spaltenbreite >= 62
      ? fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD)
      : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  for (int i = 0; i < felder; i++) {
    const GRect kasten = GRect(rand + i * spaltenbreite, y, spaltenbreite - 3, 40);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, namen[i], fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(kasten.origin.x, kasten.origin.y, kasten.size.w, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, werte[i], zahlenschrift,
                       GRect(kasten.origin.x, kasten.origin.y + 13, kasten.size.w, 28),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // --- Was die Tasten gerade tun ---
  //
  // DER SCHIRM SAGT ES, statt es vorauszusetzen. Drei Tasten mit drei
  // Bedeutungen, die vom Zustand abhaengen - das merkt sich niemand, und
  // ein Fehlgriff kostete frueher ein Training.
  if (s_speichert) {
    prv_hinweis(ctx, &bounds, rand, breite, "Speichere …", NULL);
  } else if (s_verwerfen_bis > time(NULL)) {
    prv_hinweis(ctx, &bounds, rand, breite, "Nochmal Unten: verwerfen", "Select: weiter");
  } else if (s.zustand == LaufPause) {
    prv_hinweis(ctx, &bounds, rand, breite, "PAUSE  ·  Select: weiter",
                "Oben: speichern  ·  Unten: verwerfen");
  } else if (info->bahnen && !s.kompass) {
    // LIEBER SAGEN, DASS NICHT GEZAEHLT WIRD, als eine Null zeigen. Eine
    // Null bei den Bahnen sieht aus wie "du bist noch keine geschwommen".
    prv_hinweis(ctx, &bounds, rand, breite, "Kompass nicht bereit", "Select: Pause");
  } else {
    prv_hinweis(ctx, &bounds, rand, breite, NULL,
                s_bereit ? "Zurueck: Menue" : "Select: Pause  ·  Zurueck: Hintergrund");
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
      s.kompass = (d->data2 & 0xFF) != 0;
      s.zone = d->data2 >> 8;
      break;
    case BotStand5:
      s.beginn = ((uint32_t)d->data0 << 16) | d->data1;
      if (s_kopf) text_layer_set_text(s_kopf, art_name(s_art));
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

  s_kopf = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(14, 0), bounds.size.w, 20));
  text_layer_set_text(s_kopf, art_name(s_art));
  text_layer_set_text_alignment(s_kopf, GTextAlignmentCenter);
  text_layer_set_font(s_kopf, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_background_color(s_kopf, GColorClear);
  layer_add_child(wurzel, text_layer_get_layer(s_kopf));

  s_flaeche = layer_create(GRect(0, PBL_IF_ROUND_ELSE(20, 18),
                                 bounds.size.w, bounds.size.h - 18));
  layer_set_update_proc(s_flaeche, prv_zeichne);
  layer_add_child(wurzel, s_flaeche);
}

static void prv_entladen(Window *fenster) {
  if (s_abo) { app_timer_cancel(s_abo); s_abo = NULL; }
  if (s_zu) { app_timer_cancel(s_zu); s_zu = NULL; }
  layer_destroy(s_flaeche);
  text_layer_destroy(s_kopf);
  s_flaeche = NULL;
  s_kopf = NULL;
  window_destroy(s_fenster);
  s_fenster = NULL;
}

static void prv_oeffnen(void) {
  s_speichert = false;
  s_gespeichert = false;
  s_verwerfen_bis = 0;
  memset(&s, 0, sizeof(s));

  s_fenster = window_create();
  window_set_background_color(s_fenster, GColorWhite);
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
