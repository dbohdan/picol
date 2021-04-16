/* Picol use example: custom command. */

#define PICOL_CONFIGURATION
#define PICOL_MAX_STR          4096
#define PICOL_SOURCE_BUF_SIZE     1
#define PICOL_MAX_LEVEL          10
/* A minimal Picol configuration: no arrays, no I/O, no [interp] or [glob]
command. */
#define PICOL_FEATURE_ARRAYS  0
#define PICOL_FEATURE_GLOB    0
#define PICOL_FEATURE_INTERP  0
#define PICOL_FEATURE_IO      0
#define PICOL_FEATURE_PUTS    1

#define PICOL_IMPLEMENTATION
#include "picol.h"

PICOL_COMMAND(square) {
    int n;
    PICOL_UNUSED(pd);
 
    PICOL_ARITY2(argc == 2, "square number");
    PICOL_SCAN_INT(n, argv[1]);
    picolSetIntResult(interp, n * n);
    return PICOL_OK;
}

void report_error(picolInterp* interp, int rc) {
    if (rc != PICOL_OK) {
        printf("[%d] %s\n", rc, interp->result);
    }
}

int main(int argc, char** argv) {
    int rc = 0;
    PICOL_UNUSED(argc);
    PICOL_UNUSED(argv);
 
    /* Create an interpreter with no core commands. Do not call srand(). */
    picolInterp* interp = picolCreateInterp2(0, 0);
    /* Manually register only one built-in. */
    picolRegisterCmd(interp, "puts", picol_puts, NULL);
    /* Register our custom command. */
    picolRegisterCmd(interp, "square", picol_square, NULL);
    rc = picolEval(interp, "puts [square]"); /* Wrong usage. */
    report_error(interp, rc);
    rc = picolEval(interp, "puts [square foo]"); /* Wrong usage. */
    report_error(interp, rc);
    rc = picolEval(interp, "puts [square 5]"); /* Correct usage. */
    report_error(interp, rc);
    picolFreeInterp(interp);
    
    return rc & 0xFF;
}
