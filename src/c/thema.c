#include "thema.h"

GColor thema_zonenfarbe(int zone) {
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
  // SCHWARZWEISS: eine Farbe, die es nicht gibt, ist keine Auskunft. Dort
  // traegt die Ziffer neben dem Puls die Zone, und der Balken darunter
  // zeigt sie noch einmal als Laenge.
  return zone > 0 ? GColorBlack : GColorLightGray;
#endif
}

// Das Herz, 22 Punkte breit, um seinen Mittelpunkt. Zwei Boegen oben, eine
// Spitze unten - grob genug, dass es auch auf 144 Punkten noch eines ist.
static GPoint s_herz_punkte[] = {
  {0, 10}, {-11, -1}, {-11, -6}, {-8, -9}, {-4, -9}, {0, -5},
  {4, -9}, {8, -9}, {11, -6}, {11, -1},
};
static const GPathInfo s_herz_info = {
  .num_points = sizeof(s_herz_punkte) / sizeof(s_herz_punkte[0]),
  .points = s_herz_punkte,
};
static GPath *s_herz;

// Das Dreieck fuer Start, 14 Punkte hoch, um seinen Mittelpunkt.
static GPoint s_dreieck_punkte[] = { {-5, -7}, {7, 0}, {-5, 7} };
static const GPathInfo s_dreieck_info = { 3, s_dreieck_punkte };
static GPath *s_dreieck;

// Die Zeichen sind alle in Weiss (AUF_LEISTE) auf der Leiste; wo ein Zeichen
// eine Aussparung braucht, ist die in der Leistenfarbe gemalt.
static void prv_symbol(GContext *ctx, Symbol was, GPoint m) {
  const GColor weiss = KS_FARBE_AUF_LEISTE;
  const GColor grund = KS_FARBE_LEISTE;
  graphics_context_set_fill_color(ctx, weiss);
  graphics_context_set_stroke_color(ctx, weiss);
  graphics_context_set_stroke_width(ctx, 1);
  switch (was) {
    case SymbolStart:
      if (!s_dreieck) s_dreieck = gpath_create(&s_dreieck_info);
      gpath_move_to(s_dreieck, m);
      gpath_draw_filled(ctx, s_dreieck);
      break;
    case SymbolPause:
      graphics_fill_rect(ctx, GRect(m.x - 6, m.y - 7, 4, 14), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(m.x + 2, m.y - 7, 4, 14), 0, GCornerNone);
      break;
    case SymbolSpeichern:
      // Die Diskette: ein Quadrat mit abgeschnittener Ecke, oben der
      // Schieber mit seinem Fenster, unten das Etikett.
      graphics_fill_rect(ctx, GRect(m.x - 8, m.y - 8, 16, 16), 2, GCornerBottomLeft | GCornerBottomRight | GCornerTopLeft);
      graphics_context_set_fill_color(ctx, grund);
      graphics_fill_rect(ctx, GRect(m.x - 4, m.y - 8, 8, 5), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(m.x - 5, m.y + 2, 10, 6), 0, GCornerNone);
      graphics_context_set_fill_color(ctx, weiss);
      graphics_fill_rect(ctx, GRect(m.x + 1, m.y - 7, 2, 3), 0, GCornerNone);
      break;
    case SymbolLoeschen:
    case SymbolLoeschenFrage: {
      // Der Eimer: Griff, Deckel, Koerper mit zwei Rillen. Beim zweiten
      // Druck steht ein Fragezeichen daneben: jetzt ist es ernst.
      const int16_t dx = was == SymbolLoeschenFrage ? -4 : 0;
      graphics_fill_rect(ctx, GRect(m.x + dx - 2, m.y - 9, 4, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(m.x + dx - 7, m.y - 7, 14, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(m.x + dx - 5, m.y - 4, 10, 13), 1, GCornerBottomLeft | GCornerBottomRight);
      graphics_context_set_fill_color(ctx, grund);
      graphics_fill_rect(ctx, GRect(m.x + dx - 2, m.y - 2, 1, 8), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(m.x + dx + 1, m.y - 2, 1, 8), 0, GCornerNone);
      if (was == SymbolLoeschenFrage) {
        graphics_context_set_text_color(ctx, weiss);
        graphics_draw_text(ctx, "?", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                           GRect(m.x + 5, m.y - 12, 12, 22),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      }
      break;
    }
    case SymbolWahl:
      graphics_context_set_stroke_width(ctx, 3);
      graphics_draw_line(ctx, GPoint(m.x - 3, m.y - 6), GPoint(m.x + 3, m.y));
      graphics_draw_line(ctx, GPoint(m.x + 3, m.y), GPoint(m.x - 3, m.y + 6));
      graphics_context_set_stroke_width(ctx, 1);
      break;
    default:
      break;
  }
}

void thema_leiste(GContext *ctx, GRect b, bool herz_voll, GColor herz,
                  Symbol oben, Symbol mitte, Symbol unten) {
  const int16_t sx = b.size.w - KS_LEISTE_B;
  graphics_context_set_fill_color(ctx, KS_FARBE_LEISTE);
  graphics_fill_rect(ctx, GRect(sx, 0, KS_LEISTE_B, b.size.h), 0, GCornerNone);

  // Das Herz oben, wo Drinktervall sein Glas hat. Voll heisst: der Sensor
  // misst; die Fuellung ist die Zone. Hohl heisst: kein Puls - und das sieht
  // man aus dem Augenwinkel, ohne eine Zahl zu lesen.
  const int16_t cx = sx + KS_LEISTE_B / 2 - KS_LEISTE_DX;
  const int16_t cy = PBL_IF_ROUND_ELSE(58, 22);
  if (!s_herz) s_herz = gpath_create(&s_herz_info);
  gpath_move_to(s_herz, GPoint(cx, cy));
  if (herz_voll) {
    graphics_context_set_fill_color(ctx, herz);
    gpath_draw_filled(ctx, s_herz);
  }
  graphics_context_set_stroke_color(ctx, KS_FARBE_AUF_LEISTE);
  graphics_context_set_stroke_width(ctx, 2);
  gpath_draw_outline(ctx, s_herz);
  graphics_context_set_stroke_width(ctx, 1);

  // Die Zeichen auf Tastenhoehe: die Tasten sitzen bei einem Viertel, der
  // Haelfte und drei Vierteln der Hoehe. Wer hinschaut, sieht neben der
  // Taste, was sie tut - statt unten eine Zeile fuer alle drei.
  const Symbol zeichen[3] = { oben, mitte, unten };
  for (int i = 0; i < 3; i++) {
    if (zeichen[i] == SymbolKeins) continue;
    prv_symbol(ctx, zeichen[i], GPoint(cx, b.size.h * (i + 1) / 4));
  }
}
