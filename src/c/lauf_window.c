#include "lauf_window.h"
#include "telefon.h"
#include "botschaft.h"
#include "schluessel.h"
#include "puls.h"
#include "thema.h"
#include "strings.h"
#include "symbole.h"
#include "kurve.h"

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
static bool s_gespeichert;     //< zu sehen, bis das Telefon bestaetigt hat
static time_t s_gespeichert_seit;
// So lange wartet der Schirm nach dem Speichern auf die Bestaetigung des
// Telefons, bevor die App von selbst zugeht. Die Zusammenfassung liegt
// ohnehin im Persist und geht beim naechsten Oeffnen - aber wer sie JETZT
// auf dem Telefon sehen will, soll die Uhr nicht erst nochmal in die Hand
// nehmen muessen.
#define BESTAETIGUNG_S 20
static time_t s_verwerfen_bis; //< zweiter Druck auf Unten bis dahin
static bool s_bestaetigt;      //< das Telefon hat die Aufzeichnung
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
  bool sparsam;      //< Akku schwach: Puls nur alle fuenf Sekunden
  uint16_t hrv_ms, hrv_n;  //< Yoga
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

// --- Die Seiten ---
//
// GEBAUT WIE DIE NEUE WORKOUT-APP DER PEBBLE (PebbleOS #2037): oben ein
// Balken in der Farbe der Art mit Zeichen und Namen, darunter EINE grosse
// Zahl mit ihrem Namen, dann die Pulszeile - Herz, Puls in der Farbe der
// Zone, fuenf Saeulen fuer die Zonen -, unten zwei kleine Zahlen und ganz
// unten die Seiten als Striche. Unten blaettert; die letzte Seite ist der
// Puls allein, gross, mit den Zonen als breite Saeulen.
//
// Die Leiste rechts gibt es nur noch in der Pause: dort tun die Tasten
// etwas anderes als sonst, und das muss dastehen. Beim Laufen braucht der
// Schirm die ganze Breite.

typedef enum {
  FeldKeins = 0,
  FeldDauer,
  FeldPuls,
  FeldDistanz,
  FeldTempo,
  FeldSchritte,
  FeldKcal,
  FeldSatz,      //< Kraft: Wiederholungen im Satz - oder die Pause danach
  FeldSaetze,
  FeldGesamt,
  FeldBahnen,
  FeldMeter,
  FeldHrv,
  FeldUhr,
} Feldart;

typedef struct {
  Feldart gross, links, rechts;
  bool herz;     //< die Pulsseite
} Seite;

#define KS_SEITEN_MAX 4

// Was jede Art zeigt, das Wichtigste zuerst: die erste Seite ist die, auf
// die man im Vorbeischwingen schaut. Die Pulsseite ist immer die letzte.
static int prv_seiten(Seite *aus) {
  int n = 0;
  switch (s_art) {
    case ArtLaufen:
    case ArtWandern:
      aus[n++] = (Seite){ FeldDauer, FeldDistanz, FeldTempo, false };
      aus[n++] = (Seite){ FeldDistanz, FeldSchritte, FeldKcal, false };
      break;
    case ArtKraft:
      aus[n++] = (Seite){ FeldSatz, FeldSaetze, FeldGesamt, false };
      aus[n++] = (Seite){ FeldDauer, FeldSaetze, FeldKcal, false };
      break;
    case ArtYoga:
      aus[n++] = (Seite){ FeldDauer, FeldHrv, FeldKcal, false };
      break;
    case ArtSchwimmen:
      aus[n++] = (Seite){ FeldDauer, FeldBahnen, FeldMeter, false };
      aus[n++] = (Seite){ FeldBahnen, FeldMeter, FeldKcal, false };
      break;
    default:   // Strasse/Gravel, MTB: Zeit, Puls, Kalorien - wenig, aber wahr
      aus[n++] = (Seite){ FeldDauer, FeldKcal, FeldUhr, false };
      break;
  }
  aus[n++] = (Seite){ FeldPuls, FeldDauer, FeldKcal, true };
  return n;
}

static uint8_t s_seite;   //< welche Seite gerade steht

static StringId prv_zonenname(int zone) {
  static const StringId namen[KS_ZONEN] = { STR_ZONE_1, STR_ZONE_2, STR_ZONE_3, STR_ZONE_4, STR_ZONE_5 };
  return (zone >= 1 && zone <= KS_ZONEN) ? namen[zone - 1] : STR_L_PULS;
}

typedef struct {
  const char *name;
  char zahl[16];
  const char *einheit;   //< klein hinter dem Namen, oder NULL
} Feld;

// Der Puls: frisch, alt (grau, keine Zone) oder keiner.
static bool prv_puls_frisch(void) { return s.puls > 0 && s.frisch; }

