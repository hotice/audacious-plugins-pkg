#ifndef XS_STIL_H
#define XS_STIL_H

#include "xmms-sid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Types
 */
typedef struct {
    gchar *name,
          *author,
          *title,
          *info;
} stil_subnode_t;


typedef struct _stil_node_t {
    gchar               *filename;
    gint                nsubTunes;
    stil_subnode_t      **subTunes;
    struct _stil_node_t *prev, *next;
} stil_node_t;


typedef struct {
    stil_node_t *nodes,
                **pindex;
    size_t      n;
} xs_stildb_t;


/* Functions
 */
gint            xs_stildb_read(xs_stildb_t *, gchar *);
gint            xs_stildb_index(xs_stildb_t *);
void            xs_stildb_free(xs_stildb_t *);
stil_node_t *   xs_stildb_get_node(xs_stildb_t *, gchar *);

#ifdef __cplusplus
}
#endif
#endif /* XS_STIL_H */
