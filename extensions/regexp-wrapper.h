#ifndef REGEXP_WRAPPER_H
COMMAND(regexp);
#endif /* REGEXP_WRAPPER_H */

#ifdef REGEXP_WRAPPER_IMPLEMENTATION
COMMAND(regexp) {
    Reprog* p;
    Resub m;
    int i, result;
    const char* err;
    char match[PICOL_MAX_STR];
    ARITY2(argc >= 3, "regexp exp string ?matchVar? ?subMatchVar ...?");
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
        for (i = m.nsub; i + 3 < argc; i++) {
            picolSetVar(interp, argv[i + 3], "");
        }
    }
    regfree(p);
    return picolSetBoolResult(interp, result);
}
#endif /* REGEXP_WRAPPER_IMPLEMENTATION */