static void prv_fuelle(Feld *f, Feldart art) {
  memset(f, 0, sizeof(*f));
  const size_t platz = sizeof(f->zahl);
  switch (art) {
    case FeldKeins:
      break;
    case FeldDauer:
      f->name = S(STR_L_DAUER);
      prv_zeit(f->zahl, platz, s_bereit ? 0 : s.sekunden);
      break;
    case FeldPuls:
      f->name = S(STR_L_PULS);
      f->einheit = "bpm";
      if (s.puls == 0) snprintf(f->zahl, platz, "--");
      else snprintf(f->zahl, platz, "%u", (unsigned)s.puls);
      break;
    case FeldDistanz:
      f->name = S(STR_L_DISTANZ);
      f->einheit = "km";
      snprintf(f->zahl, platz, "%u.%02u", (unsigned)(s.meter / 1000), (unsigned)((s.meter % 1000) / 10));
      break;
    case FeldTempo:
      // Minuten je Kilometer. Unter fuenfzig Metern ist es Rauschen.
      f->name = S(STR_L_TEMPO);
      f->einheit = "/km";
      if (s.meter >= 50 && s.sekunden > 0) {
        const uint32_t je_km = (uint32_t)s.sekunden * 1000 / s.meter;
        if (je_km < 100 * 60) prv_zeit(f->zahl, platz, je_km);
        else snprintf(f->zahl, platz, "--:--");
      } else {
        snprintf(f->zahl, platz, "--:--");
      }
      break;
    case FeldSchritte:
      f->name = S(STR_L_SCHRITTE);
      snprintf(f->zahl, platz, "%u", (unsigned)s.schritte);
      break;
    case FeldKcal:
      f->name = S(STR_L_KCAL);
      snprintf(f->zahl, platz, "%u", (unsigned)s.kcal);
      break;
    case FeldSatz:
      // SIE IST NICHT IMMER DIESELBE ZAHL. Waehrend eines Satzes schaut man
      // auf die Wiederholungen, danach auf die Pause.
      if (s.ruht) {
        f->name = S(STR_L_PAUSE);
        prv_zeit(f->zahl, platz, s.ruhe_s);
      } else {
        f->name = S(STR_L_WDH);
        snprintf(f->zahl, platz, "%u", (unsigned)s.laufend);
      }
      break;
    case FeldSaetze:
      f->name = S(STR_L_SAETZE);
      snprintf(f->zahl, platz, "%u", (unsigned)s.saetze);
      break;
    case FeldGesamt:
      f->name = S(STR_L_GESAMT);
      snprintf(f->zahl, platz, "%u", (unsigned)s.reps);
      break;
    case FeldBahnen:
      // LIEBER STRICHE ALS EINE NULL, solange der Kompass nicht zaehlt: eine
      // Null sieht aus wie "du bist noch keine geschwommen".
      f->name = S(STR_L_BAHNEN);
      if (s.kompass) snprintf(f->zahl, platz, "%u", (unsigned)s.bahnen);
      else snprintf(f->zahl, platz, "--");
      break;
    case FeldMeter:
      f->name = S(STR_L_DISTANZ);
      f->einheit = "m";
      snprintf(f->zahl, platz, "%u", (unsigned)s.meter);
      break;
    case FeldHrv:
      f->name = "HRV";
      f->einheit = "ms";
      if (s.hrv_ms > 0) snprintf(f->zahl, platz, "%u", (unsigned)s.hrv_ms);
      else snprintf(f->zahl, platz, "--");
      break;
    case FeldUhr: {
      f->name = S(STR_L_UHR);
      // Ohne AM/PM: LECO hat keine Buchstaben, und die Stunde reicht.
      const time_t jetzt = time(NULL);
      const struct tm *t = localtime(&jetzt);
      int h = t->tm_hour;
      if (!clock_is_24h_style()) { h %= 12; if (h == 0) h = 12; }
      snprintf(f->zahl, platz, "%d:%02d", h, t->tm_min);
      break;
    }
  }
}

// Name und Einheit in einer Zeile: "DISTANZ km", wie "DIST KM" im Workout.
static void prv_beschriftung(char *aus, size_t platz, const Feld *f) {
  if (f->einheit) snprintf(aus, platz, "%s %s", f->name, f->einheit);
  else snprintf(aus, platz, "%s", f->name);
}

// Die Zahlenschriften, gross nach klein. Genommen wird die groesste, die in
// Hoehe und Breite passt - "1:02:37" braucht auf flint eine kleinere als
// "42".
static const char *const s_zahlschriften[] = {
  FONT_KEY_LECO_42_NUMBERS,
  FONT_KEY_LECO_38_BOLD_NUMBERS,
  FONT_KEY_LECO_36_BOLD_NUMBERS,
  FONT_KEY_LECO_32_BOLD_NUMBERS,
  FONT_KEY_LECO_28_LIGHT_NUMBERS,
  FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
  FONT_KEY_LECO_20_BOLD_NUMBERS,
};
static const int16_t s_zahlhoehen[] = { 42, 38, 36, 32, 28, 26, 20 };
#define KS_SCHRIFTEN ((int)(sizeof(s_zahlhoehen) / sizeof(s_zahlhoehen[0])))

static int16_t prv_breite(const char *text, GFont schrift) {
  return graphics_text_layout_get_content_size(text, schrift, GRect(0, 0, 400, 80),
                                               GTextOverflowModeTrailingEllipsis,
                                               GTextAlignmentLeft).w;
}

// Die groesste Schrift, die in Hoehe und Breite passt.
static int prv_schrift_fuer(const char *zahl, int16_t hoehe, int16_t breite) {
  for (int i = 0; i < KS_SCHRIFTEN; i++) {
    if (s_zahlhoehen[i] > hoehe) continue;
    if (prv_breite(zahl, fonts_get_system_font(s_zahlschriften[i])) <= breite) return i;
  }
  return KS_SCHRIFTEN - 1;
}

