/* An interactive Picol interpreter. */

#define PICOL_IMPLEMENTATION
#include "picol.h"
#include "vendor/regexp.h"
#define REGEXP_WRAPPER_IMPLEMENTATION
#include "extensions/regexp-wrapper.h"

int set_interp_argv(picolInterp* interp, int offset, int argc, char** argv) {
    char buf[PICOL_MAX_STR] = "";
    int i;
    picolSetIntVar(interp, "argc", argc - offset);
    for (i = offset; i < argc; i++) {
        LAPPEND(buf, argv[i]);
    }
    picolSetVar(interp, "argv", buf);
    return PICOL_OK;
}

int main(int argc, char** argv) {
    picolInterp* interp = picolCreateInterp();
    picolRegisterCmd(interp, "regexp", picol_regexp, NULL);
    char buf[PICOL_MAX_STR] = "";
    int rc = 0;
    FILE* fp = fopen("init.pcl", "r");
    picolSetVar(interp, "argv0", argv[0]);
    picolSetVar(interp, "argv",  "");
    picolSetVar(interp, "argc",  "0");
    picolSetVar(interp, "auto_path", "");
    /* The array ::env is lazily populated with the environment variables'
       values. */
    picolEval(interp, "array set env {}");
    if (fp != NULL) {
        fclose(fp);
        rc = picolSource(interp, "init.pcl");
        if (rc != PICOL_OK) {
            puts(interp->result);
        }
        interp->current = NULL; /* Prevent a misleading error traceback. */
    }
    if (argc == 1) { /* No arguments - interactive mode. */
        while (1) {
            printf("picol> ");
            fflush(stdout);
            if (fgets(buf, sizeof(buf), stdin) == NULL) {
                return 0;
            }
            rc = picolEval(interp, buf);
            if (interp->result[0] != '\0' || rc != PICOL_OK) {
                printf("[%d] %s\n", rc, interp->result);
            }
        }
    } else if (EQ(argv[1], "-e")) { /* A script in argv[2]. */
        set_interp_argv(interp, 1, argc, argv);
        rc = picolEval(interp, argv[2]);
        if (rc != PICOL_OK) {
            picolVar *v = picolGetVar(interp, "::errorInfo");
            if (v != NULL) {
                puts(v->val);
            }
        } else {
            puts(interp->result);
        }
    } else { /* The first arg is the file to source; the rest goes into argv. */
        picolSetVar(interp, "argv0", argv[1]);
        set_interp_argv(interp, 2, argc, argv);
        rc = picolSource(interp, argv[1]);
        if (rc != PICOL_OK) {
            picolVar *v = picolGetVar(interp, "::errorInfo");
            if (v != NULL) {
                puts(v->val);
            }
        }
    }
    return rc & 0xFF;
}
