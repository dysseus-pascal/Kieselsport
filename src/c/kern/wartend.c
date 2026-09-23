#include "wartend.h"
#include "schluessel.h"

#define HALB 200

void wartend_merken(const Trainingsstand *t, const char *liste) {
  persist_write_data(PERSIST_WARTET, t, sizeof(*t));
  char halb[HALB + 1];
  strncpy(halb, liste, HALB);
  halb[HALB] = 0;
  persist_write_string(PERSIST_WARTET_LISTE_A, halb);
  if (strlen(liste) > HALB) {
    persist_write_string(PERSIST_WARTET_LISTE_B, liste + HALB);
  } else {
    persist_delete(PERSIST_WARTET_LISTE_B);
  }
}

bool wartend_laden(Trainingsstand *t, char *liste, size_t platz) {
  if (!persist_exists(PERSIST_WARTET)) return false;
  memset(t, 0, sizeof(*t));
  persist_read_data(PERSIST_WARTET, t, sizeof(*t));
  liste[0] = 0;
  if (persist_exists(PERSIST_WARTET_LISTE_A)) {
    persist_read_string(PERSIST_WARTET_LISTE_A, liste, (uint16_t)(platz > HALB + 1 ? HALB + 1 : platz));
  }
  if (persist_exists(PERSIST_WARTET_LISTE_B)) {
    const size_t bisher = strlen(liste);
    if (platz > bisher + 1) {
      persist_read_string(PERSIST_WARTET_LISTE_B, liste + bisher, (uint16_t)(platz - bisher));
    }
  }
  return t->beginn > 0 && t->dauer_s > 0;
}

void wartend_vergessen(void) {
  persist_delete(PERSIST_WARTET);
  persist_delete(PERSIST_WARTET_LISTE_A);
  persist_delete(PERSIST_WARTET_LISTE_B);
}
