#pragma once
#include <pebble.h>

// Der Schirm am Morgen: der Worker hat die App geholt, weil eine Nacht
// fertig ist (nacht.h). Er sagt, was geschieht, und geht von selbst wieder
// zu, sobald das Telefon die Nacht hat - oder nach einer Minute.
void morgen_window_zeige(void);
