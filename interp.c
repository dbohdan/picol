/* An interactive Picol interpreter. */

#define PICOL_IMPLEMENTATION
#include "picol.h"

int set_interp_argv(picolInterp* i, int offset, int argc, char** argv) {
    char buf[MAXSTR] = "";
    int j;
    picolSetIntVar(i, "argc", argc - offset);
    for (j = offset; j < argc; j++) {
        LAPPEND(buf, argv[j]);
    }
    picolSetVar(i, "argv", buf);
    return PICOL_OK;
}

int main(int argc, char** argv) {
    picolInterp* i = picolCreateInterp();
    char buf[MAXSTR] = "";
    int rc = 0;
    FILE* fp = fopen("init.pcl", "r");
    picolSetVar(i, "argv0", argv[0]);
    picolSetVar(i, "argv",  "");
    picolSetVar(i, "argc",  "0");
    picolSetVar(i, "auto_path", "");
    /* The array ::env is lazily populated with environment variables. */
    picolEval(i, "array set env {}");
    if (fp) {
        fclose(fp);
        rc = picolSource(i, "init.pcl");
        if (rc != PICOL_OK) {
            puts(i->result);
        }
        i->current = NULL; /* Prevent a misleading error traceback. */
    }
    if (argc == 1) { /* No arguments - interactive mode. */
        while (1) {
            printf("picol> ");
            fflush(stdout);
            if (fgets(buf, sizeof(buf), stdin) == NULL) {
                return 0;
            }
            rc = picolEval(i, buf);
            if (i->result[0] != '\0' || rc != PICOL_OK) {
                printf("[%d] %s\n", rc, i->result);
            }
        }
    } else if (EQ(argv[1], "-e")) { /* A script in argv[2]. */
        set_interp_argv(i, 1, argc, argv);
        rc = picolEval(i, argv[2]);
        if (rc != PICOL_OK) {
            picolVar *v = picolGetVar(i, "::errorInfo");
            if (v) {
                puts(v->val);
            }
        } else {
            puts(i->result);
        }
    } else { /* The first arg is the file to source; the rest goes into argv. */
        picolSetVar(i, "argv0", argv[1]);
        set_interp_argv(i, 2, argc, argv);
        rc = picolSource(i, argv[1]);
        if (rc != PICOL_OK) {
            picolVar *v = picolGetVar(i, "::errorInfo");
            if (v) {
                puts(v->val);
            }
        }
    }
    return rc & 0xFF;
}
