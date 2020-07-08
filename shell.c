/* An interactive Picol shell. */

#define PICOL_IMPLEMENTATION
#include "picol.h"

#include "vendor/regexp.h"
#define REGEXP_WRAPPER_IMPLEMENTATION
#include "extensions/regexp-wrapper.h"

#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_UNIX
    #define PICOL_SHELL_LINENOISE
#endif
#ifdef PICOL_SHELL_LINENOISE
    #include "vendor/linenoise.h"
#endif

#define PICOL_SHELL_HISTORY_FILE "history.pcl"
#define PICOL_SHELL_HISTORY_LEN 100
#define PICOL_SHELL_PROMPT "picol> "

int set_interp_argv(picolInterp* interp, int offset, int argc, char** argv) {
    char buf[PICOL_MAX_STR] = "";
    int i;
    picolSetIntVar(interp, "argc", argc - offset);
    for (i = offset; i < argc; i++) {
        PICOL_LAPPEND(buf, argv[i]);
    }
    picolSetVar(interp, "argv", buf);
    return PICOL_OK;
}

int main(int argc, char** argv) {
    char buf[PICOL_MAX_STR] = "";
    int rc = 0;
    FILE* fp;
    picolInterp* interp = picolCreateInterp();
    picolRegisterCmd(interp, "regexp", picol_regexp, NULL);
    fp = fopen("init.pcl", "r");
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
        #ifdef PICOL_SHELL_LINENOISE
            linenoiseSetMultiLine(1);
            linenoiseHistorySetMaxLen(PICOL_SHELL_HISTORY_LEN);
            linenoiseHistoryLoad(PICOL_SHELL_HISTORY_FILE);
        #endif

        while (1) {
            #ifdef PICOL_SHELL_LINENOISE
                char* line = linenoise(PICOL_SHELL_PROMPT);
                if (line == NULL) {
                    break;
                }
                strncpy(buf, line, sizeof(buf));
                linenoiseHistoryAdd(buf);
                linenoiseFree(line);
            #else
                printf(PICOL_SHELL_PROMPT);
                fflush(stdout);
                if (fgets(buf, sizeof(buf), stdin) == NULL) {
                    break;
                }
            #endif

            rc = picolEval(interp, buf);
            if (interp->result[0] != '\0' || rc != PICOL_OK) {
                printf("[%d] %s\n", rc, interp->result);
            }
        }

        #ifdef PICOL_SHELL_LINENOISE
            linenoiseHistorySave(PICOL_SHELL_HISTORY_FILE);
        #endif
    } else if (argc == 3 && PICOL_EQ(argv[1], "-e")) { /* A script in argv[2]. */
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
