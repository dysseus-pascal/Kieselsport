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

// --- Die Felder ---
//
// GEBAUT WIE DIE WORKOUT-APP DER PEBBLE: je Feld ein Wort in Versalien und
// darunter die Zahl gross in LECO, zwei oder drei Felder uebereinander, und
// mit Unten die naechste Seite. Frueher stand eine grosse Zahl oben und
// darunter eine Reihe kleiner Spalten; die kleinen las beim Laufen niemand.
//
// Das Pulsfeld faerbt sich in der Farbe der Zone und traegt ihren Namen -
// das sieht man aus dem Augenwinkel. Die rote Leiste rechts bleibt: sie
// macht Kieselsport zu einem Geschwister von Drinktervall und Flynformer.

typedef enum {
  FeldDauer = 0,
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
} Feldart;

#define KS_FELDER_MAX 6

// Was jede Art zeigt, das Wichtigste zuerst: die erste Seite ist die, auf
// die man im Vorbeischwingen schaut.
static int prv_felder(Feldart *aus) {
  int n = 0;
  switch (s_art) {
    case ArtLaufen:
    case ArtWandern:
      aus[n++] = FeldDauer; aus[n++] = FeldPuls; aus[n++] = FeldDistanz;
      aus[n++] = FeldTempo; aus[n++] = FeldSchritte; aus[n++] = FeldKcal;
      break;
    case ArtKraft:
      aus[n++] = FeldSatz; aus[n++] = FeldPuls; aus[n++] = FeldDauer;
      aus[n++] = FeldSaetze; aus[n++] = FeldGesamt; aus[n++] = FeldKcal;
      break;
    case ArtYoga:
      aus[n++] = FeldDauer; aus[n++] = FeldPuls; aus[n++] = FeldHrv; aus[n++] = FeldKcal;
      break;
    case ArtSchwimmen:
      aus[n++] = FeldDauer; aus[n++] = FeldPuls; aus[n++] = FeldBahnen;
      aus[n++] = FeldMeter; aus[n++] = FeldKcal;
      break;
    default:   // Strasse/Gravel, MTB: Zeit, Puls, Kalorien - wenig, aber wahr
      aus[n++] = FeldDauer; aus[n++] = FeldPuls; aus[n++] = FeldKcal;
      break;
  }
  return n;
}

static uint8_t s_seite;   //< welche Seite der Felder gerade steht

static StringId prv_zonenname(int zone) {
  static const StringId namen[KS_ZONEN] = { STR_ZONE_1, STR_ZONE_2, STR_ZONE_3, STR_ZONE_4, STR_ZONE_5 };
  return (zone >= 1 && zone <= KS_ZONEN) ? namen[zone - 1] : STR_L_PULS;
}

typedef struct {
  const char *name;
  char zahl[16];
  const char *einheit;   //< klein hinter der Zahl, oder NULL
  GColor grund;
  int zone;              //< Balken unter dem Namen; -1: keiner
  bool herz;             //< Herz hinter der Zahl
  bool voll;             //< ... gefuellt: frischer Wert
  bool grau;             //< alter Wert: grau statt schwarz
} Feld;

