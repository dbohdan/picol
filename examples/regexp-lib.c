/* Picol use example: regexp library. */

#define PICOL_IMPLEMENTATION
#include "picol.h"
#include "vendor/regexp.h"

COMMAND(regexp) {
    ARITY2(argc >= 3, "regexp exp string ?matchVar? ?subMatchVar ...?");
    Reprog* p;
    Resub m;
    int i, result;
    const char* err;
    char match[PICOL_MAX_STR];
    p = regcomp(argv[1], 0, &err);
    if (p == NULL) {
        return picolErr1(interp, "can't compile regexp: %s", (char*) err);
    }
    result = !regexec(p, argv[2], &m, 0);
    if (result) {
        for (i = 0; (i < m.nsub) && (i + 3 < argc); i++) {
            /* n should always be less than PICOL_MAX_STR under normal
               operation, but we'll check it anyway in case the command was,
               e.g., run directly from C with pathological arguments. */
            size_t n = m.sub[i].ep - m.sub[i].sp;
            if (n > PICOL_MAX_STR - 1) {
                return picolErr1(interp,
                                 "match to be stored in variable "
                                 "\"%s\" too long",
                                 argv[i + 3]);
            }
            memcpy(match, m.sub[i].sp, n);
            match[n] = '\0';
            picolSetVar(interp, argv[i + 3], match);
        }
    }
    regfree(p);
    return picolSetBoolResult(interp, result);
}

void eval_and_report(picolInterp* interp, char* command) {
    printf("   command: %s\n", command);
    int rc = picolEval(interp, command);
    printf("    result: [%d] %s\n", rc, interp->result);
}

int main(int argc, char** argv) {
    picolInterp* interp = picolCreateInterp();
    picolRegisterCmd(interp, "regexp", picol_regexp, NULL);
    eval_and_report(interp, "regexp {***++} foo");
    eval_and_report(interp, "regexp bar foo");
    eval_and_report(interp, "regexp foo foo");
    eval_and_report(interp, "regexp (a+)(b+) aaabb match sub1 sub2 s");
    picolEval(interp,
              "puts \"     match: $match\nsubmatches: [list $sub1 $sub2]\"");
    return 0;
}
