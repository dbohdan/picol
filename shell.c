/* An interactive Picol shell. */

/* --- Libraries --- */

#define PICOL_IMPLEMENTATION
#include "picol.h"

#include "vendor/regexp.h"
#define PICOL_REGEXP_WRAPPER_IMPLEMENTATION
#include "extensions/regexp-wrapper.h"

#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_UNIX
    #define PICOL_SHELL_LINENOISE 1
    #include "vendor/linenoise.h"
#else
    #define PICOL_SHELL_LINENOISE 0
#endif

/* --- Configuration --- */

/* History is currently only available on *nix. */
#define PICOL_SHELL_HISTORY_FILE ".picolsh_history"
#define PICOL_SHELL_HISTORY_LEN 100

#define PICOL_SHELL_INIT_FILE_UNIX ".picolshrc"
#define PICOL_SHELL_INIT_FILE_WINDOWS "picolshrc.pcl"
#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_UNIX
    #define PICOL_SHELL_INIT_FILE PICOL_SHELL_INIT_FILE_UNIX
#elif PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS
    #define PICOL_SHELL_INIT_FILE PICOL_SHELL_INIT_FILE_WINDOWS
#else
    #define PICOL_SHELL_INIT_FILE ""
#endif

#define PICOL_SHELL_PROMPT "picol> "

/* --- Implementation --- */

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

int home_dir_path(char* dest, size_t size, const char* append) {
    char* dir = NULL;
    #if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_UNIX
        dir = getenv("HOME");
    #elif PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS
        dir = getenv("USERPROFILE");
    #endif

    if (size == 0 || dir == NULL) {
        return PICOL_ERR;
    }

    strncpy(dest, dir, size);
    strcat(dest, append);
    return PICOL_OK;
}

int main(int argc, char** argv) {
    char buf[PICOL_MAX_STR] = "";
    int rc = 0;
    FILE* fp = NULL;
    picolInterp* interp = picolCreateInterp();
    picolRegisterCmd(interp, "regexp", picol_regexp, NULL);

    picolSetVar(interp, "argv0", argv[0]);
    picolSetVar(interp, "argv",  "");
    picolSetVar(interp, "argc",  "0");
    picolSetVar(interp, "auto_path", "");
    /* The array ::env is lazily populated with the environment variables'
       values. */
    picolEval(interp, "array set env {}");
    if (argc == 1) { /* No arguments - interactive mode. */
        #if PICOL_SHELL_LINENOISE
            char history_file_path[PICOL_MAX_STR] = "";
        #endif

        rc = home_dir_path(buf, sizeof(buf), "/" PICOL_SHELL_INIT_FILE);
        if (rc == PICOL_OK) {
            fp = fopen(buf, "r");
        }

        if (fp != NULL) {
            fclose(fp);
            rc = picolSource(interp, buf);
            if (rc != PICOL_OK) {
                puts(interp->result);
            }
            interp->current = NULL; /* Prevent a misleading error traceback. */
        }

        #if PICOL_SHELL_LINENOISE
            linenoiseSetMultiLine(1);
            linenoiseHistorySetMaxLen(PICOL_SHELL_HISTORY_LEN);
            rc = home_dir_path(
                history_file_path,
                sizeof(history_file_path),
                "/" PICOL_SHELL_HISTORY_FILE
            );
            if (rc == PICOL_OK) {
                linenoiseHistoryLoad(history_file_path);
            }
        #endif

        while (1) {
            #if PICOL_SHELL_LINENOISE
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
            linenoiseHistorySave(history_file_path);
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
