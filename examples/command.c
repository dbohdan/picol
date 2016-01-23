/* Picol use example: custom command. */

#include "picol.h"

COMMAND(square) {
    ARITY2(argc == 2, "square number"); 
    int n;
    SCAN_INT(n, argv[1])
    picolSetIntResult(i, n * n);
    return PICOL_OK;
}

void report_error(picolInterp* pi, int rc) {
    if (rc != PICOL_OK) {
        printf("[%d] %s\n", rc, pi->result);
    }
}

int main(int argc, char** argv) {
    picolInterp* pi = picolCreateInterp();
    picolRegisterCmd(pi, "square", picol_square, NULL);
    int rc = 0;
    rc = picolEval(pi, "puts [square]"); /* Wrong usage. */
    report_error(pi, rc);
    rc = picolEval(pi, "puts [square foo]"); /* Wrong usage. */
    report_error(pi, rc);
    rc = picolEval(pi, "puts [square 5]"); /* Correct usage. */
    report_error(pi, rc);
    return rc & 0xFF;
}
