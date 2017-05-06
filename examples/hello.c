/* Picol use example: "Hello, World!" */

#define PICOL_IMPLEMENTATION
#include "picol.h"

int main(int argc, char** argv) {
    picolInterp* interp = picolCreateInterp();
    int rc = picolEval(interp, "puts {Hello, World!}");
    if (rc != PICOL_OK) {
        printf("[%d] %s\n", rc, interp->result);
    }
    return rc & 0xFF;
}