static void prv_fuelle(Feld *f, Feldart art) {
  memset(f, 0, sizeof(*f));
  f->grund = KS_FARBE_GRUND;
  f->zone = -1;
  const size_t platz = sizeof(f->zahl);
  switch (art) {
    case FeldDauer:
      f->name = S(STR_L_DAUER);
      prv_zeit(f->zahl, platz, s_bereit ? 0 : s.sekunden);
      break;
    case FeldPuls:
      f->herz = true;
      if (s.puls == 0) {
        f->name = S(STR_L_PULS);
        snprintf(f->zahl, platz, "--");
      } else if (!s.frisch) {
        // EIN ALTER WERT STEHT GRAU DA und traegt keine Zone. Der Sensor
        // behaelt den letzten guten Wert, wenn er am Lenker nichts
        // Brauchbares misst - eine halbe Stunde "75" in Schwarz saehe aus
        // wie eine Messung.
        f->name = S(STR_L_PULS);
        f->grau = true;
        snprintf(f->zahl, platz, "%u", (unsigned)s.puls);
      } else {
        f->name = S(prv_zonenname(s.zone));
        f->grund = thema_zonengrund(s.zone);
        f->zone = s.zone;
        f->voll = true;
        snprintf(f->zahl, platz, "%u", (unsigned)s.puls);
      }
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
      f->einheit = "kcal";
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
  }
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

static int16_t prv_breite(const char *text, GFont schrift) {
  return graphics_text_layout_get_content_size(text, schrift, GRect(0, 0, 400, 80),
                                               GTextOverflowModeTrailingEllipsis,
                                               GTextAlignmentLeft).w;
}

// Das kleine Herz hinter der Pulszahl, 13 Punkte breit.
static GPoint s_herzchen_punkte[] = {
  {6, 11}, {0, 5}, {0, 2}, {2, 0}, {4, 0}, {6, 2}, {8, 0}, {10, 0}, {12, 2}, {12, 5},
};
static const GPathInfo s_herzchen_info = { 10, s_herzchen_punkte };
static GPath *s_herzchen;

#define KS_NAME_H 20
#define KS_NAME_SCHRIFT FONT_KEY_GOTHIC_18_BOLD

static void prv_feld(GContext *ctx, GRect r, const Feld *f, int16_t rand) {
  graphics_context_set_fill_color(ctx, f->grund);
  graphics_fill_rect(ctx, r, 0, GCornerNone);

  const int16_t x = r.origin.x + rand;
  const int16_t breite = r.size.w - rand - 4;
  int16_t y = r.origin.y + 1;

  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  prv_text(ctx, f->name, KS_NAME_SCHRIFT, GRect(x, y - 2, breite, KS_NAME_H), GTextAlignmentLeft);
  y += KS_NAME_H - 2;

  // DER BALKEN DER ZONE: fuenf Kaestchen, die erreichten gefuellt. Auf
  // Schwarzweiss ist er die ganze Auskunft ueber die Zone.
  if (f->zone >= 0) {
    for (int z = 1; z <= KS_ZONEN; z++) {
      const GRect k = GRect(x + (z - 1) * 12, y, 10, 4);
      graphics_context_set_fill_color(ctx, KS_FARBE_TEXT);
      graphics_context_set_stroke_color(ctx, KS_FARBE_TEXT);
      if (z <= f->zone) graphics_fill_rect(ctx, k, 0, GCornerNone);
      else graphics_draw_rect(ctx, k);
    }
    y += 5;
  }

  // Die groesste Schrift, die passt.
  const GFont klein = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  const int16_t einheit_b = f->einheit ? prv_breite(f->einheit, klein) + 3 : 0;
  const int16_t herz_b = f->herz ? 17 : 0;
  const int16_t frei_h = r.origin.y + r.size.h - y;
  const int anzahl = (int)(sizeof(s_zahlhoehen) / sizeof(s_zahlhoehen[0]));
  int wahl = anzahl - 1;
  for (int i = 0; i < anzahl; i++) {
    if (s_zahlhoehen[i] + 2 > frei_h) continue;
    const GFont g = fonts_get_system_font(s_zahlschriften[i]);
    if (prv_breite(f->zahl, g) + einheit_b + herz_b <= breite) { wahl = i; break; }
  }
  const GFont zahl = fonts_get_system_font(s_zahlschriften[wahl]);
  const int16_t zh = s_zahlhoehen[wahl];
  // LECO steht mit etwas Luft oben in seiner Zeile: die Ziffern beginnen ein
  // Fuenftel der Hoehe tiefer. Ein Stueck hoeher gesetzt, sitzt die Zahl
  // unter dem Namen statt mitten im Feld.
  const int16_t zy = y - zh / 6;
  graphics_context_set_text_color(ctx, f->grau ? KS_FARBE_NEBEN : KS_FARBE_TEXT);
  prv_text(ctx, f->zahl, s_zahlschriften[wahl], GRect(x, zy, breite, zh + 8), GTextAlignmentLeft);
  const int16_t zb = prv_breite(f->zahl, zahl);

  if (f->einheit) {
    prv_text(ctx, f->einheit, FONT_KEY_GOTHIC_18_BOLD,
             GRect(x + zb + 3, zy + zh - 18, einheit_b + 4, 22), GTextAlignmentLeft);
  }
  if (f->herz) {
    if (!s_herzchen) s_herzchen = gpath_create(&s_herzchen_info);
    gpath_move_to(s_herzchen, GPoint(x + zb + 4, zy + zh / 5 + 2));
    if (f->voll) {
      graphics_context_set_fill_color(ctx, KS_FARBE_TEXT);
      gpath_draw_filled(ctx, s_herzchen);
    }
    graphics_context_set_stroke_color(ctx, f->grau ? KS_FARBE_NEBEN : KS_FARBE_TEXT);
    graphics_context_set_stroke_width(ctx, 2);
    gpath_draw_outline(ctx, s_herzchen);
    graphics_context_set_stroke_width(ctx, 1);
  }
}

// --- Die Schirme ---

#define KS_KOPF_H PBL_IF_ROUND_ELSE(36, 16)

// Die Kopfzeile: die Uhrzeit, wie in jeder Pebble-App - in der Pause statt
// ihrer die Art und das Wort "Pause", fett, damit man es nicht uebersieht.
static void prv_kopf(GContext *ctx, GRect b, int16_t w) {
  char zeile[40];
  const char *schrift = FONT_KEY_GOTHIC_14;
  if (!s_bereit && s.da && s.zustand == LaufPause) {
    snprintf(zeile, sizeof(zeile), S(STR_ZEILE_PAUSE), art_name(s_art));
    schrift = FONT_KEY_GOTHIC_14_BOLD;
  } else {
    clock_copy_time_string(zeile, sizeof(zeile));
  }
  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  prv_text(ctx, zeile, schrift, GRect(0, PBL_IF_ROUND_ELSE(12, -1), w, 16), GTextAlignmentCenter);
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

// VOR DEM START: die Art gross, als Bild auf ihrer Farbe - wie der Schirm
// "Run" in der Workout-App. Select startet.
static void prv_zeichne_bereit(GContext *ctx, GRect b, int16_t w) {
  const GColor grund = thema_sportfarbe(s_art);
  const GColor schrift = gcolor_legible_over(grund);
  graphics_context_set_fill_color(ctx, grund);
  graphics_fill_rect(ctx, GRect(0, 0, w, b.size.h), 0, GCornerNone);

  char uhr[10];
  clock_copy_time_string(uhr, sizeof(uhr));
  graphics_context_set_text_color(ctx, schrift);
  prv_text(ctx, uhr, FONT_KEY_GOTHIC_14, GRect(0, PBL_IF_ROUND_ELSE(12, -1), w, 16), GTextAlignmentCenter);

  const int16_t g = (w < b.size.h ? w : b.size.h) * 9 / 20;
  const int16_t mitte_x = w / 2 + PBL_IF_ROUND_ELSE(KS_LEISTE_DX, 0);
  const int16_t mitte_y = b.size.h * 2 / 5;
  symbol_sport(ctx, s_art, GPoint(mitte_x, mitte_y), g, schrift);

  const int16_t ty = mitte_y + g / 2 + 4;
  graphics_context_set_text_color(ctx, schrift);
  prv_text(ctx, art_name(s_art), KS_BREIT ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD,
           GRect(4, ty, w - 8, 34), GTextAlignmentCenter);
  prv_text(ctx, S(STR_SELECT_STARTET), FONT_KEY_GOTHIC_14,
           GRect(4, ty + (KS_BREIT ? 32 : 28), w - 8, 18), GTextAlignmentCenter);

  // OHNE TELEFON KEINE STRECKE: das Telefon zeichnet sie auf, die Uhr hat
  // kein GPS. Wer das vor dem Start liest, kann das Telefon holen - danach
  // ist es zu spaet.
  if (art_info(s_art)->distanz && !connection_service_peek_pebble_app_connection()) {
    prv_fuss(ctx, b, w, S(STR_FUSS_KEIN_TEL));
  }
  thema_leiste(ctx, b, false, GColorWhite, SymbolKeins, SymbolStart, SymbolKeins);
}

// NACH DEM SPEICHERN: die Dauer und, wenn es einen Puls gab, die Zeit je
// Zone als Balken - wie "Time in Zones" in der Health-App. Darunter, ob das
// Telefon die Aufzeichnung schon hat.
static bool s_zonen_da;
static uint32_t s_zonen[KS_ZONEN + 1];

static void prv_zeichne_gespeichert(GContext *ctx, GRect b, int16_t w) {
  const int16_t rand = KS_RAND;
  const int16_t breite = w - rand - 4;
  int16_t y = KS_KOPF_H;
  char text[24];
  prv_kopf(ctx, b, w);

  graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
  prv_text(ctx, art_name(s_art), FONT_KEY_GOTHIC_18_BOLD, GRect(rand, y - 2, breite, 20), GTextAlignmentLeft);
  y += 16;
  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  prv_zeit(text, sizeof(text), s.sekunden);
  prv_text(ctx, text, KS_BREIT ? FONT_KEY_LECO_36_BOLD_NUMBERS : FONT_KEY_LECO_28_LIGHT_NUMBERS,
           GRect(rand, y - 4, breite, 44), GTextAlignmentLeft);
  y += KS_BREIT ? 38 : 30;

  uint32_t summe = 0, groesste = 0;
  for (int z = 1; z <= KS_ZONEN; z++) {
    summe += s_zonen[z];
    if (s_zonen[z] > groesste) groesste = s_zonen[z];
  }
  if (s_zonen_da && summe > 0) {
    // Der Kopf wie in der Health-App: ein farbiges Band mit weisser Schrift.
    graphics_context_set_fill_color(ctx, KS_FARBE_LEISTE);
    graphics_fill_rect(ctx, GRect(rand, y, breite, 18), 3, GCornersAll);
    graphics_context_set_text_color(ctx, KS_FARBE_AUF_LEISTE);
    prv_text(ctx, S(STR_ZEIT_IN_ZONEN), FONT_KEY_GOTHIC_14_BOLD, GRect(rand, y, breite, 18), GTextAlignmentCenter);
    y += 20;
    // Die Zeilen: Zone, Balken, Minuten. Die laengste Zone fuellt die Breite.
    const int16_t zeile_h = (b.size.h - PBL_IF_ROUND_ELSE(44, 18) - y) / KS_ZONEN;
    const int16_t zh = zeile_h > 18 ? 18 : zeile_h;
    for (int z = KS_ZONEN; z >= 1; z--) {
      graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
      snprintf(text, sizeof(text), "Z%d", z);
      prv_text(ctx, text, FONT_KEY_GOTHIC_14_BOLD, GRect(rand, y - 3, 20, 18), GTextAlignmentLeft);
      snprintf(text, sizeof(text), "%u'", (unsigned)((s_zonen[z] + 30) / 60));
      prv_text(ctx, text, FONT_KEY_GOTHIC_14, GRect(rand + breite - 30, y - 3, 30, 18), GTextAlignmentRight);
      const int16_t bx = rand + 20, bb = breite - 20 - 32;
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
      graphics_context_set_stroke_color(ctx, KS_FARBE_TEXT);
      const GRect voll = GRect(bx, y + 3, bb, zh - 8 > 4 ? zh - 8 : 4);
      graphics_fill_rect(ctx, voll, 0, GCornerNone);
#ifndef PBL_COLOR
      graphics_draw_rect(ctx, voll);   // weiss auf weiss braucht einen Rand
#endif
      const int16_t lang = groesste ? (int16_t)(bb * s_zonen[z] / groesste) : 0;
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(thema_zonenfarbe(z), GColorBlack));
      graphics_fill_rect(ctx, GRect(bx, voll.origin.y, lang, voll.size.h), 0, GCornerNone);
      y += zh;
    }
  } else {
    graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
    prv_text(ctx, S(STR_GESPEICHERT), KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD,
             GRect(rand, y, breite, 30), GTextAlignmentLeft);
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
  thema_leiste(ctx, b, false, GColorWhite, SymbolKeins, SymbolKeins, SymbolKeins);
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  // DIE UNVERDECKTE FLAECHE, nicht die ganze: faehrt die Timeline-
  // Schnellansicht von unten herein, schrumpft der Schirm - und die Felder
  // ruecken mit, statt darunter zu verschwinden.
  const GRect bounds = layer_get_unobstructed_bounds(layer);
  // Die Flaeche links von der Leiste.
  const int16_t w = bounds.size.w - KS_LEISTE_B;

  if (s_gespeichert) { prv_zeichne_gespeichert(ctx, bounds, w); return; }
  if (s_bereit) { prv_zeichne_bereit(ctx, bounds, w); return; }

  if (!s.da) {
    prv_kopf(ctx, bounds, w);
    graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
    prv_text(ctx, S(STR_HOLE_STAND), KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD,
             GRect(KS_RAND, bounds.size.h / 2 - 14, w - KS_RAND - 4, 30), GTextAlignmentLeft);
    thema_leiste(ctx, bounds, false, GColorWhite, SymbolKeins, SymbolKeins, SymbolKeins);
    return;
  }

  prv_kopf(ctx, bounds, w);

  // --- Die Felder dieser Seite ---
  //
  // DREI UEBEREINANDER, wo die Hoehe es traegt (emery), sonst zwei. Auf der
  // runden Uhr zwei: oben und unten nimmt der Kreis zu viel weg.
  Feldart alle[KS_FELDER_MAX];
  const int anzahl = prv_felder(alle);
  const int16_t oben = KS_KOPF_H;
  const int16_t unten = bounds.size.h - PBL_IF_ROUND_ELSE(30, 0);
  const int16_t hoehe = unten - oben;
  const int je_seite = PBL_IF_ROUND_ELSE(2, hoehe >= 200 ? 3 : (hoehe >= 110 ? 2 : 1));
  const int seiten = (anzahl + je_seite - 1) / je_seite;
  if (s_seite >= seiten) s_seite = 0;

  const int erstes = s_seite * je_seite;
  int hier = anzahl - erstes;
  if (hier > je_seite) hier = je_seite;
  const int16_t feld_h = hoehe / je_seite;
  for (int i = 0; i < hier; i++) {
    Feld f;
    prv_fuelle(&f, alle[erstes + i]);
    // Rund: mehr Rand, der Kreis nimmt oben und unten die linke Ecke.
    prv_feld(ctx, GRect(0, oben + i * feld_h, w, feld_h), &f, PBL_IF_ROUND_ELSE(48, KS_RAND));
    // Ein feiner Strich zwischen zwei Feldern mit demselben Grund.
    if (i > 0) {
      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
      graphics_draw_line(ctx, GPoint(0, oben + i * feld_h), GPoint(w - 1, oben + i * feld_h));
    }
  }

  // Die Seite als Punkte in der Kopfzeile rechts, wenn es mehrere gibt.
  if (seiten > 1) {
    for (int i = 0; i < seiten; i++) {
      const GPoint p = GPoint(w - PBL_IF_ROUND_ELSE(40, 6) - (seiten - 1 - i) * 6, PBL_IF_ROUND_ELSE(30, 8));
      graphics_context_set_fill_color(ctx, KS_FARBE_TEXT);
      if (i == s_seite) graphics_fill_circle(ctx, p, 2);
      else {
        graphics_context_set_stroke_color(ctx, KS_FARBE_NEBEN);
        graphics_draw_circle(ctx, p, 2);
      }
    }
  }

  // --- Was die Tasten gerade tun ---
  //
  // DIE LEISTE ZEIGT ES, auf der Hoehe der Taste, als Zeichen statt als
  // Wort. Beim Laufen blaettert Unten weiter (">>" wie im Workout), in der
  // Pause verwirft es - deshalb dort zweimal.
  Symbol so = SymbolKeins, sm = SymbolKeins, su = SymbolKeins;
  const char *fuss = NULL;
  if (s_speichert) {
    fuss = S(STR_FUSS_SPEICHERE);
  } else if (s_verwerfen_bis > time(NULL)) {
    sm = SymbolStart;
    su = SymbolLoeschenFrage;
    fuss = S(STR_FUSS_NOCHMAL);
  } else if (s.zustand == LaufPause) {
    so = SymbolSpeichern;
    sm = SymbolStart;
    su = SymbolLoeschen;
    fuss = S(STR_FUSS_PAUSE);
  } else {
    sm = SymbolPause;
    if (seiten > 1) su = SymbolWeiter;
    if (s.sparsam) fuss = S(STR_FUSS_AKKU);
    else if (art_info(s_art)->bahnen && !s.kompass) fuss = S(STR_FUSS_KOMPASS);
  }
  prv_fuss(ctx, bounds, w, fuss);

  const bool herz = s.puls > 0 && s.frisch;
  thema_leiste(ctx, bounds, herz, thema_zonenfarbe(s.zone), so, sm, su);
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
      app_worker_kill();
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
  // Nach dem Speichern schliesst Select - hat das Telefon die Aufzeichnung
  // noch nicht, bleibt sie im Persist und geht beim naechsten Oeffnen.
  if (s_gespeichert) { window_stack_remove(s_fenster, true); return; }
  if (s_speichert) return;
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
