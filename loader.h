#include <conc.h>

/* struct type def shared with loader and ident asembler code */
typedef struct {
    int id;
    void *minint;
    void *memstart;
    Channel *up_in;
    Channel *dn_out[3];
    void *ldstart;
    void *entryp;
    void *wspace;
    void *ldaddr;
    int trantype;
} LOADGB;
