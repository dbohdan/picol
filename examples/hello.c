/* Picol use example: Hello, World! */

#define PICOL_IMPLEMENTATION
#include "picol.h"

int main(int argc, char** argv) {
    picolInterp* i = picolCreateInterp();
    int rc = picolEval(i, "puts {Hello, World!}");
    if (rc != PICOL_OK) {
        printf("[%d] %s\n", rc, i->result);
    }
    return rc & 0xFF;
}
