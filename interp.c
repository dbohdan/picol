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
    picolEval(i, "array set env {}"); /* populated from real env on demand */
    if (fp) {
        fclose(fp);
        rc = picolSource(i, "init.pcl");
        if (rc != PICOL_OK) {
            puts(i->result);
        }
        i->current = NULL; /* avoid misleading error traceback */
    }
    if (argc == 1) { /* no arguments - interactive mode */
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
    } else if (EQ(argv[1], "-e")) { /* script in argv[2] */
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
    } else { /* first arg is file to source, rest goes to argv */
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