// Eine Zahl in LECO, mittig in `r` - LECO steht mit etwas Luft oben in
// seiner Zeile, deshalb ein Stueck hoeher gesetzt. Gibt die Breite zurueck.
static int16_t prv_zahl(GContext *ctx, const char *zahl, int schrift, GRect r, GColor farbe,
                        GTextAlignment wie) {
  const int16_t zh = s_zahlhoehen[schrift];
  graphics_context_set_text_color(ctx, farbe);
  prv_text(ctx, zahl, s_zahlschriften[schrift],
           GRect(r.origin.x, r.origin.y + (r.size.h - zh) / 2 - zh / 6, r.size.w, zh + 8), wie);
  return prv_breite(zahl, fonts_get_system_font(s_zahlschriften[schrift]));
}

// Das Herz als Umriss, `f`-fach vergroessert (13 Punkte breit bei 1).
static void prv_herz(GContext *ctx, GPoint links_oben, int f, GColor farbe, bool voll) {
  static const GPoint roh[] = {
    {6, 11}, {0, 5}, {0, 2}, {2, 0}, {4, 0}, {6, 2}, {8, 0}, {10, 0}, {12, 2}, {12, 5},
  };
  GPoint p[10];
  for (int i = 0; i < 10; i++) p[i] = GPoint(roh[i].x * f, roh[i].y * f);
  GPathInfo info = { 10, p };
  GPath *pfad = gpath_create(&info);
  if (!pfad) return;
  gpath_move_to(pfad, links_oben);
  if (voll) {
    graphics_context_set_fill_color(ctx, farbe);
    gpath_draw_filled(ctx, pfad);
  }
  graphics_context_set_stroke_color(ctx, farbe);
  graphics_context_set_stroke_width(ctx, f >= 2 ? 3 : 2);
  gpath_draw_outline(ctx, pfad);
  graphics_context_set_stroke_width(ctx, 1);
  gpath_destroy(pfad);
}

// Die Farbe der Pulszahl: die der Zone, auf Schwarzweiss und ohne frischen
// Wert schwarz beziehungsweise grau.
static GColor prv_pulsfarbe(void) {
  if (s.puls == 0 || !s.frisch) return KS_FARBE_NEBEN;
#if defined(PBL_COLOR)
  // Die hellen Zonen sind auf Weiss nicht zu lesen - eine Stufe dunkler.
  switch (s.zone) {
    case 1: return GColorCobaltBlue;
    case 2: return GColorArmyGreen;
    case 3: return GColorWindsorTan;
    case 4: return GColorOrange;
    case 5: return GColorRed;
    default: return KS_FARBE_TEXT;
  }
#else
  return KS_FARBE_TEXT;
#endif
}

// DIE FUENF SAEULEN DER ZONEN, wie im Workout: jede in ihrer Farbe, die
// aktuelle hoch, die anderen halb. Auf Schwarzweiss die aktuelle gefuellt,
// die anderen als Rahmen.
static void prv_saeulen(GContext *ctx, GRect r, int16_t luecke) {
  const int zone = prv_puls_frisch() ? s.zone : 0;
  const int16_t sb = (r.size.w - luecke * (KS_ZONEN - 1)) / KS_ZONEN;
  for (int z = 1; z <= KS_ZONEN; z++) {
    const int16_t h = z == zone ? r.size.h : r.size.h / 2;
    const GRect k = GRect(r.origin.x + (z - 1) * (sb + luecke), r.origin.y + r.size.h - h, sb, h);
#if defined(PBL_COLOR)
    graphics_context_set_fill_color(ctx, thema_zonenfarbe(z));
    graphics_fill_rect(ctx, k, 0, GCornerNone);
#else
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    if (z == zone) graphics_fill_rect(ctx, k, 0, GCornerNone);
    else graphics_draw_rect(ctx, k);
#endif
  }
}

// --- Die Teile eines Schirms ---

#define KS_BALKEN_H PBL_IF_ROUND_ELSE(44, (KS_BREIT ? 30 : 24))

// DER BALKEN OBEN: die Farbe der Art, ihr Zeichen und ihr Name. Auf
// Schwarzweiss weiss mit einem Strich darunter.
static void prv_balken(GContext *ctx, int16_t w, const char *text) {
  const GColor grund = thema_sportfarbe(s_art);
  const GColor schrift = gcolor_legible_over(grund);
  const int16_t h = KS_BALKEN_H;
  graphics_context_set_fill_color(ctx, grund);
  graphics_fill_rect(ctx, GRect(0, 0, w, h), 0, GCornerNone);
#if !defined(PBL_COLOR)
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, h - 1), GPoint(w - 1, h - 1));
#endif
  const char *font = KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD;
  const int16_t g = KS_BREIT ? 20 : 16;
  const int16_t tb = prv_breite(text, fonts_get_system_font(font));
  const int16_t gesamt = g + 5 + tb;
  const int16_t x = (w - gesamt) / 2 > 2 ? (w - gesamt) / 2 : 2;
  // Auf der runden Uhr sitzt die Zeile unten im Balken: oben schneidet der
  // Kreis sie ab.
  const int16_t mitte = PBL_IF_ROUND_ELSE(h - 13, h / 2);
  symbol_sport(ctx, s_art, GPoint(x + g / 2, mitte), g, schrift);
  graphics_context_set_text_color(ctx, schrift);
  prv_text(ctx, text, font, GRect(x + g + 5, mitte - (KS_BREIT ? 16 : 12), w - x - g - 7, 30),
           GTextAlignmentLeft);
}

