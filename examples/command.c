/* Picol use example: custom command. */

#define PICOL_CONFIGURATION
#define MAXSTR 4096
/* A minimal Picol configuration: no arrays, no I/O, no [interp] or [glob]
command. */
#define PICOL_FEATURE_ARRAYS  0
#define PICOL_FEATURE_GLOB    0
#define PICOL_FEATURE_INTERP  0
#define PICOL_FEATURE_IO      0
#define PICOL_FEATURE_PUTS    1

#define PICOL_IMPLEMENTATION
#include "picol.h"

COMMAND(square) {
    ARITY2(argc == 2, "square number"); 
    int n;
    SCAN_INT(n, argv[1]);
    picolSetIntResult(i, n * n);
    return PICOL_OK;
}

void report_error(picolInterp* i, int rc) {
    if (rc != PICOL_OK) {
        printf("[%d] %s\n", rc, i->result);
    }
}

int main(int argc, char** argv) {
    picolInterp* i = picolCreateInterp();
    picolRegisterCmd(i, "square", picol_square, NULL);
    int rc = 0;
    rc = picolEval(i, "puts [square]"); /* Wrong usage. */
    report_error(i, rc);
    rc = picolEval(i, "puts [square foo]"); /* Wrong usage. */
    report_error(i, rc);
    rc = picolEval(i, "puts [square 5]"); /* Correct usage. */
    report_error(i, rc);
    return rc & 0xFF;
}
