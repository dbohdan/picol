/* An interactive Picol interpreter. */

#include "picol.h"

void set_interp_argv(picolInterp* pi, int offset, int argc, char** argv) {
    char buf[MAXSTR] = "";
    int i;
    picolSetIntVar(pi, "argc", argc - offset);
    for (i = offset; i < argc; i++) {
        LAPPEND(buf, argv[i]);
    }
    picolSetVar(pi, "argv", buf);
}

int main(int argc, char** argv) {
    picolInterp* pi = picolCreateInterp();
    char buf[MAXSTR] = "";
    int rc = 0;
    int i = 0;
    FILE* fp = fopen("init.pcl", "r");
    picolSetVar(pi, "argv0", argv[0]);
    picolSetVar(pi, "argv",  "");
    picolSetVar(pi, "argc",  "0");
    picolSetVar(pi, "auto_path", "");
    picolEval(pi, "array set env {}"); /* populated from real env on demand */
    if (fp) {
        fclose(fp);
        rc = picolSource(pi, "init.pcl");
        if (rc != PICOL_OK) {
            puts(pi->result);
        }
        pi->current = NULL; /* avoid misleading error traceback */
    }
    if (argc == 1) { /* no arguments - interactive mode */
        while (1) {
            printf("picol> ");
            fflush(stdout);
            if (fgets(buf, sizeof(buf), stdin) == NULL) return 0;
            rc = picolEval(pi, buf);
            if (pi->result[0] != '\0' || rc != PICOL_OK)
                printf("[%d] %s\n", rc, pi->result);
        }
    } else if (EQ(argv[1], "-e")) { /* script in argv[2] */
        set_interp_argv(pi, 1, argc, argv);
        rc = picolEval(pi, argv[2]);
        if (rc != PICOL_OK) {
            picolVar *v = picolGetVar(pi, "::errorInfo");
            if (v) {
                puts(v->val);
            }
        } else puts(pi->result);
    } else { /* first arg is file to source, rest goes to argv */
        picolSetVar(pi, "argv0", argv[1]);
        set_interp_argv(pi, 2, argc, argv);
        rc = picolSource(pi, argv[1]);
        if (rc != PICOL_OK) {
            picolVar *v = picolGetVar(pi, "::errorInfo");
            if (v) {
                puts(v->val);
            }
        }
    }
    return rc & 0xFF;
}