// Eine kleine Zahl mit ihrem Namen darueber - die beiden unten.
static void prv_klein(GContext *ctx, GRect r, Feldart art, bool grau) {
  if (art == FeldKeins) return;
  Feld f;
  prv_fuelle(&f, art);
  char name[32];
  prv_beschriftung(name, sizeof(name), &f);
  graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
  prv_text(ctx, name, FONT_KEY_GOTHIC_14, GRect(r.origin.x, r.origin.y - 3, r.size.w, 18),
           GTextAlignmentCenter);
  const int16_t zh = r.size.h - 14;
  const int wahl = prv_schrift_fuer(f.zahl, zh, r.size.w - 4);
  prv_zahl(ctx, f.zahl, wahl, GRect(r.origin.x, r.origin.y + 14, r.size.w, zh),
           grau ? KS_FARBE_NEBEN : KS_FARBE_TEXT, GTextAlignmentCenter);
}

// Die Pulszeile: Herz, Puls in der Farbe der Zone, fuenf Saeulen.
static void prv_pulszeile(GContext *ctx, GRect r) {
  char zahl[8];
  if (s.puls == 0) snprintf(zahl, sizeof(zahl), "--");
  else snprintf(zahl, sizeof(zahl), "%u", (unsigned)s.puls);
  const int16_t herz_b = KS_BREIT ? 26 : 20;
  const int16_t saeulen_b = KS_BREIT ? 56 : 42;
  const int wahl = prv_schrift_fuer(zahl, r.size.h, r.size.w - herz_b - saeulen_b - 12);
  const int16_t zb = prv_breite(zahl, fonts_get_system_font(s_zahlschriften[wahl]));
  const int16_t gesamt = herz_b + 4 + zb + 8 + saeulen_b;
  int16_t x = r.origin.x + (r.size.w - gesamt) / 2;
  const int16_t mitte = r.origin.y + r.size.h / 2;
  prv_herz(ctx, GPoint(x, mitte - (KS_BREIT ? 11 : 9)), KS_BREIT ? 2 : 1, prv_pulsfarbe(), false);
  x += herz_b + 4;
  prv_zahl(ctx, zahl, wahl, GRect(x, r.origin.y, zb + 4, r.size.h), prv_pulsfarbe(), GTextAlignmentLeft);
  x += zb + 8;
  const int16_t sh = r.size.h * 2 / 3;
  prv_saeulen(ctx, GRect(x, mitte - sh / 2, saeulen_b, sh), 2);
}

// Die Seiten als Striche, die aktuelle dunkel.
static void prv_striche(GContext *ctx, int16_t w, int16_t y, int seiten) {
  if (seiten < 2) return;
  const int16_t sb = KS_BREIT ? 12 : 9, luecke = 4, sh = KS_BREIT ? 3 : 2;
  const int16_t gesamt = seiten * sb + (seiten - 1) * luecke;
  int16_t x = (w - gesamt) / 2;
  for (int i = 0; i < seiten; i++) {
    graphics_context_set_fill_color(ctx, i == s_seite ? KS_FARBE_TEXT
                                    : PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
    const GRect k = GRect(x, y, sb, sh);
    graphics_fill_rect(ctx, k, 0, GCornerNone);
#if !defined(PBL_COLOR)
    if (i != s_seite) { graphics_context_set_stroke_color(ctx, GColorBlack); graphics_draw_rect(ctx, k); }
#endif
    x += sb + luecke;
  }
}

// Der kleine Halbkreis am rechten Rand, auf der Hoehe von Select - wie im
// Workout: da ist eine Taste, die etwas tut.
static void prv_tastenpunkt(GContext *ctx, GRect b) {
#if !defined(PBL_ROUND)
  graphics_context_set_fill_color(ctx, KS_FARBE_TEXT);
  graphics_fill_circle(ctx, GPoint(b.size.w + 3, b.size.h / 2), 9);
#endif
}

// Ein Hinweis ueber dem unteren Rand: kein Telefon, Akku, Kompass, Tasten.
static void prv_fuss(GContext *ctx, GRect b, int16_t w, const char *fuss) {
  if (!fuss) return;
  const int16_t h = 18;
  const int16_t y = b.size.h - PBL_IF_ROUND_ELSE(46, h);
  graphics_context_set_fill_color(ctx, KS_FARBE_TEXT);
  graphics_fill_rect(ctx, GRect(0, y, w, h), 0, GCornerNone);
  graphics_context_set_text_color(ctx, KS_FARBE_GRUND);
  prv_text(ctx, fuss, FONT_KEY_GOTHIC_14_BOLD, GRect(PBL_IF_ROUND_ELSE(KS_RAND, 4), y, w - PBL_IF_ROUND_ELSE(KS_RAND, 8), h),
           PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
}

// --- Die Schirme ---

// VOR DEM START: ein Kreis in der Farbe der Art, darin ihr Zeichen - oder,
// nach Select, der Countdown 3-2-1. Darunter der Name.
static uint8_t s_countdown;   //< 3..1 waehrend des Countdowns, sonst 0

static void prv_zeichne_bereit(GContext *ctx, GRect b, int16_t w) {
  const GColor grund = thema_sportfarbe(s_art);
  const GColor schrift = gcolor_legible_over(grund);

  char uhr[10];
  clock_copy_time_string(uhr, sizeof(uhr));
  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  const int16_t oben = PBL_IF_ROUND_ELSE(14, 0);
  prv_text(ctx, uhr, FONT_KEY_GOTHIC_14_BOLD, GRect(0, oben - 1, w, 16), GTextAlignmentCenter);
  graphics_context_set_stroke_color(ctx, KS_FARBE_TEXT);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(PBL_IF_ROUND_ELSE(70, 0), oben + 16), GPoint(w - PBL_IF_ROUND_ELSE(70, 1), oben + 16));
  graphics_context_set_stroke_width(ctx, 1);

  const int16_t kleiner = w < b.size.h ? w : b.size.h;
  const int16_t r = kleiner * 3 / 10;
  const GPoint mitte = GPoint(w / 2, oben + 20 + (b.size.h - oben - 20) * 2 / 5);
  graphics_context_set_fill_color(ctx, grund);
  graphics_fill_circle(ctx, mitte, r);
#if !defined(PBL_COLOR)
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_circle(ctx, mitte, r);
  graphics_context_set_stroke_width(ctx, 1);
#endif
  if (s_countdown > 0) {
    char zahl[4];
    snprintf(zahl, sizeof(zahl), "%u", (unsigned)s_countdown);
    prv_zahl(ctx, zahl, 0, GRect(mitte.x - r, mitte.y - r, 2 * r, 2 * r), schrift, GTextAlignmentCenter);
  } else {
    symbol_sport(ctx, s_art, mitte, r * 6 / 5, schrift);
  }

  const int16_t ty = mitte.y + r + 2;
  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  prv_text(ctx, art_name(s_art), KS_BREIT ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD,
           GRect(4, ty, w - 8, 34), GTextAlignmentCenter);
  if (s_countdown == 0) {
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, S(STR_SELECT_STARTET), FONT_KEY_GOTHIC_14,
             GRect(4, ty + (KS_BREIT ? 30 : 26), w - 8, 18), GTextAlignmentCenter);
  }

  // OHNE TELEFON KEINE STRECKE: das Telefon zeichnet sie auf, die Uhr hat
  // kein GPS. Wer das vor dem Start liest, kann das Telefon holen - danach
  // ist es zu spaet.
  if (art_info(s_art)->distanz && !connection_service_peek_pebble_app_connection()) {
    prv_fuss(ctx, b, w, S(STR_FUSS_KEIN_TEL));
  }
  prv_tastenpunkt(ctx, b);
}

