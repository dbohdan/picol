/* Picol use example: Hello, World! */

#include "picol.h"

int main(int argc, char **argv) {
    picolInterp *pi = picolCreateInterp();
    char buf[MAXSTR] = "";
    int rc = picolEval(pi, "puts {Hello, World!}");
    if (rc != PICOL_OK) {
        printf("[%d] %s\n", rc, pi->result);
    }
    return rc & 0xFF;
}
