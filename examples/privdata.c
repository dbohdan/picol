/* Picol use example: command with private data. */

#define PICOL_IMPLEMENTATION
#include "picol.h"

PICOL_COMMAND(store) {
    PICOL_ARITY2(argc == 1 || argc == 2, "store ?number?");

    if (argc == 1) {
        picolSetIntResult(interp, *(int*)pd);
        return PICOL_OK;
    }
    
    PICOL_SCAN_INT(*(int*)pd, argv[1]);
    return PICOL_OK;
}

void report_error(picolInterp* interp, int rc) {
    if (rc != PICOL_OK) {
        printf("[%d] %s\n", rc, interp->result);
    }
}

int main(int argc, char** argv) {
    int rc = 0;
    int storage = 0;

    PICOL_UNUSED(argc);
    PICOL_UNUSED(argv);
 
    picolInterp* interp = picolCreateInterp();
    picolRegisterCmd(interp, "store", picol_store, &storage);
    rc = picolEval(interp, "puts [store]");
    report_error(interp, rc);
    rc = picolEval(interp, "store 108");
    report_error(interp, rc);
    rc = picolEval(interp, "puts [store]");
    report_error(interp, rc);

    picolFreeInterp(interp);

    return rc & 0xFF;
}