// NACH DEM SPEICHERN: die Dauer und, wenn es einen Puls gab, die Zeit je
// Zone als Balken - wie "Time in Zones" in der Health-App. Darunter, ob das
// Telefon die Aufzeichnung schon hat.
static bool s_zonen_da;
static uint32_t s_zonen[KS_ZONEN + 1];

static void prv_zeichne_gespeichert(GContext *ctx, GRect b, int16_t w) {
  const int16_t rand = KS_RAND;
  const int16_t breite = w - 2 * rand;
  int16_t y = KS_BALKEN_H + 2;
  char text[24];
  prv_balken(ctx, w, art_name(s_art));

  prv_zeit(text, sizeof(text), s.sekunden);
  const int16_t zh = KS_BREIT ? 38 : 30;
  prv_zahl(ctx, text, prv_schrift_fuer(text, zh, breite), GRect(rand, y, breite, zh),
           KS_FARBE_TEXT, GTextAlignmentCenter);
  y += zh;
  graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
  prv_text(ctx, S(STR_L_DAUER), FONT_KEY_GOTHIC_14, GRect(rand, y - 3, breite, 18), GTextAlignmentCenter);
  y += 16;

  uint32_t summe = 0, groesste = 0;
  for (int z = 1; z <= KS_ZONEN; z++) {
    summe += s_zonen[z];
    if (s_zonen[z] > groesste) groesste = s_zonen[z];
  }
  if (s_zonen_da && summe > 0) {
    graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
    prv_text(ctx, S(STR_ZEIT_IN_ZONEN), FONT_KEY_GOTHIC_14_BOLD, GRect(rand, y - 2, breite, 18), GTextAlignmentCenter);
    y += 16;
    // Die Zeilen: Zone, Balken, Minuten. Die laengste Zone fuellt die Breite.
    const int16_t zeile_h = (b.size.h - PBL_IF_ROUND_ELSE(44, 18) - y) / KS_ZONEN;
    const int16_t zh2 = zeile_h > 18 ? 18 : zeile_h;
    for (int z = KS_ZONEN; z >= 1; z--) {
      graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
      snprintf(text, sizeof(text), "Z%d", z);
      prv_text(ctx, text, FONT_KEY_GOTHIC_14_BOLD, GRect(rand, y - 3, 20, 18), GTextAlignmentLeft);
      snprintf(text, sizeof(text), "%u'", (unsigned)((s_zonen[z] + 30) / 60));
      prv_text(ctx, text, FONT_KEY_GOTHIC_14, GRect(rand + breite - 30, y - 3, 30, 18), GTextAlignmentRight);
      const int16_t bx = rand + 20, bb = breite - 20 - 32;
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
      graphics_context_set_stroke_color(ctx, KS_FARBE_TEXT);
      const GRect voll = GRect(bx, y + 3, bb, zh2 - 8 > 4 ? zh2 - 8 : 4);
      graphics_fill_rect(ctx, voll, 0, GCornerNone);
#ifndef PBL_COLOR
      graphics_draw_rect(ctx, voll);   // weiss auf weiss braucht einen Rand
#endif
      const int16_t lang = groesste ? (int16_t)(bb * s_zonen[z] / groesste) : 0;
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(thema_zonenfarbe(z), GColorBlack));
      graphics_fill_rect(ctx, GRect(bx, voll.origin.y, lang, voll.size.h), 0, GCornerNone);
      y += zh2;
    }
  } else {
    graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
    prv_text(ctx, S(STR_GESPEICHERT), KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD,
             GRect(rand, y + 4, breite, 30), GTextAlignmentCenter);
  }

  const bool wartet = telefon_wartet();
  const bool aufgegeben = wartet && time(NULL) - s_gespeichert_seit >= BESTAETIGUNG_S;
  // Aufgegeben ist zweizeilig - der kurze Satz im Fuss reicht nicht, und
  // dann steht er ueber den Zonen, was nach dem Aufgeben niemand mehr liest.
  if (aufgegeben) {
    graphics_context_set_fill_color(ctx, KS_FARBE_TEXT);
    const int16_t h = 36;
    const int16_t fy = b.size.h - PBL_IF_ROUND_ELSE(56, h);
    graphics_fill_rect(ctx, GRect(0, fy, w, h), 0, GCornerNone);
    graphics_context_set_text_color(ctx, KS_FARBE_GRUND);
    prv_text(ctx, S(STR_TEL_AUFGEGEBEN), FONT_KEY_GOTHIC_14_BOLD,
             GRect(PBL_IF_ROUND_ELSE(KS_RAND, 4), fy, w - PBL_IF_ROUND_ELSE(KS_RAND, 8), h),
             PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  } else {
    prv_fuss(ctx, b, w, wartet ? S(STR_TEL_WARTET) : S(STR_TEL_ANGEKOMMEN));
  }
}

