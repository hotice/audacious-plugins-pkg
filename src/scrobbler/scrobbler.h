#ifndef NET_H
#define NET_H 1

#include <libaudcore/tuple.h>

#define SC_CURL_TIMEOUT 60

gboolean sc_timeout(gpointer data);

int sc_idle(GMutex *);
void sc_init(char *, char *, char *);
void sc_addentry(GMutex *, Tuple *, int);
void sc_cleaner(void);
int sc_catch_error(void);
char *sc_fetch_error(void);
void sc_clear_error(void);
#endif
