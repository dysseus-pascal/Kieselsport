#pragma once

// Derselbe Kern fuer App und Worker.
//
// DER WORKER HAT EINEN EIGENEN KOPF. Ein Hintergrund-Worker darf nur einen
// Teil der Schnittstelle benutzen - keine Fenster, keine Grafik, keine
// AppMessage -, und dafuer gibt es pebble_worker.h statt pebble.h. Alles im
// Ordner kern/ wird fuer beide uebersetzt und nimmt hier den passenden.
#ifdef KS_WORKER
#include <pebble_worker.h>
#else
#include <pebble.h>
#endif