// DIE PULSSEITE: Herz und Puls gross, darunter die Zonen als breite Saeulen
// und der Name der Zone.
static void prv_zeichne_pulsseite(GContext *ctx, GRect bereich, int16_t rand) {
  const int16_t x = bereich.origin.x + rand, breite = bereich.size.w - 2 * rand;
  int16_t y = bereich.origin.y;
  char zahl[8];
  if (s.puls == 0) snprintf(zahl, sizeof(zahl), "--");
  else snprintf(zahl, sizeof(zahl), "%u", (unsigned)s.puls);

  const int f = KS_BREIT ? 3 : 2;
  const int16_t herz_b = 13 * f;
  const int16_t zh = KS_BREIT ? 42 : 36;
  const int wahl = prv_schrift_fuer(zahl, zh, breite - herz_b - 8);
  const int16_t zb = prv_breite(zahl, fonts_get_system_font(s_zahlschriften[wahl]));
  int16_t hx = x + (breite - herz_b - 8 - zb) / 2;
  prv_herz(ctx, GPoint(hx, y + (zh - 12 * f) / 2), f, KS_FARBE_TEXT, false);
  prv_zahl(ctx, zahl, wahl, GRect(hx + herz_b + 8, y, zb + 4, zh), prv_pulsfarbe(), GTextAlignmentLeft);
  y += zh;
  graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
  prv_text(ctx, S(STR_L_PULS), FONT_KEY_GOTHIC_14, GRect(x, y - 3, breite, 18), GTextAlignmentCenter);
  y += 18;

  const int16_t sh = KS_BREIT ? 22 : 16;
  prv_saeulen(ctx, GRect(x, y, breite, sh), 3);
  y += sh + 1;
  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  prv_text(ctx, prv_puls_frisch() ? S(prv_zonenname(s.zone)) : "--",
           FONT_KEY_GOTHIC_14_BOLD, GRect(x, y - 2, breite, 18), GTextAlignmentCenter);
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  // DIE UNVERDECKTE FLAECHE, nicht die ganze: faehrt die Timeline-
  // Schnellansicht von unten herein, schrumpft der Schirm - und alles
  // rueckt mit, statt darunter zu verschwinden.
  const GRect bounds = layer_get_unobstructed_bounds(layer);
  const int16_t voll = bounds.size.w;

  if (s_gespeichert) { prv_zeichne_gespeichert(ctx, bounds, voll); return; }
  if (s_bereit) { prv_zeichne_bereit(ctx, bounds, voll); return; }

  if (!s.da) {
    prv_balken(ctx, voll, art_name(s_art));
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, S(STR_HOLE_STAND), KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD,
             GRect(4, bounds.size.h / 2 - 14, voll - 8, 30), GTextAlignmentCenter);
    return;
  }

  // IN DER PAUSE DIE LEISTE: dort tun die Tasten etwas anderes als sonst.
  const bool pause = s.zustand == LaufPause || s_speichert || s_verwerfen_bis > time(NULL);
  const int16_t w = pause ? voll - KS_LEISTE_B : voll;
  const int16_t rand = PBL_IF_ROUND_ELSE(30, 4);

  Seite seiten[KS_SEITEN_MAX];
  const int anzahl = prv_seiten(seiten);
  if (s_seite >= anzahl) s_seite = 0;
  // In der Pause immer die erste Seite: die Zeit, die stehengeblieben ist.
  const Seite *seite = &seiten[pause ? 0 : s_seite];

  // --- Was die Tasten gerade tun ---
  Symbol so = SymbolKeins, sm = SymbolKeins, su = SymbolKeins;
  const char *fuss = NULL;
  if (s_speichert) {
    fuss = S(STR_FUSS_SPEICHERE);
  } else if (s_verwerfen_bis > time(NULL)) {
    sm = SymbolStart;
    su = SymbolLoeschenFrage;
    fuss = S(STR_FUSS_NOCHMAL);
  } else if (pause) {
    so = SymbolSpeichern;
    sm = SymbolStart;
    su = SymbolLoeschen;
    // KEIN Hinweis: die Leiste zeigt Speichern und Loeschen schon als
    // Zeichen, und neben ihr passte der Satz in keiner Sprache ganz hin.
  } else {
    if (s.sparsam) fuss = S(STR_FUSS_AKKU);
    else if (art_info(s_art)->bahnen && !s.kompass) fuss = S(STR_FUSS_KOMPASS);
  }
  prv_balken(ctx, w, art_name(s_art));

  const int16_t oben = KS_BALKEN_H + (KS_BREIT ? 4 : 2);
  const int16_t strich_y = bounds.size.h - PBL_IF_ROUND_ELSE(24, KS_BREIT ? 8 : 6);
  // Steht unten ein Hinweis, rueckt alles ueber ihn - sonst deckte er die
  // zwei kleinen Zahlen zu.
  const int16_t unten = fuss ? bounds.size.h - PBL_IF_ROUND_ELSE(46, 18) - 2
                             : strich_y - (KS_BREIT ? 4 : 2);

  // Die Hoehen: die grosse Zahl samt Namen, die Pulszeile, unten die zwei
  // kleinen. Was uebrig ist, verteilt sich als Luft dazwischen.
  const int16_t gross_h = KS_BREIT ? 44 : 36;
  const int16_t puls_h = pause ? 0 : (KS_BREIT ? 32 : 26);
  const int16_t klein_h = KS_BREIT ? 42 : 34;
  const int16_t kopf_h = pause ? 16 : 0;   // "PAUSE" ueber der Zahl
  const int16_t inhalt = kopf_h + gross_h + 16 + puls_h + klein_h;
  const int luecken = pause ? 2 : 3;
  int16_t luft = (unten - oben - inhalt) / luecken;
  if (luft < 0) luft = 0;
  int16_t y = oben;

  if (seite->herz) {
    prv_zeichne_pulsseite(ctx, GRect(0, y + luft / 2, w, unten - y), rand);
  } else {
    if (pause) {
      graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
      prv_text(ctx, S(STR_PAUSIERT), FONT_KEY_GOTHIC_14_BOLD, GRect(0, y - 2, w, 18), GTextAlignmentCenter);
      y += kopf_h;
    }
    Feld f;
    prv_fuelle(&f, seite->gross);
    const int wahl = prv_schrift_fuer(f.zahl, gross_h, w - 2 * rand);
    prv_zahl(ctx, f.zahl, wahl, GRect(rand, y, w - 2 * rand, gross_h),
             pause ? KS_FARBE_NEBEN : KS_FARBE_TEXT, GTextAlignmentCenter);
    y += gross_h;
    char name[32];
    prv_beschriftung(name, sizeof(name), &f);
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, name, FONT_KEY_GOTHIC_14, GRect(rand, y - 3, w - 2 * rand, 18), GTextAlignmentCenter);
    y += 16 + luft;

    if (!pause) {
      prv_pulszeile(ctx, GRect(rand, y, w - 2 * rand, puls_h));
      y += puls_h + luft;
    }
  }

  // Die zwei kleinen, nebeneinander. Rund: enger zusammen, der Kreis
  // schneidet unten die Ecken ab.
  const int16_t ky = unten - klein_h;
  const int16_t kr = PBL_IF_ROUND_ELSE(44, rand);
  const int16_t halb = (w - 2 * kr) / 2;
  prv_klein(ctx, GRect(kr, ky, halb, klein_h), seite->links, false);
  prv_klein(ctx, GRect(kr + halb, ky, halb, klein_h), seite->rechts, false);

  if (!pause && !fuss) prv_striche(ctx, w, strich_y, anzahl);

  prv_fuss(ctx, bounds, w, fuss);

  if (pause) {
    thema_leiste(ctx, bounds, so, sm, su);
  } else {
    prv_tastenpunkt(ctx, bounds);
  }
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
  if (!s_fenster) return;
  if (s_gespeichert && telefon_wartet()) {
    if (s_flaeche) layer_mark_dirty(s_flaeche);
    const time_t seit = time(NULL) - s_gespeichert_seit;
    // Noch warten - und nach dem Aufgeben den Hinweis zwei Sekunden lang
    // stehen lassen, damit man ihn liest.
    if (seit < BESTAETIGUNG_S + 2) {
      s_zu = app_timer_register(500, prv_zu, NULL);
      return;
    }
  } else if (s_gespeichert && !s_bestaetigt) {
    // Bestaetigt: "angekommen" zeigen - und mit den Zonen so lange, dass man
    // sie lesen kann. Zurueck oder Select schliessen frueher.
    s_bestaetigt = true;
    if (s_flaeche) layer_mark_dirty(s_flaeche);
    s_zu = app_timer_register(s_zonen_da ? 15000 : 1200, prv_zu, NULL);
    return;
  }
  window_stack_remove(s_fenster, true);
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
      // nichts mehr. Dann gehoert der Schirm zu. Der Worker bleibt - er
      // misst auch die Nacht (nacht.h).
      // Nicht in den ersten Sekunden nach dem Start: da kann der Worker
      // die Bestellung noch nicht gelesen haben.
      if (s.zustand == LaufAus && !s_bereit && !s_speichert && !s_gespeichert &&
          time(NULL) - s_gestartet > 3) {
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
      if (s_art == ArtYoga) {
        s.hrv_ms = d->data0;
        s.hrv_n = d->data1;
      } else {
        s.saetze = d->data0;
        s.reps = d->data1;
        s.laufend = d->data2 & 0x7FFF;
        s.ruht = (d->data2 & 0x8000) != 0;
      }
      break;
    case BotStand4:
      s.ruhe_s = d->data0;
      s.bahnen = d->data1;
      s.kompass = (d->data2 & 0x01) != 0;
      s.sparsam = (d->data2 & 0x02) != 0;
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
      s_gespeichert_seit = time(NULL);
      // Die Zeit je Zone JETZT, solange die Kurve im Persist liegt: ist sie
      // beim Telefon angekommen, wird sie geloescht. Eine alte Kurve eines
      // frueheren Trainings zaehlt nicht.
      s_zonen_da = kurve_beginn() == s.beginn && kurve_zeiten(s_zonen);
      telefon_nachsenden();
      // NICHT NACH ZWEI SEKUNDEN ZUGEHEN. Der erste Entwurf tat das - und
      // war die Verbindung in diesen zwei Sekunden besetzt, ging die App zu,
      // bevor der zweite Anlauf kam. Die Zusammenfassung lag dann im Persist
      // und kam erst beim naechsten Oeffnen der App an, Stunden spaeter,
      // und auf dem Telefon fehlte die Fahrt. Jetzt bleibt der Schirm, bis
      // das Telefon bestaetigt hat - oder BESTAETIGUNG_S um sind.
      if (!s_zu) s_zu = app_timer_register(500, prv_zu, NULL);
      break;
    case BotVerworfen:
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

static AppTimer *s_countdown_timer;

static void prv_countdown_ende(void) {
  if (s_countdown_timer) { app_timer_cancel(s_countdown_timer); s_countdown_timer = NULL; }
  s_countdown = 0;
  prv_start();
  if (s_flaeche) layer_mark_dirty(s_flaeche);
}

static void prv_countdown(void *data) {
  s_countdown_timer = NULL;
  if (!s_bereit || s_countdown == 0) return;
  if (--s_countdown == 0) {
    // Los: ein langes Brummen, dann laeuft die Zeit.
    vibes_long_pulse();
    prv_countdown_ende();
    return;
  }
  vibes_short_pulse();
  s_countdown_timer = app_timer_register(1000, prv_countdown, NULL);
  if (s_flaeche) layer_mark_dirty(s_flaeche);
}

static void prv_select(ClickRecognizerRef anlass, void *context) {
  // Nach dem Speichern schliesst Select - hat das Telefon die Aufzeichnung
  // noch nicht, bleibt sie im Persist und geht beim naechsten Oeffnen.
  if (s_gespeichert) { window_stack_remove(s_fenster, true); return; }
  if (s_speichert) return;
  if (s_bereit) {
    // ERST 3-2-1, WIE IM WORKOUT: Zeit, den Arm zu senken. Ein zweiter
    // Druck waehrend des Countdowns startet sofort.
    if (s_countdown > 0) { prv_countdown_ende(); return; }
    s_countdown = 3;
    vibes_short_pulse();
    s_countdown_timer = app_timer_register(1000, prv_countdown, NULL);
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
  if (s_bereit || s_speichert || s_gespeichert) return;
  if (s.zustand == LaufLaeuft) {
    // BEIM LAUFEN BLAETTERT UNTEN - zur naechsten Seite der Felder. Ein
    // Druck hier kostet nichts; verworfen wird nur aus der Pause.
    s_seite++;
    layer_mark_dirty(s_flaeche);
    return;
  }
  if (s.zustand != LaufPause) return;
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

static void prv_verdeckt(AnimationProgress fortschritt, void *context) {
  if (s_flaeche) layer_mark_dirty(s_flaeche);
}

static void prv_laden(Window *fenster) {
  Layer *wurzel = window_get_root_layer(fenster);
  const GRect bounds = layer_get_bounds(wurzel);

  s_flaeche = layer_create(bounds);
  layer_set_update_proc(s_flaeche, prv_zeichne);
  layer_add_child(wurzel, s_flaeche);
  // Neu zeichnen, waehrend die Schnellansicht hereinfaehrt.
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .change = prv_verdeckt,
  }, NULL);
}

static void prv_entladen(Window *fenster) {
  unobstructed_area_service_unsubscribe();
  if (s_abo) { app_timer_cancel(s_abo); s_abo = NULL; }
  if (s_zu) { app_timer_cancel(s_zu); s_zu = NULL; }
  if (s_countdown_timer) { app_timer_cancel(s_countdown_timer); s_countdown_timer = NULL; }
  s_countdown = 0;
  layer_destroy(s_flaeche);
  s_flaeche = NULL;
  window_destroy(s_fenster);
  s_fenster = NULL;
}

static void prv_oeffnen(void) {
  s_speichert = false;
  s_gespeichert = false;
  s_bestaetigt = false;
  s_zonen_da = false;
  s_seite = 0;
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
