/* Picol use example: regexp extension. */

#define PICOL_IMPLEMENTATION
#include "picol.h"
#define REGEXP_IMPLEMENTATION
#include "vendor/regexp.h"
#define PICOL_REGEXP_WRAPPER_IMPLEMENTATION
#include "extensions/regexp-wrapper.h"

void eval_and_report(picolInterp* interp, char* command) {
    int rc;
    printf("   command: %s\n", command);
    rc = picolEval(interp, command);
    printf("    result: [%d] %s\n", rc, interp->result);
}

int main(int argc, char** argv) {
    picolInterp* interp = picolCreateInterp();
    PICOL_UNUSED(argc);
    PICOL_UNUSED(argv);
    
    picolRegisterCmd(interp, "regexp", picol_regexp, NULL);
    eval_and_report(interp, "regexp {***++} foo");
    eval_and_report(interp, "regexp bar foo");
    eval_and_report(interp, "regexp foo foo");
    eval_and_report(interp, "regexp (a+)(b+) aaabb match sub1 sub2 sub3");
    picolEval(
        interp,
        "puts \"     match: $match\nsubmatches: [list $sub1 $sub2 $sub3]\""
    );
    picolFreeInterp(interp);
    
    return 0;
}
