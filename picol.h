/* Tcl in ~ 500 lines of code by Salvatore antirez Sanfilippo. BSD-licensed.
 * 2007-04-01 Added by suchenwi: many more commands, see below.
 * 2016-2020  Misc. improvements and fixes by dbohdan. See the Fossil timeline.
 *
 * This header file contains both the interface and the implementation for
 * Picol. To instantiate the implementation put the line
 *     #define PICOL_IMPLEMENTATION
 * in a single source code file of your project above where you include this
 * file. To override the default compilation options place a modified copy of
 * the entire PICOL_CONFIGURATION section above where you include this file.
 */

/* -------------------------------------------------------------------------- */

#ifndef PICOL_CONFIGURATION
#define PICOL_CONFIGURATION

#define PICOL_MAX_STR        4096
#ifdef _MSC_VER
#    define PICOL_MAX_LEVEL  10
#else
#    define PICOL_MAX_LEVEL  30
#endif

#ifdef __MINGW32__
#    include <_mingw.h> /* For __MINGW64_VERSION_MAJOR. */
#endif

/* Optional features. Define as zero to disable. */
#define PICOL_FEATURE_ARRAYS    1
#if defined(_MSC_VER) || defined(__MINGW64_VERSION_MAJOR)
/*                       ^^^ MinGW-w64 lacks glob.h. */
#    define PICOL_FEATURE_GLOB  0
#else
#    define PICOL_FEATURE_GLOB  1
#endif
#define PICOL_FEATURE_INTERP    1
#define PICOL_FEATURE_IO        1
#define PICOL_FEATURE_PUTS      1

#endif /* PICOL_CONFIGURATION */

/* -------------------------------------------------------------------------- */

#ifndef PICOL_H
#define PICOL_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define PICOL_PATCHLEVEL "0.4.0"

/* MSVC compatibility. */
#ifdef _MSC_VER
#    define PICOL_GETCWD _getcwd
#    define PICOL_GETPID GetCurrentProcessId
#    define PICOL_POPEN  _popen
#    define PICOL_PCLOSE _pclose
#    define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

#    if _MSC_VER < 1900
#        define PICOL_SNPRINTF(str, size, format, ...) \
                _snprintf_s(str, size, _TRUNCATE, format, __VA_ARGS__)
#    endif
/*   We also include <windows.h> below, but for all compilers on Windows,
     not just MSVC. */
#else
#    include <unistd.h>
#    define PICOL_GETCWD getcwd
#    define PICOL_GETPID getpid
#    define PICOL_POPEN  popen
#    define PICOL_PCLOSE pclose
#    define PICOL_SNPRINTF(str, size, format, ...) \
            snprintf(str, size, format, __VA_ARGS__)
#endif /* _MSC_VER */

#if PICOL_FEATURE_GLOB
#    include <glob.h>
#endif

/* The value for ::tcl_platform(engine), which Tcl code can use to distinguish
   between Tcl implementations. */
#define PICOL_TCL_PLATFORM_ENGINE_STRING "Picol"

/* Crudely detect the platform we're being compiled on. Choose the value for
   ::tcl_platform(platform) and a corresponding numerical value we can use for
   conditional compilation accordingly. */
#define PICOL_TCL_PLATFORM_UNKNOWN  0
#define PICOL_TCL_PLATFORM_UNIX     1
#define PICOL_TCL_PLATFORM_WINDOWS  2
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || \
        defined(_MSC_VER)
/* These names are ugly, but consistent and arguably self-explanatory. */
#    define PICOL_TCL_PLATFORM_PLATFORM         PICOL_TCL_PLATFORM_WINDOWS
#    define PICOL_TCL_PLATFORM_PLATFORM_STRING  "windows"
#    include <windows.h>
#elif defined(_POSIX_VERSION) && !defined(__MSDOS__)
/*        A workaround for DJGPP ^^^, which defines _POSIX_VERSION. */
#    define PICOL_TCL_PLATFORM_PLATFORM         PICOL_TCL_PLATFORM_UNIX
#    define PICOL_TCL_PLATFORM_PLATFORM_STRING  "unix"
#    if !defined(PICOL_CAN_USE_CLOCK_GETTIME)
#        if _POSIX_C_SOURCE >= 199309L && (!defined(__GNU_LIBRARY__) || \
            (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 17))
#            define PICOL_CAN_USE_CLOCK_GETTIME 1
#        else
#            define PICOL_CAN_USE_CLOCK_GETTIME 0
#        endif
#    endif
#else
#    define PICOL_TCL_PLATFORM_PLATFORM         PICOL_TCL_PLATFORM_UNKNOWN
#    define PICOL_TCL_PLATFORM_PLATFORM_STRING  "unknown"
#endif

#define PICOL_INFO_SCRIPT_VAR "::_script_"

/* ---------- Most macros need the picol_* environment: argc, argv and interp */
#define PICOL_ERROR_TOO_LONG "string too long"

#define PICOL_APPEND(dst, src) \
    do { \
        if ((strlen(dst) + strlen(src)) > sizeof(dst) - 1) { \
            return picolErr(interp, PICOL_ERROR_TOO_LONG); \
        } \
        strcat(dst, src); \
    } while (0)

#define PICOL_APPEND_BREAK(dst, src, too_long) \
    { \
        if ((strlen(dst) + strlen(src)) > sizeof(dst) - 1) { \
            too_long = 1; break; \
        } \
        strcat(dst, src); \
    }

#define PICOL_ARITY(x) \
    do { \
        if (!(x)) { \
            return picolErr1(interp, "wrong # args for \"%s\"", argv[0]); \
        } \
    } while (0);

#define PICOL_ARITY2(x,s) \
    do { \
        if (!(x)) { \
            return picolErr1(interp, "wrong # args: should be \"%s\"", s); \
        } \
    } while (0)

#define PICOL_COLONED(_p) \
    (*(_p) == ':' && *(_p+1) == ':') /* indicates global */

#define PICOL_COMMAND(c) \
    int picol_##c(picolInterp* interp, int argc, \
    char** argv, void* pd)

#define PICOL_EQ(a,b) \
    ((*a)==(*b) && !strcmp(a, b))

#define PICOL_FOLD(init,step,n) \
    do {init; for(p=n;p<argc;p++) { \
    PICOL_SCAN_INT(a, argv[p]);step;}} while (0)

#define PICOL_FOREACH(_v,_p,_s) \
    for(_p = picolParseList(_s,_v); _p; _p = picolParseList(_p,_v))

#define PICOL_LAPPEND(dst, src) \
    do { \
        int needbraces = picolNeedsBraces(src); \
        if (*dst != '\0') {PICOL_APPEND(dst, " ");} \
        if (needbraces) {PICOL_APPEND(dst, "{");} \
        PICOL_APPEND(dst, src); \
        if (needbraces) {PICOL_APPEND(dst, "}");} \
    } while (0)

/* This is the unchecked version, for functions without access to 'interp'. */
#define PICOL_LAPPEND_X(dst,src) \
    do { \
        int needbraces = picolNeedsBraces(src); \
        if (*dst != '\0') strcat(dst, " "); \
        if (needbraces) {strcat(dst, "{");} \
        strcat(dst, src); \
        if (needbraces) {strcat(dst, "}");} \
    } while (0)

#define PICOL_GET_VAR(_v,_n) \
    _v = picolGetVar(interp, _n); \
    if (_v == NULL) return picolErr1(interp, \
    "can't read \"%s\": no such variable", _n);

#define PICOL_PARSED(_t) \
    do {p->end = p->p-1; p->type = _t;} while (0)

#define PICOL_RETURN_PARSED(_t) \
    do {PICOL_PARSED(_t); return PICOL_OK;} while (0)

#define PICOL_SCAN_INT(v,x) \
    do { \
        int base = picolIsInt(x); \
        if (base > 0) {v = picolScanInt(x, base);} \
        else { return picolErr1(interp, "expected integer " \
                                "but got \"%s\"", x); } \
    } while (0)

#define PICOL_SCAN_PTR(v,x) \
    do { \
        void* _p; \
        if ((_p=picolScanPtr(x))) {v = _p;} \
        else { return picolErr1(interp, "expected pointer but got \"%s\"", x);}\
    } while (0)

#define PICOL_SUBCMD(x) \
    (PICOL_EQ(argv[1], x))

enum {PICOL_OK, PICOL_ERR, PICOL_RETURN, PICOL_BREAK, PICOL_CONTINUE};
enum {PICOL_PT_ESC, PICOL_PT_STR, PICOL_PT_CMD, PICOL_PT_VAR, PICOL_PT_SEP,
      PICOL_PT_EOL, PICOL_PT_EOF, PICOL_PT_XPND};
enum {PICOL_PTR_NONE, PICOL_PTR_CHAN, PICOL_PTR_ARRAY, PICOL_PTR_INTERP};

/* ------------------------------------------------------------------- types */
typedef struct picolParser {
    char*  text;
    char*  p;           /* current text position */
    size_t len;         /* remaining length */
    char*  start;       /* token start */
    char*  end;         /* token end */
    int    type;        /* token type, PICOL_PT_... */
    int    insidequote; /* True if inside " " */
    int    expand;      /* true after {*} */
} picolParser;

typedef struct picolVar {
    char*  name;
    char*  val;
    struct picolVar* next;
} picolVar;

struct picolInterp; /* forward declaration */
typedef int (*picol_Func)(struct picolInterp *interp, int argc, char **argv, \
                          void *pd);

typedef struct picolCmd {
    char*            name;
    picol_Func       func;
    void*            privdata;
    struct picolCmd* next;
} picolCmd;

typedef struct picolCallFrame {
    picolVar*              vars;
    char*                  command;
    struct picolCallFrame* parent; /* parent is NULL at top level */
} picolCallFrame;

typedef struct picolPtr {
    void*             ptr;
    int               type;
    struct picolPtr*  next;
} picolPtr;

typedef struct picolInterp {
    int             level;      /* Level of scope nesting */
    int             maxlevel;
    picolCallFrame* callframe;
    picolCmd*       commands;
    char*           current;    /* currently executed command */
    char*           result;
    int             debug;      /* 1 to display each command, 0 not to */
    picolPtr*       validptrs;
} picolInterp;

#define PICOL_ARR_BUCKETS 32

typedef struct picolArray {
    picolVar* table[PICOL_ARR_BUCKETS];
    int       size;
} picolArray;

/* Ease of use macros. */

#define picolEval(_i,_t)             picolEval2(_i, _t, 1)
#define picolGetGlobalVar(_i,_n)     picolGetVar2(_i, _n, 1)
#define picolGetVar(_i,_n)           picolGetVar2(_i, _n, 0)
#define picolSetBoolResult(_i,x)     picolSetFmtResult(_i, "%d", !!x)
#define picolSetGlobalVar(_i,_n,_v)  picolSetVar2(_i, _n, _v, 1)
#define picolSetIntResult(_i,x)      picolSetFmtResult(_i, "%d", x)
#define picolSetVar(_i,_n,_v)        picolSetVar2(_i, _n, _v, 0)
#define picolSubst(_i,_t)            picolEval2(_i, _t, 0)

/* prototypes */

char* picolList(char* buf, int argc, char** argv);
int   picolNeedsBraces(char* str);
char* picolParseList(char* start, char* trg);
char* picolStrFirstTrailing(char* str, char chr);
char* picolStrRev(char* str);
char* picolToLower(char* str);
char* picolToUpper(char* str);
PICOL_COMMAND(abs);
#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_UNIX || \
    PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS
PICOL_COMMAND(after);
#endif
PICOL_COMMAND(append);
PICOL_COMMAND(apply);
PICOL_COMMAND(bitwise_not);
PICOL_COMMAND(break);
PICOL_COMMAND(catch);
PICOL_COMMAND(clock);
PICOL_COMMAND(concat);
PICOL_COMMAND(continue);
PICOL_COMMAND(debug);
PICOL_COMMAND(error);
PICOL_COMMAND(eval);
PICOL_COMMAND(expr);
/* [file delete|exists|isdirectory|isfile|size] are enabled with
   PICOL_FEATURE_IO. */
PICOL_COMMAND(file);
PICOL_COMMAND(for);
PICOL_COMMAND(foreach);
PICOL_COMMAND(format);
PICOL_COMMAND(global);
PICOL_COMMAND(if);
PICOL_COMMAND(incr);
PICOL_COMMAND(info);
PICOL_COMMAND(join);
PICOL_COMMAND(lappend);
PICOL_COMMAND(lassign);
PICOL_COMMAND(lindex);
PICOL_COMMAND(linsert);
PICOL_COMMAND(list);
PICOL_COMMAND(llength);
PICOL_COMMAND(lmap);
PICOL_COMMAND(lrange);
PICOL_COMMAND(lrepeat);
PICOL_COMMAND(lreplace);
PICOL_COMMAND(lreverse);
PICOL_COMMAND(lsearch);
PICOL_COMMAND(lset);
PICOL_COMMAND(lsort);
PICOL_COMMAND(max);
PICOL_COMMAND(min);
PICOL_COMMAND(not);
PICOL_COMMAND(pid);
PICOL_COMMAND(proc);
PICOL_COMMAND(rand);
PICOL_COMMAND(rename);
PICOL_COMMAND(return);
PICOL_COMMAND(scan);
PICOL_COMMAND(set);
PICOL_COMMAND(split);
PICOL_COMMAND(string);
PICOL_COMMAND(subst);
PICOL_COMMAND(switch);
PICOL_COMMAND(time);
PICOL_COMMAND(trace);
PICOL_COMMAND(try);
PICOL_COMMAND(unset);
PICOL_COMMAND(uplevel);
PICOL_COMMAND(variable);
PICOL_COMMAND(while);
#if PICOL_FEATURE_ARRAYS
    PICOL_COMMAND(array);

    picolArray* picolArrCreate(picolInterp *interp, char *name);
    int         picolArrDestroy(picolInterp* interp, char* name);
    int         picolArrDestroy1(picolArray* ap);
    char*       picolArrGet(picolArray *ap, char* pat, char* buf, int mode);
    picolVar*   picolArrGet1(picolArray* ap, char* key);
    picolArray* picolArrFindByName(picolInterp* interp, char* name, int create,
                                   char* key_dest);
    int         picolArrUnset(picolArray* ap, char* key);
    int         picolArrUnset1(picolInterp *interp, char *name);
    picolVar*   picolArrSet(picolArray* ap, char* key, char* value);
    picolVar*   picolArrSet1(picolInterp *interp, char *name, char *value);
    char*       picolArrStat(picolArray *ap, char* buf, size_t buf_size);
    int         picolHash(char* key, int modul);
#endif
#if PICOL_FEATURE_GLOB
    PICOL_COMMAND(glob);
#endif
#if PICOL_FEATURE_INTERP
    PICOL_COMMAND(interp);
#endif
#if PICOL_FEATURE_IO
    PICOL_COMMAND(cd);
    PICOL_COMMAND(exec);
    PICOL_COMMAND(exit);
    PICOL_COMMAND(gets);
    PICOL_COMMAND(open);
    PICOL_COMMAND(pwd);
    PICOL_COMMAND(rawexec);
    PICOL_COMMAND(read);
    PICOL_COMMAND(source);

    int picolFileUtil(picolInterp *interp, int argc, char **argv, void *pd);
#endif
#if PICOL_FEATURE_PUTS
    PICOL_COMMAND(puts);
#endif
int picolValidPtrAdd(picolInterp *interp, int type, void* ptr);
int picolValidPtrFind(picolInterp *interp, int type, void* ptr);
int picolValidPtrRemove(picolInterp *interp, int type, void* ptr);
int picolCallProc(picolInterp *interp, int argc, char **argv, void *pd);
char* picolConcat(char* buf, int argc, char** argv);
int picolCondition(picolInterp *interp, char* str);
int picolErr1(picolInterp *interp, char* format, char* arg);
int picolErr(picolInterp *interp, char* str);
int picolEval2(picolInterp *interp, char *t, int mode);
size_t picolExpandLC(char* destination, char* source, size_t num);
int picol_EqNe(picolInterp* interp, int argc, char** argv, void* pd);
int picolGetToken(picolInterp *interp, picolParser *p);
int picol_InNi(picolInterp *interp, int argc, char **argv, void *pd);
int picolIsDirectory(char* path);
int picolIsInt(char* str);
int picolLmap(picolInterp* interp, char* vars, char* list, char* body,
              int accumulate);
int picolLsort(picolInterp *interp, int argc, char **argv, void *pd);
int picolMatch(char* pat, char* str);
int picol_Math(picolInterp *interp, int argc, char **argv, void *pd);
int picolParseBrace(picolParser *p);
int picolParseCmd(picolParser *p);
int picolParseComment(picolParser *p);
int picolParseEol(picolParser *p);
int picolParseSep(picolParser *p);
int picolParseString(picolParser *p);
int picolParseVar(picolParser *p);
int picolRegisterCmd(picolInterp *interp, char *name, picol_Func f, void *pd);
int picolScanInt(char* str, int base);
int picolSetFmtResult(picolInterp* interp, char* fmt, int result);
int picolSetIntVar(picolInterp *interp, char *name, int value);
int picolSetResult(picolInterp *interp, char *s);
int picolSetVar2(picolInterp *interp, char *name, char *val, int glob);
int picolStrCompare(const char* a, const char* b, size_t len, int nocase);
int picolReplace(char* str, char* from, char* to, int nocase);
int picolSource(picolInterp *interp, char *filename);
int picolQuoteForShell(char* dest, int argc, char** argv);
int picolWildEq(char* pat, char* str, int n);
int picolQsortCompStr(const void* a, const void *b);
int picolQsortCompStrDecr(const void* a, const void *b);
int picolQsortCompInt(const void* a, const void *b);
picolCmd *picolGetCmd(picolInterp *interp, char *name);
picolInterp* picolCreateInterp(void);
picolInterp* picolCreateInterp2(int register_core_cmds, int randomize);
picolVar *picolGetVar2(picolInterp *interp, char *name, int glob);
void picolDropCallFrame(picolInterp *interp);
void picolEscape(char *str);
void picolInitInterp(picolInterp *interp);
void picolInitParser(picolParser *p, char *text);
void* picolScanPtr(char* str);
void picolRegisterCoreCmds(picolInterp *interp);

#endif /* PICOL_H */

/* -------------------------------------------------------------------------- */

#ifdef PICOL_IMPLEMENTATION

/* ---------------------------====-------------------------- parser functions */
void picolInitParser(picolParser* p, char* text) {
    p->text  = p->p = text;
    p->len   = strlen(text);
    p->start = 0;
    p->end = 0;
    p->insidequote = 0;
    p->type  = PICOL_PT_EOL;
    p->expand = 0;
}
int picolParseSep(picolParser* p) {
    p->start = p->p;
    while (isspace(*p->p) || (*p->p=='\\' && *(p->p+1)=='\n')) {
        p->p++;
        p->len--;
    }
    PICOL_RETURN_PARSED(PICOL_PT_SEP);
}
int picolParseEol(picolParser* p) {
    p->start = p->p;
    while (isspace(*p->p) || *p->p == ';') {
        p->p++;
        p->len--;
    }
    PICOL_RETURN_PARSED(PICOL_PT_EOL);
}
int picolParseCmd(picolParser* p) {
    int level = 1, blevel = 0;
    p->start = ++p->p;
    p->len--;
    while (p->len) {
        if (*p->p == '[' && blevel == 0) {
            level++;
        } else if (*p->p == ']' && blevel == 0) {
            if (!--level) {
                break;
            }
        } else if (*p->p == '\\') {
            p->p++;
            p->len--;
        } else if (*p->p == '{') {
            blevel++;
        } else if (*p->p == '}') {
            if (blevel != 0) {
                blevel--;
            }
        }
        p->p++;
        p->len--;
    }
    p->end  = p->p-1;
    p->type = PICOL_PT_CMD;
    if (*p->p == ']') {
        p->p++;
        p->len--;
    }
    return PICOL_OK;
}
int picolParseBrace(picolParser* p) {
    int level = 1;
    p->start = ++p->p;
    p->len--;
    while (1) {
        if (p->len >= 2 && *p->p == '\\') {
            p->p++;
            p->len--;
        } else if (p->len == 0 || *p->p == '}') {
            level--;
            if (level == 0 || p->len == 0) {
                p->end = p->p-1;
                if (p->len) {
                    p->p++; /* Skip final closed brace */
                    p->len--;
                }
                p->type = PICOL_PT_STR;
                return PICOL_OK;
            }
        } else if (*p->p == '{') {
            level++;
        }
        p->p++;
        p->len--;
    }
}
int picolParseString(picolParser* p) {
    int newword = (p->type == PICOL_PT_SEP || p->type == PICOL_PT_EOL ||
                   p->type == PICOL_PT_STR);
    if (p->len >= 3 && !strncmp(p->p, "{*}", 3)) {
        p->expand = 1;
        p->p += 3;
        p->len -= 3; /* skip the {*} expand indicator */
    }
    if (newword && *p->p == '{') {
        return picolParseBrace(p);
    } else if (newword && *p->p == '"') {
        p->insidequote = 1;
        p->p++;
        p->len--;
    }
    for (p->start = p->p; 1; p->p++, p->len--) {
        if (p->len == 0) {
            PICOL_RETURN_PARSED(PICOL_PT_ESC);
        }
        switch(*p->p) {
        case '\\':
            if (p->len >= 2) {
                p->p++;
                p->len--;
            };
            break;
        case '$':
        case '[':
            PICOL_RETURN_PARSED(PICOL_PT_ESC);
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case ';':
            if (!p->insidequote) {
                PICOL_RETURN_PARSED(PICOL_PT_ESC);
            }
            break;
        case '"':
            if (p->insidequote) {
                p->end = p->p-1;
                p->type = PICOL_PT_ESC;
                p->p++;
                p->len--;
                p->insidequote = 0;
                return PICOL_OK;
            }
            break;
        }
    }
}
int picolParseVar(picolParser* p) {
    int parened = 0;
    p->start = ++p->p;
    p->len--; /* skip the '$' */
    if (*p->p == '{') {
        picolParseBrace(p);
        p->type = PICOL_PT_VAR;
        return PICOL_OK;
    }
    if (PICOL_COLONED(p->p)) {
        p->p += 2;
        p->len -= 2;
    }

    while (isalnum(*p->p) || *p->p == '_' || *p->p == '(' || *p->p == ')') {
        if (*p->p=='(') {
            parened = 1;
        }
        p->p++;
        p->len--;
    }
    if (!parened && *(p->p-1) == ')') {
        p->p--;
        p->len++;
    }
    if (p->start == p->p) {
        /* It's just the single char string "$" */
        picolParseString(p);
        p->start--;
        p->len++; /* back to the '$' character */
        p->type = PICOL_PT_STR;
        return PICOL_OK;
    } else {
        PICOL_RETURN_PARSED(PICOL_PT_VAR);
    }
}
int picolParseComment(picolParser* p) {
    while (p->len && *p->p != '\n') {
        if (*p->p=='\\' && *(p->p+1)=='\n') {
            p->p++;
            p->len--;
        }
        p->p++;
        p->len--;
    }
    return PICOL_OK;
} /*------------------------------------ General functions: variables, errors */
int picolSetResult(picolInterp* interp, char* s) {
    free(interp->result);
    interp->result = strdup(s);
    return PICOL_OK;
}
int picolSetFmtResult(picolInterp* interp, char* fmt, int result) {
    char buf[32];
    PICOL_SNPRINTF(buf, sizeof(buf), fmt, result);
    return picolSetResult(interp, buf);
}
#define PICOL_APPEND_BREAK_PICOLERR(src) \
    { \
        size_t src_len = strlen(src); \
        if ((len + src_len) > sizeof(buf) - 1) { \
            too_long = 1; break; \
        } \
        strcat(buf, src); \
        added_len += src_len; \
    }
int picolErr(picolInterp* interp, char* str) {
    char buf[PICOL_MAX_STR] = "";
    int too_long = 0;
    size_t len = 0, added_len = 0;

    do {
        picolCallFrame* cf;
        PICOL_APPEND_BREAK_PICOLERR(str);
        len += added_len; added_len = 0;
        if (interp->current != NULL) {
            PICOL_APPEND_BREAK_PICOLERR("\n    while executing\n\"");
            PICOL_APPEND_BREAK_PICOLERR(interp->current);
            PICOL_APPEND_BREAK_PICOLERR("\"");
        }
        for (cf = interp->callframe;
             cf->command != NULL && cf->parent != NULL;
             cf = cf->parent) {
            len += added_len; added_len = 0;
            PICOL_APPEND_BREAK_PICOLERR("\n    invoked from within\n\"");
            PICOL_APPEND_BREAK_PICOLERR(cf->command);
            PICOL_APPEND_BREAK_PICOLERR("\"");
        }
    } while (0);
    if (too_long) {
        /* Truncate the error message at the last complete chunk. */
        buf[len] = '\0';
        do {
            PICOL_APPEND_BREAK_PICOLERR(len > 0 ? "\n..." : "...");
        } while (0);
    }
    /* Not exactly the same as in Tcl. */
    picolSetVar2(interp, "::errorInfo", buf, 1);
    picolSetResult(interp, str);
    return PICOL_ERR;
}
#undef PICOL_APPEND_BREAK_PICOLERR
int picolErr1(picolInterp* interp, char* format, char* arg) {
    /* The format line must contain exactly one "%s" specifier. */
    char buf[PICOL_MAX_STR], truncated[PICOL_MAX_STR];
    size_t max_len;
    strncpy(truncated, arg, PICOL_MAX_STR);

    /* The two chars are for the "%s". */
    max_len = PICOL_MAX_STR - 1 - strlen(format) + 2;
    if (strlen(truncated) > max_len) {
        truncated[max_len - 3] = '.';
        truncated[max_len - 2] = '.';
        truncated[max_len - 1] = '.';
        truncated[max_len] = '\0';
    }

    PICOL_SNPRINTF(buf, sizeof(buf), format, truncated);

    return picolErr(interp, buf);
}
picolVar* picolGetVar2(picolInterp* interp, char* name, int glob) {
    picolVar* v = interp->callframe->vars;
    int global = PICOL_COLONED(name);
    if (global || glob) {
        picolCallFrame* c = interp->callframe;
        while (c->parent) {
            c = c->parent;
        }
        v = c->vars;
        if (global) {
            name += 2; /* skip the "::" */
        }
    }
#if PICOL_FEATURE_ARRAYS
    {
        char buf[PICOL_MAX_STR], buf2[PICOL_MAX_STR], *cp, *cp2;
        /* array element syntax? */
        if ((cp = strchr(name, '('))) {
            picolArray* ap;
            int found = 0;
            strncpy(buf, name, cp - name);
            buf[cp - name] = '\0';
            for (; v != NULL; v = v->next) {
                if (PICOL_EQ(v->name, buf)) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                return NULL;
            }
            ap = picolScanPtr(v->val);
            if (ap == NULL ||
                !picolValidPtrFind(interp, PICOL_PTR_ARRAY, (void*)ap)) {
                return NULL;
            }
            /* copy the key that starts after the opening paren */
            strcpy(buf2, cp +1);
            cp = strchr(buf2, ')');
            if (cp == NULL) {
                return NULL;
            }
            /* overwrite closing paren */
            *cp = '\0';
            v = picolArrGet1(ap, buf2);
            if (v == NULL) {
                if ((global || interp->callframe->parent == NULL)
                        && PICOL_EQ(buf, "env")) {
                    cp2 = getenv(buf2);
                    if (cp2 == NULL) {
                        return NULL;
                    }
                    strcpy(buf, "::env(");
                    strcat(buf, buf2);
                    strcat(buf, ")");
                    return picolArrSet1(interp, buf, cp2);
                } else {
                    return NULL;
                }
            }
            return v;
        }
    }
#endif /* PICOL_FEATURE_ARRAYS */
    for (; v != NULL; v = v->next) {
        if (PICOL_EQ(v->name, name)) {
            return v;
        }
    }
    return NULL;
}
int picolSetVar2(picolInterp* interp, char* name, char* val, int glob) {
    picolVar*       v = picolGetVar(interp, name);
    picolCallFrame* c = interp->callframe, *localc = c;
    int global = PICOL_COLONED(name);
    if (glob||global) v = picolGetGlobalVar(interp, name);

    if (v != NULL) {
        /* existing variable case */
        if (v->val != NULL) {
            /* local value */
            free(v->val);
        } else {
            return picolSetGlobalVar(interp, name, val);
        }
    } else {
        /* nonexistent variable */
#if PICOL_FEATURE_ARRAYS
        if (strchr(name, '(')) {
            if (picolArrSet1(interp, name, val) == NULL) {
                return picolErr1(interp,
                                 "can't set \"%s\": variable isn't array",
                                 name);
            } else {
                return PICOL_OK;
            }
        }
#endif
        if (glob || global) {
            if (global) name += 2;
            while (c->parent) {
                c = c->parent;
            }
        }
        v       = malloc(sizeof(*v));
        v->name = strdup(name);
        v->next = c->vars;
        c->vars = v;
        interp->callframe = localc;
    }
    v->val = (val ? strdup(val) : NULL);
    return PICOL_OK;
}
int picolSetIntVar(picolInterp* interp, char* name, int value) {
    char buf[32];
    PICOL_SNPRINTF(buf, sizeof(buf), "%d", value);
    return picolSetVar(interp, name, buf);
}
int picolGetToken(picolInterp* interp, picolParser* p) {
    int rc;
    while (1) {
        if (!p->len) {
            if (p->type != PICOL_PT_EOL && p->type != PICOL_PT_EOF) {
                p->type = PICOL_PT_EOL;
            } else {
                p->type = PICOL_PT_EOF;
            }
            return PICOL_OK;
        }
        switch(*p->p) {
        case ' ':
        case '\t':
            if (p->insidequote) {
                return picolParseString(p);
            } else {
                return picolParseSep(p);
            }
        case '\n':
        case '\r':
        case ';':
            if (p->insidequote) {
                return picolParseString(p);
            } else {
                return picolParseEol(p);
            }
        case '[':
            rc = picolParseCmd(p);
            if (rc == PICOL_ERR) {
                return picolErr(interp, "missing close-bracket");
            }
            return rc;
        case '$':
            return picolParseVar(p);
        case '#':
            if (p->type == PICOL_PT_EOL) {
                picolParseComment(p);
                continue;
            }
            /* FALLTHRU */
        default:
            return picolParseString(p);
        }
    }
}
void picolInitInterp(picolInterp* interp) {
    interp->level     = 0;
    interp->maxlevel  = PICOL_MAX_LEVEL;
    interp->callframe = calloc(1, sizeof(picolCallFrame));
    interp->result    = strdup("");
}
picolCmd* picolGetCmd(picolInterp* interp, char* name) {
    picolCmd* c;
    for (c = interp->commands; c; c = c->next) {
        if (PICOL_EQ(c->name, name)) return c;
    }
    return NULL;
}
int picolRegisterCmd(picolInterp* interp, char* name, picol_Func f, void* pd) {
    picolCmd* c = picolGetCmd(interp, name);
    if (c != NULL) {
        return picolErr1(interp, "command \"%s\" already defined", name);
    }
    c = malloc(sizeof(picolCmd));
    c->name     = strdup(name);
    c->func     = f;
    c->privdata = pd;
    c->next     = interp->commands;
    interp->commands = c;
    return PICOL_OK;
}
char* picolList(char* buf, int argc, char** argv) {
    int a;
    /* The caller is responsible for supplying a large enough buffer. */
    buf[0] = '\0';
    for (a=0; a<argc; a++) {
        PICOL_LAPPEND_X(buf, argv[a]);
    }
    return buf;
}
char* picolParseList(char* start, char* trg) {
    char* cp = start;
    int bracelevel = 0, offset = 0, quoted = 0, done;
    if (cp == NULL || *cp == '\0') {
        return NULL;
    }

    /* Skip initial whitespace. */
    while (isspace(*cp)) {
        cp++;
    }
    if (!*cp) {
        return NULL;
    }
    start = cp;

    for (done=0; 1; cp++) {
        if (*cp == '{') {
            bracelevel++;
        } else if (*cp == '}') {
            bracelevel--;
        } else if (bracelevel == 0 && *cp == '\"') {
            if (quoted) {
                done = 1;
                quoted = 0;
            } else if ((cp == start) || isspace(*cp)) {
                quoted = 1;
            }
        } else if (bracelevel == 0 &&!quoted && (isspace(*cp) || *cp == '\0')) {
            done = 1;
        }
        if (done && !quoted) {
            if (*start == '{')  {
                start++;
                offset = 1;
            }
            if (*start == '\"') {
                start++;
                offset = 1;
                cp++;
            }
            strncpy(trg, start, cp-start-offset);
            trg[cp-start-offset] = '\0';
            while (isspace(*cp)) {
                cp++;
            }
            break;
        }
        if (!*cp) break;
    }
    return cp;
}
void picolEscape(char* str) {
    char buf[PICOL_MAX_STR], *cp, *cp2;
    int ichar;
    for (cp = str, cp2 = buf; *cp; cp++) {
        if (*cp == '\\') {
            switch(*(cp+1)) {
            case 'n':
                *cp2++ = '\n';
                cp++;
                break;
            case 'r':
                *cp2++ = '\r';
                cp++;
                break;
            case 't':
                *cp2++ = '\t';
                cp++;
                break;
            case 'x':
                sscanf(cp+2, "%x", (unsigned int *)&ichar);
                *cp2++ = (char)ichar&0xFF;
                cp += 3;
                break;
            case '\\':
                *cp2++ = '\\';
                cp++;
                break;
            case '\n':
                *cp2++ = ' ';
                cp+=2;
                while (isspace(*cp)) {
                    cp++;
                }
                cp--;
                break;
            default:; /* drop backslash */
            }
        } else *cp2++ = *cp;
    }
    *cp2 = '\0';
    strcpy(str, buf);
}
size_t picolExpandLC(char* destination, char* source, size_t num) {
    /* Copy the string source to destination while substituting a single space
       for \<newline><whitespace> line continuation sequences. Return the number
       of characters copied to destination. */
    char* dest = destination;
    char* src = source;
    size_t i;
    for (i = 0; i < num; i++) {
        if ((i == 0 || *(src - 1) != '\\') &&
                *src == '\\' &&
                i < num - 1 && *(src + 1) == '\n') {
            src += 2;
            i += 2;
            if (i == num) break;
            while (isspace(*src)) {
                src++;
                i++;
            }
            *dest = ' ';
            dest++;
            if (i == num) break;
        }
        *dest = *src;
        src++;
        dest++;
    }
    return dest - destination;
}
#define PICOL_EVAL_BUF_SIZE (PICOL_MAX_STR*2)
int picolEval2(picolInterp* interp, char* t, int mode) { /*------------ EVAL! */
    /* mode==0: subst only, mode==1: full eval */
    picolParser p;
    int argc = 0, j;
    char** argv = NULL;
    char buf[PICOL_EVAL_BUF_SIZE];
    int rc = PICOL_OK;
    picolSetResult(interp, "");
    picolInitParser(&p, t);
    while (1) {
        size_t tlen;
        int prevtype = p.type;
        picolGetToken(interp, &p);
        if (p.type == PICOL_PT_EOF) {
            break;
        }
        tlen = p.end < p.start ? 0 : p.end - p.start + 1;
        t = malloc(tlen + 1);
        if (p.type == PICOL_PT_STR || p.type == PICOL_PT_VAR) {
            tlen = picolExpandLC(t, p.start, tlen);
        } else {
            memcpy(t, p.start, tlen);
        }
        t[tlen] = '\0';
        if (p.type == PICOL_PT_VAR) {
            picolVar* v = picolGetVar(interp, t);
            if (v != NULL && !v->val) {
                v = picolGetGlobalVar(interp, t);
            }
            if (v == NULL) {
                rc = picolErr1(interp,
                               "can't read \"%s\": no such variable",
                               t);
            }
            free(t);
            t = NULL;
            if (v == NULL) {
                goto err;
            }
            t = strdup(v->val);
        } else if (p.type == PICOL_PT_CMD) {
            rc = picolEval(interp, t);
            free(t);
            t = NULL;
            if (rc != PICOL_OK) {
                goto err;
            }
            t = strdup(interp->result);
        } else if (p.type == PICOL_PT_ESC) {
            if (strchr(t, '\\')) {
                picolEscape(t);
            }
        } else if (p.type == PICOL_PT_SEP) {
            prevtype = p.type;
            free(t);
            t = NULL;
            continue;
        }

        /* We have a complete command + args. Call it! */
        if (p.type == PICOL_PT_EOL) {
            picolCmd* c;
            free(t);
            t = NULL;
            if (mode == 0) {
                /* do a quasi-subst only */
                rc = picolSetResult(interp, picolList(buf, argc, argv));
                /* not an error if rc == PICOL_OK */
                goto err;
            }
            prevtype = p.type;
            if (argc) {
                int i, total_len;

                if ((c = picolGetCmd(interp, argv[0])) == NULL) {
                    if (PICOL_EQ(argv[0], "") || *argv[0]=='#') {
                        goto err;
                    }
                    if ((c = picolGetCmd(interp, "unknown"))) {
                        argv = realloc(argv, sizeof(char*)*(argc+1));
                        for (j = argc; j > 0; j--) {
                            /* copy up */
                            argv[j] = argv[j - 1];
                        }
                        argv[0] = strdup("unknown");
                        argc++;
                    } else {
                        rc = picolErr1(interp,
                                       "invalid command name \"%s\"",
                                       argv[0]);
                        goto err;
                    }
                }
                if (interp->current != NULL) {
                    free(interp->current);
                    interp->current = NULL;
                }

                total_len = 0;
                for (i = 0; i < argc; i++) {
                    int arg_len = strlen(argv[i]);
                    if (c->func == &picolCallProc &&
                        arg_len >= PICOL_MAX_STR - 1) {
                        rc = picolErr1(interp,
                                       "proc argument too long: \"%s\"",
                                       argv[i]);
                        goto err;
                    }
                    total_len += arg_len;
               }
                if (total_len >= PICOL_EVAL_BUF_SIZE - 1) {
                    rc = picolErr(interp,
                                  "script too long to parse "
                                  "(missing closing brace?)");
                    goto err;
                }

                interp->current = strdup(picolList(buf, argc, argv));

#if PICOL_FEATURE_PUTS
                if (interp->debug) {
                    fprintf(stderr, "< %d: %s\n", interp->level,
                            interp->current);
                    fflush(stderr);
                }
#endif
                rc = c->func(interp, argc, argv, c->privdata);
#if PICOL_FEATURE_PUTS
                if (interp->debug) {
                    fprintf(stderr, "> %d: {%s} -> {%s}\n", interp->level,
                            picolList(buf, argc, argv), interp->result);
                    fflush(stderr);
                }
#endif
                if (rc != PICOL_OK) {
                    goto err;
                }
            }
            /* Prepare for the next command. */
            for (j = 0; j < argc; j++) {
                free(argv[j]);
            }
            free(argv);
            argv = NULL;
            argc = 0;
            continue;
        }
        /* We have a new token. Append it to the previous or use it as a
           new arg. */
        if (prevtype == PICOL_PT_SEP || prevtype == PICOL_PT_EOL) {
            if (!p.expand) {
                argv       = realloc(argv, sizeof(char*)*(argc+1));
                argv[argc] = t;
                argc++;
                p.expand = 0;
            } else if (strlen(t)) {
                char buf2[PICOL_MAX_STR], *cp;
                PICOL_FOREACH(buf2, cp, t) {
                    argv       = realloc(argv, sizeof(char*)*(argc+1));
                    argv[argc] = strdup(buf2);
                    argc++;
                }
                free(t);
                t = NULL;
                p.expand = 0;
            }
        } else if (p.expand) {
            /* slice in the words separately */
            char buf2[PICOL_MAX_STR], *cp;
            PICOL_FOREACH(buf2, cp, t) {
                argv       = realloc(argv, sizeof(char*)*(argc+1));
                argv[argc] = strdup(buf2);
                argc++;
            }
            free(t);
            t = NULL;
            p.expand = 0;
        } else {
            /* Interpolation */
            size_t oldlen = strlen(argv[argc-1]), tlen2 = strlen(t);
            argv[argc-1]  = realloc(argv[argc-1], oldlen + tlen2 + 1);
            memcpy(argv[argc-1] + oldlen, t, tlen2);
            argv[argc-1][oldlen + tlen2] = '\0';
            free(t);
            t = NULL;
        }
        prevtype = p.type;
    }
err:
    for (j = 0; j < argc; j++) {
        free(argv[j]);
    }
    free(argv);
    return rc;
}
#undef PICOL_EVAL_BUF_SIZE
int picolCondition(picolInterp* interp, char* str) {
    if (str != NULL) {
        char substBuf[PICOL_MAX_STR], buf[PICOL_MAX_STR];
        char *argv[3], *cp, *substP;
        int a = 0, rc;

        rc = picolSubst(interp, str);
        if (rc != PICOL_OK) {
            return rc;
        }
        strcpy(substBuf, interp->result);

        /* Check whether the format suits [expr]. */
        strcpy(buf, "llength ");
        PICOL_LAPPEND(buf, interp->result);
        rc = picolEval(interp, buf);
        if (rc != PICOL_OK) {
            return rc;
        }

        /* Three elements? */
        if (PICOL_EQ(interp->result, "3")) {
            PICOL_FOREACH(buf, cp, substBuf) {
                argv[a++] = strdup(buf);
            }

            /* Is there an operator in the middle? */
            if (picolGetCmd(interp, argv[1])) {
                /* Translate to Polish :-) */
                /* E.g., {1 > 2} -> {> 1 2} */
                strcpy(buf, argv[1]);
                PICOL_LAPPEND(buf, argv[0]);
                PICOL_LAPPEND(buf, argv[2]);

                for (a = 0; a < 3; a++) {
                    free(argv[a]);
                }

                rc = picolEval(interp, buf);
                return rc;
            }
        }

        /* The expression is not a triple. */
        substP = substBuf;
        if (*substP == '!') {
            /* Allow !$x */
            strcpy(buf, "== 0 ");
            substP++;
        } else {
            strcpy(buf, "!= 0 ");
        }
        strcat(buf, substP);

        return picolEval(interp, buf);
    } else {
        return picolErr(interp, "NULL condition");
    }
}
int picolIsDirectory(char* path) {
    struct stat path_stat;
    if (stat(path, &path_stat) == 0) {
        return S_ISDIR(path_stat.st_mode);
    } else {
        /* Return -2 if the path doesn't exist and -1 on other errors. */
        return (errno == ENOENT ? -2 : -1);
    }
}
int picolIsInt(char* str) {
    char* cp = str;
    int base = 10, rem = strlen(str);

    if (rem == 0) return 0;

    /* Allow a leading minus. */
    if (*cp == '-') {
        cp++; rem--;
    }
    if (rem == 0) return 0;

    /* Parse the optional base marker. */
    if (*cp == '0' && rem >= 2) {
        char b = *(cp + 1);
        if (b == 'b' || b == 'B') {
            base = 2;
        } else if (b == 'o' || b == 'O') {
            base = 8;
        } else if (b == 'x' || b == 'X') {
            base = 16;
        }
        if (base != 10) {
            cp += 2;
        }
    }

    /* Validate the number itself. */
    for (; *cp; cp++) {
        char c = *cp;
        int n;
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
        if (c >= '0' && c <= '9') {
            n = c - '0';
        } else if (c >= 'a' && c <= 'z') {
            n = c - 'a' + 10;
        } else {
            return 0;
        }
        if (n >= base) {
            return 0;
        }
    }

    return base;
}
int picolScanInt(char* str, int base) {
    int sign = 1;
    if (*str == '-') {
        str++; sign = -1;
    }
    if (base == 10) {
        while (*str == '0') { str++; }
    } else {
        str += 2;
    }
    return sign*strtol(str, NULL, base);
}
void* picolScanPtr(char* str) {
    void* p = NULL;
    sscanf(str, "%p", &p);
    return (p && strlen(str) >= 3 ? p : NULL);
}
void picolDropCallFrame(picolInterp* interp) {
    picolCallFrame* cf = interp->callframe;
    picolVar* v, *next;
    for (v = cf->vars; v != NULL; v = next) {
        next = v->next;
        free(v->name);
        free(v->val);
        free(v);
    }
    if (cf->command != NULL) {
        free(cf->command);
    }
    interp->callframe = cf->parent;
    free(cf);
}
int picolValidPtrAdd(picolInterp* interp, int type, void* ptr) {
    picolPtr* p = interp->validptrs;
    while (p != NULL) {
        /* Adding a valid pointer should be idempotent. */
        if (p->type == type && p->ptr == ptr) {
            return 2;
        }
        p = p->next;
    }

    p = calloc(1, sizeof(picolPtr));
    if (p == NULL) return 0;
    p->type = type;
    p->ptr = ptr;
    p->next = interp->validptrs;
    interp->validptrs = p;

    return 1;
}
int picolValidPtrFind(picolInterp* interp, int type, void* ptr) {
    picolPtr* p = interp->validptrs;
    while (p != NULL) {
        if (p->type == type && p->ptr == ptr) {
            return 1;
        }
        p = p->next;
    }
    return 0;
}
int picolValidPtrRemove(picolInterp* interp, int type, void* ptr) {
    picolPtr* p = interp->validptrs, *prev = NULL;

    if (p == NULL) {
        return 0;
    }
    while (p != NULL && p->type != type && p->ptr != ptr) {
        prev = p;
        p = p->next;
    }
    if (p == NULL) {
        return 0;
    } else {
        picolPtr* nextnext = (p->next == NULL ? NULL : p->next->next);
        if (prev == NULL) {
            interp->validptrs = nextnext;
        } else {
            prev->next = nextnext;
        }
        free(p);
        return 1;
    }
}
int picolCallProc(picolInterp* interp, int argc, char** argv, void* pd) {
    char** x = pd, *alist = x[0], *body = x[1], *p = strdup(alist), *tofree;
    char buf[PICOL_MAX_STR];
    picolCallFrame* cf = calloc(1, sizeof(*cf));
    int a = 0, done = 0, errcode = PICOL_OK;
    if (cf == NULL) {
        printf("could not allocate callframe\n");
        exit(1);
    }
    cf->parent = interp->callframe;
    interp->callframe = cf;
    if (interp->level > interp->maxlevel) {
        free(p);
        picolDropCallFrame(interp);
        return picolErr(interp, "too many nested evaluations (infinite loop?)");
    }
    interp->level++;
    tofree = p;
    while (1) {
        char* start = p;
        while (!isspace(*p) && *p != '\0') p++;
        if (*p != '\0' && p == start) {
            p++;
            continue;
        }
        if (p == start) {
            break;
        }
        if (*p == '\0') {
            done=1;
        } else {
            *p = '\0';
        }
        if (PICOL_EQ(start, "args") && done) {
            picolSetVar(interp, start, picolList(buf, argc-a-1, argv+a+1));
            a = argc-1;
            break;
        }
        if (++a > argc-1) {
            goto arityerr;
        }
        picolSetVar(interp, start, argv[a]);
        p++;
        if (done) {
            break;
        }
    }
    free(tofree);
    if (a != argc-1) {
        goto arityerr;
    }
    cf->command = strdup(picolList(buf, argc, argv));
    errcode     = picolEval(interp, body);
    if (errcode == PICOL_RETURN) {
        errcode = PICOL_OK;
    }
    /* remove the called proc's callframe on success */
    picolDropCallFrame(interp);
    interp->level--;
    return errcode;
arityerr:
    /* remove the called proc's callframe on error */
    picolDropCallFrame(interp);
    interp->level--;
    return picolErr1(interp, "wrong # args for \"%s\"", argv[0]);
}
int picolWildEq(char* pat, char* str, int n) {
    /* Check if the first n characters of str match the pattern pat where
       a '?' in pat matches any character. */
    int escaped = 0;
    for (; *pat != '\0' && *str != '\0' && n != 0; pat++, str++, n--) {
        if (*pat == '\\') {
            escaped = 1;
            pat++;
        }
        if (!(*pat == *str || (!escaped && *pat == '?'))) {
            return 0;
        }
        escaped = 0;
    }
    return (n == 0 || *pat == *str || (!escaped && *pat == '?'));
}
int picolMatch(char* pat, char* str) {
    /* An incomplete implementation of [string match]. It only handles these
       common special cases: <string>, *<string>, <string>* and *<string>*
       where the string may contain '?' to match any character. Other patterns
       are rejected with an error. Escape '*' or '?' with '\' to match the
       literal character.
    */
    int pat_type = 0; /* 0 - no asterisk, 1 - left, 2 - right, 3 - both. */
    int escaped = 0, escaped_count = 0, pat_len, i;

    /*  Validate the pattern and detect its type. */
    for (i = 0; pat[i] != '\0'; i++) {
        if (pat_type > 1) {
            return -1; /* Invalid pattern: unescaped '*' in the middle. */
        }
        if (pat[i] == '*') {
            if (i == 0) {
                pat_type = 1;
            } else if (!escaped) {
                pat_type = pat_type + 2;
            }
        }
        if (!escaped && pat[i] == '\\') {
            escaped = 1;
            escaped_count++;
        } else {
            escaped = 0;
        }
    }
    pat_len = i - escaped_count;

    if (pat_type == 0) { /*   <string>   */
        return picolWildEq(pat, str, -1);
    } else if (pat_type == 1) { /*   *<string>   */
        int str_len;
        if (pat_len == 1) { /* The pattern is just "*". */
            return 1;
        }
        str_len = strlen(str);
        if (str_len < pat_len - 1) {
            return 0;
        }
        return picolWildEq(pat + 1, str + str_len - (pat_len - 1), -1);
    } else if (pat_type == 2) { /*   <string>*   */
        return picolWildEq(pat, str, pat_len - 1);
    } else { /* pat_type == 3,   *<string>*   */
        int offset = 0, res = 0, str_len = strlen(str);
        if (pat_len == 2) { /* The pattern is just "**". */
            return 1;
        }
        if (str_len < pat_len - 2) {
            return 0;
        }
        for (offset = 0; offset < str_len - (pat_len - 2) + 1; offset++) {
            res |= picolWildEq(pat + 1, str + offset, pat_len - 2);
            if (res) break;
        }
        return res;
    }
}
/* Returns 0 if the strings are equal, the index of the first differing
   character + 1 if they are not, and -1 if there was an error. */
int picolStrCompare(
    const char* str1,
    const char* str2,
    size_t num,
    int nocase
) {
    size_t i;

    if (str1 == NULL || str2 == NULL) {
        return -1;
    }

    for (i = 0; *str1 && *str2 && i < num; str1++, str2++, i++) {
        char a = *str1;
        char b = *str2;

        if (nocase) {
            a = tolower(a);
            b = tolower(b);
        }

        if (a != b) {
            return i + 1;
        }
    }

    if (i < num && ((!*str1 && *str2) || (*str1 && !*str2))) {
        return i;
    }

    return 0;
}
int picolReplace(char* str, char* from, char* to, int nocase) {
    int strLen = strlen(str);
    int fromLen = strlen(from);
    int toLen = strlen(to);

    int fromIndex = 0;
    char result[PICOL_MAX_STR] = "\0";
    char buf[PICOL_MAX_STR] = "\0";
    int resultLen = 0;
    int bufLen = 0;
    int count = 0;

    int strIndex;
    int j;

    for (strIndex = 0; strIndex < strLen; strIndex++) {
        char strC = str[strIndex];
        char fromC = from[fromIndex];
        int match = nocase ? toupper(strC) == toupper(fromC) : strC == fromC;

        if (match) {
            /* Append the current str character to buf. */
            buf[bufLen] = strC;
            bufLen++;

            fromIndex++;
        } else {
            /* Append buf to result. */
            for (j = 0; j < bufLen; j++) {
                result[resultLen + j] = buf[j];
            }
            resultLen += bufLen;

            /* Append the current str character to result. */
            result[resultLen] = strC;
            resultLen++;

            fromIndex = 0;
            bufLen = 0;
        }
        if (fromIndex == fromLen) {
            /* Append to[] to result. */

            for (j = 0; j < toLen; j++) {
                result[resultLen + j] = to[j];
                if (resultLen + j >= PICOL_MAX_STR - 1) {
                    break;
                }
            }
            resultLen += j;

            fromIndex = 0;
            bufLen = 0;
            count++;

            if (resultLen >= PICOL_MAX_STR - 1) {
                break;
            }
        }
    }

    if (resultLen < PICOL_MAX_STR - 1) {
        for (j = 0; j < bufLen; j++) {
            result[resultLen + j] = buf[j];
        }
        resultLen += bufLen;
        result[resultLen] = '\0';
    } else {
        result[PICOL_MAX_STR - 1] = '\0';
    }

    strcpy(str, result);
    return count;
}
int picolQuoteForShell(char* dest, int argc, char** argv) {
    char command[PICOL_MAX_STR] = "\0";
    int j;
    unsigned int k;
    unsigned int offset = 0;
#define PICOL_ADDCHAR(c) \
    do { \
        command[offset] = c; offset++; \
        if (offset >= sizeof(command) - 1) {return -1;} \
    } while (0)
#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS
    /* See http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/
           04/23/everyone-quotes-arguments-the-wrong-way.aspx */
    int backslashes = 0;
    int m;
    int length;
    char command_unquoted[PICOL_MAX_STR] = "\0";
    for (j = 1; j < argc; j++) {
        PICOL_ADDCHAR(' ');
        length = strlen(argv[j]);
        if (strchr(argv[j], ' ') == NULL && \
                strchr(argv[j], '\t') == NULL && \
                strchr(argv[j], '\n') == NULL && \
                strchr(argv[j], '\v') == NULL && \
                strchr(argv[j], '"') == NULL) {
            if (offset + length >= sizeof(command) - 1) {
                return -1;
            }
            strcat(command, argv[j]);
            offset += length;
        } else {
            PICOL_ADDCHAR('"');
            for (k = 0; k < length; k++) {
                backslashes = 0;
                while (argv[j][k] == '\\') {
                    backslashes++;
                    k++;
                }
                if (k == length) {
                    for (m = 0; m < backslashes * 2; m++) {
                        PICOL_ADDCHAR('\\');
                    }
                    PICOL_ADDCHAR(argv[j][k]);
                } else if (argv[j][k] == '"') {
                    for (m = 0; m < backslashes * 2 + 1; m++) {
                        PICOL_ADDCHAR('\\');
                    }
                    PICOL_ADDCHAR('"');
                } else {
                    for (m = 0; m < backslashes; m++) {
                        PICOL_ADDCHAR('\\');
                    }
                    PICOL_ADDCHAR(argv[j][k]);
                }
            }
            PICOL_ADDCHAR('"');
        }
    }
    PICOL_ADDCHAR('\0');
    memcpy(command_unquoted, command, offset);
    length = offset;
    offset = 0;
    /* Skip the first character, which is a space. */
    for (j = 1; j < length; j++) {
        PICOL_ADDCHAR('^');
        PICOL_ADDCHAR(command_unquoted[j]);
    }
    PICOL_ADDCHAR('\0');
#else
    /* Assume a POSIXy platform. */
    for (j = 1; j < argc; j++) {
        PICOL_ADDCHAR(' ');
        PICOL_ADDCHAR('\'');
        for (k = 0; k < strlen(argv[j]); k++) {
            if (argv[j][k] == '\'') {
                PICOL_ADDCHAR('\'');
                PICOL_ADDCHAR('\\');
                PICOL_ADDCHAR('\'');
                PICOL_ADDCHAR('\'');
            } else {
                PICOL_ADDCHAR(argv[j][k]);
            }
        }
        PICOL_ADDCHAR('\'');
    }
    PICOL_ADDCHAR('\0');
#endif
#undef PICOL_ADDCHAR
    memcpy(dest, command, strlen(command));
    return 0;
}
/* ------------------------------------------- Commands in alphabetical order */
PICOL_COMMAND(abs) {
    /* This is an example of how to wrap int functions. */
    int x;
    PICOL_ARITY2(argc == 2, "abs int");
    PICOL_SCAN_INT(x, argv[1]);
    return picolSetIntResult(interp, abs(x));
}
#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_UNIX || \
    PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS
#ifdef _MSC_VER
PICOL_COMMAND(after) {
    unsigned int ms;
    PICOL_ARITY2(argc == 2, "after ms");
    PICOL_SCAN_INT(ms, argv[1]);
    Sleep(ms);
    return picolSetResult(interp, "");
}
#else
PICOL_COMMAND(after) {
    unsigned int ms;
    struct timespec t, rem;
    PICOL_ARITY2(argc == 2, "after ms");
    PICOL_SCAN_INT(ms, argv[1]);
    t.tv_sec = ms/1000;
    t.tv_nsec = (ms % 1000)*1000000;
    while (nanosleep(&t, &rem) == -1) {
        if (errno != EINTR) {
            return picolErr1(interp,
                             "nanosleep() failed (%s)",
                             errno == EFAULT ? "EFAULT" : "EINVAL");
        }
        t = rem;
    }
    return picolSetResult(interp, "");
}
#endif /* _MSC_VER */
#endif /* PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_UNIX ||
          PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS */
PICOL_COMMAND(append) {
    picolVar* v;
    char buf[PICOL_MAX_STR] = "";
    int a, set_rc;
    PICOL_ARITY2(argc > 1, "append varName ?value value ...?");
    v = picolGetVar(interp, argv[1]);
    if (v != NULL) {
        strcpy(buf, v->val);
    }
    for (a = 2; a < argc; a++) {
        PICOL_APPEND(buf, argv[a]);
    }
    set_rc = picolSetVar(interp, argv[1], buf);
    if (set_rc != PICOL_OK) {
        return PICOL_OK;
    }
    return picolSetResult(interp, buf);
}
PICOL_COMMAND(apply) {
    char* procdata[2], *cp;
    char buf[PICOL_MAX_STR], buf2[PICOL_MAX_STR];
    PICOL_ARITY2(argc >= 2, "apply {argl body} ?arg ...?");
    cp = picolParseList(argv[1], buf);
    picolParseList(cp, buf2);
    procdata[0] = buf;
    procdata[1] = buf2;
    return picolCallProc(interp, argc-1, argv+1, (void*)procdata);
}
/*--------------------------------------------------------------- Array stuff */
#if PICOL_FEATURE_ARRAYS
int picolHash(char* key, int modul) {
    char* cp;
    int hash = 0;
    for (cp = key; *cp; cp++) {
        hash = (hash << 1) ^ *cp;
    }
    return hash % modul;
}
picolArray* picolArrCreate(picolInterp* interp, char* name) {
    char buf[PICOL_MAX_STR];
    picolArray* ap = calloc(1, sizeof(picolArray));
    PICOL_SNPRINTF(buf, sizeof(buf), "%p", (void*)ap);
    picolValidPtrAdd(interp, PICOL_PTR_ARRAY, (void*)ap);
    picolSetVar(interp, name, buf);
    return ap;
}
int picolArrDestroy(picolInterp* interp, char* name) {
    picolArray* ap;
    ap = picolArrFindByName(interp, name, 0, NULL);
    if (ap == NULL) {
        return -1;
    }
    return picolArrDestroy1(ap);
}
int picolArrDestroy1(picolArray* ap) {
    picolVar* v, *next;
    int i;
    for (i = 0; i < PICOL_ARR_BUCKETS; i++) {
        for (v = ap->table[i]; v != NULL; v = next) {
            next = v->next;
            free(v->name);
            free(v->val);
            free(v);
            ap->size--;
        }
        ap->table[i] = NULL;
    }
    if (ap->size != 0) {
        return 0;
    }
    free(ap);
    return 1;
}
picolArray* picolArrFindByName(picolInterp* interp, char* name, int create,
                               char* key_dest) {
    char buf[PICOL_MAX_STR], *cp;
    picolArray* ap;
    picolVar*   v;
    cp = strchr(name, '(');
    if (cp == NULL) {
        strcpy(buf, name);
    } else {
        strncpy(buf, name, cp - name);
        buf[cp - name] = '\0';
    }
    v = picolGetVar(interp, buf);
    if (v == NULL) {
        if (create) {
            ap = picolArrCreate(interp, buf);
        } else {
            return NULL;
        }
    } else {
        ap = picolScanPtr(v->val);
    }
    if (ap == NULL || !picolValidPtrFind(interp, PICOL_PTR_ARRAY, (void*)ap)) {
        return NULL;
    }
    if (key_dest != NULL) {
        strcpy(buf, cp + 1);
        cp = strchr(buf, ')');
        if (cp == NULL) {
            return NULL; /*picolErr1(interp, "bad array syntax %x", name);*/
        }
        /* overwrite closing paren */
        *cp = '\0';
        strcpy(key_dest, buf);
    }
    return ap;
}
char* picolArrGet(picolArray* ap, char* pat, char* buf, int mode) {
    int j;
    picolVar* v;
    for (j = 0; j < PICOL_ARR_BUCKETS; j++) {
        for (v = ap->table[j]; v != NULL; v = v->next) {
            if (picolMatch(pat, v->name) > 0) {
                /* mode==1: array names */
                PICOL_LAPPEND_X(buf, v->name);
                if (mode==2) {
                    /* array get */
                    PICOL_LAPPEND_X(buf, v->val);
                }
            }
        }
    }
    return buf;
}
picolVar* picolArrGet1(picolArray* ap, char* key) {
    int hash = picolHash(key, PICOL_ARR_BUCKETS), found = 0;
    picolVar* pos = ap->table[hash], *v;
    for (v = pos; v != NULL; v = v->next) {
        if (PICOL_EQ(v->name, key)) {
            found = 1;
            break;
        }
    }
    return (found ? v : NULL);
}
int picolArrUnset(picolArray* ap, char* key) {
    int hash = picolHash(key, PICOL_ARR_BUCKETS);
    picolVar* v = ap->table[hash], *prev = NULL;

    for (v = ap->table[hash]; v != NULL; v = v->next) {
        if (v == NULL || PICOL_EQ(v->name, key)) {
            break;
        }
        prev = v;
    }

    if (v == NULL) {
        return 0;
    } else {
        if (prev == NULL) {
            ap->table[hash] = v->next;
        } else {
            prev->next = v->next;
        }
        ap->size--;
        free(v->name);
        free(v->val);
        free(v);
    }

    return 1;
}
int picolArrUnset1(picolInterp* interp, char* name) {
    char buf[PICOL_MAX_STR];
    picolArray* ap = picolArrFindByName(interp, name, 0, buf);
    if (ap == NULL) {
        return -1;
    }
    return picolArrUnset(ap, buf);
}
picolVar* picolArrSet(picolArray* ap, char* key, char* value) {
    int hash = picolHash(key, PICOL_ARR_BUCKETS);
    picolVar* v = NULL;

    for (v = ap->table[hash]; v != NULL && !PICOL_EQ(v->name, key); v = v->next);

    if (v == NULL) {
        /* Create a new variable. */
        v       = malloc(sizeof(*v));
        v->name = strdup(key);
        v->val  = strdup(value);
        v->next = ap->table[hash];
        ap->table[hash] = v;
        ap->size++;
    } else {
        /* Replace the value for an existing variable. */
        free(v->val);
        v->val = strdup(value);
    }

    return v;
}
picolVar* picolArrSet1(picolInterp* interp, char* name, char* value) {
    char buf[PICOL_MAX_STR];
    picolArray* ap = picolArrFindByName(interp, name, 1, buf);
    if (ap == NULL) {
        return NULL;
    }
    return picolArrSet(ap, buf, value);
}
char* picolArrStat(picolArray* ap, char* buf, size_t buf_size) {
    int a, buckets=0, j, count[11], depth;
    picolVar* v;
    char tmp[128];
    for (j = 0; j < 11; j++) {
        count[j] = 0;
    }
    for (a = 0; a < PICOL_ARR_BUCKETS; a++) {
        depth = 0;
        if ((v = ap->table[a])) {
            buckets++;
            depth = 1;
            while ((v = v->next)) {
                depth++;
            }
            if (depth > 10) {
                depth = 10;
            }
        }
        count[depth]++;
    }
    PICOL_SNPRINTF(buf,
                   buf_size,
                   "%d entries in table, %d buckets",
                   ap->size,
                   buckets);
    for (j=0; j<10; j++) {
        PICOL_SNPRINTF(tmp,
                       sizeof(tmp),
                       "\nnumber of buckets with %d entries: %d",
                       j,
                       count[j]);
        strcat(buf, tmp);
    }
    PICOL_SNPRINTF(tmp,
                   sizeof(tmp),
                   "\nnumber of buckets with 10 or more entries: %d",
                   count[10]);
    strcat(buf, tmp);
    return buf;
}
PICOL_COMMAND(array) {
    picolVar*   v;
    picolArray* ap = NULL;
    char buf[PICOL_MAX_STR] = "", buf2[PICOL_MAX_STR], *cp;
    /* default: array size */
    int mode = 0;
    int valid = 1;
    PICOL_ARITY2(
        argc > 2,
        "array exists|get|names|set|size|statistics arrayName ?arg ...?"
    );
    v = picolGetVar(interp, argv[2]);
    if (v != NULL) {
        PICOL_SCAN_PTR(ap, v->val);
        valid = picolValidPtrFind(interp, PICOL_PTR_ARRAY, (void*)ap);
    }
    if (PICOL_SUBCMD("exists")) {
        picolSetBoolResult(interp, (v != NULL) && valid);
    }
    else if (PICOL_SUBCMD("get") ||
             PICOL_SUBCMD("names") ||
             PICOL_SUBCMD("size")) {
        char* pat = "*";
        if (v == NULL || !valid) {
            if (PICOL_SUBCMD("get") || PICOL_SUBCMD("names")) {
                return picolSetResult(interp, "");
            } else { /* PICOL_SUBCMD("size") */
                return picolSetIntResult(interp, 0);
            }
        }
        if (PICOL_SUBCMD("size")) {
            return picolSetIntResult(interp, ap->size);
        }
        if (argc==4) {
            pat = argv[3];
        }
        if (PICOL_SUBCMD("names")) {
            mode = 1;
        } else if (PICOL_SUBCMD("get")) {
            mode = 2;
        } else if (argc != 3) {
            return picolErr(interp, "usage: array get|names|size a");
        }
        picolSetResult(interp, picolArrGet(ap, pat, buf, mode));
    } else if (PICOL_SUBCMD("set")) {
        PICOL_ARITY2(argc == 4, "array set arrayName list");
        if (v == NULL) {
            ap = picolArrCreate(interp, argv[2]);
        }
        PICOL_FOREACH(buf, cp, argv[3]) {
            cp = picolParseList(cp, buf2);
            if (!cp) {
                return picolErr(interp,
                                "list must have an even number of elements");
            }
            /* Wait until here to check if the array is valid in order to
               generate the same error message as Tcl 8.6. */
            if (!valid) {
                int ret = PICOL_SNPRINTF(
                    buf2,
                    sizeof(buf2),
                    "%s(%s)",
                    argv[2],
                    buf
                );

                if (ret < 0) {
                    return picolErr1(
                        interp,
                        "can't set \"%s...\": variable isn't array",
                        buf2
                    );
                }
                return picolErr1(
                    interp,
                    "can't set \"%s\": variable isn't array",
                    buf2
                );
            }
            picolArrSet(ap, buf, buf2);
        }
    } else if (PICOL_SUBCMD("statistics")) {
        PICOL_ARITY2(argc == 3, "array statistics arrname");
        if (v == NULL || !valid) {
            return picolErr1(interp, "\"%s\" isn't an array", argv[2]);
        }
        picolSetResult(interp, picolArrStat(ap, buf, sizeof(buf)));
    } else {
        return picolErr1(interp,
            "bad subcommand \"%s\": must be exists, get, set, size, or names",
            argv[1]);
    }
    return PICOL_OK;
}
#endif /* PICOL_FEATURE_ARRAYS */
PICOL_COMMAND(break)    {
    PICOL_ARITY(argc == 1);
    return PICOL_BREAK;
}
PICOL_COMMAND(bitwise_not) {
    int res;
    PICOL_ARITY2(argc == 2, "~ number");
    PICOL_SCAN_INT(res, argv[1]);
    return picolSetIntResult(interp, ~res);
}
char* picolConcat(char* buf, int argc, char** argv) {
    int a;
    /* The caller is responsible for supplying a large enough buffer. */
    buf[0] = '\0';
    for (a = 1; a < argc; a++) {
        strcat(buf, argv[a]);
        if (*argv[a] && a < argc-1) {
            strcat(buf, " ");
        }
    }
    return buf;
}
PICOL_COMMAND(concat) {
    char buf[PICOL_MAX_STR];
    PICOL_ARITY2(argc > 0, "concat ?arg...?");
    return picolSetResult(interp, picolConcat(buf, argc, argv));
}
PICOL_COMMAND(continue) {
    PICOL_ARITY(argc == 1);
    return PICOL_CONTINUE;
}
PICOL_COMMAND(catch) {
    int rc, set_rc;
    PICOL_ARITY2(argc==2 || argc==3, "catch command ?varName?");
    rc = picolEval(interp, argv[1]);
    if (argc == 3) {
        set_rc = picolSetVar(interp, argv[2], interp->result);
        if (set_rc != PICOL_OK) {
            return PICOL_OK;
        }
    }
    return picolSetIntResult(interp, rc);
}
#if PICOL_FEATURE_IO
PICOL_COMMAND(cd) {
    PICOL_ARITY2(argc == 2, "cd dirName");
    if (chdir(argv[1])) {
        return PICOL_ERR;
    }
    return PICOL_OK;
}
#endif
PICOL_COMMAND(clock) {
    time_t t;
    PICOL_ARITY2(argc > 1, "clock clicks|format|seconds ?arg..?");
    if (PICOL_SUBCMD("clicks")) {
        picolSetIntResult(interp, clock());
    } else if (PICOL_SUBCMD("format")) {
        if (argc==3 || (argc==5 && PICOL_EQ(argv[3], "-format"))) {
            PICOL_SCAN_INT(t, argv[2]);
            char buf[128], *cp;
            struct tm* mytm = localtime(&t);
            if (argc==3) {
                cp = "%a %b %d %H:%M:%S %Y";
            } else {
                cp = argv[4];
            }
            strftime(buf, sizeof(buf), cp, mytm);
            picolSetResult(interp, buf);
        } else {
            return picolErr(interp,
                            "usage: clock format clockval ?-format string?");
        }
    } else if (PICOL_SUBCMD("seconds")) {
        PICOL_ARITY2(argc == 2, "clock seconds");
        picolSetIntResult(interp, (int)time(&t));
    } else {
        return picolErr(interp, "usage: clock clicks|format|seconds ..");
    }
    return PICOL_OK;
}
PICOL_COMMAND(debug) {
    int enable;
    PICOL_ARITY2(argc == 1 || argc == 2, "debug ?enable?");
    if (argc == 2) {
        PICOL_SCAN_INT(enable, argv[1]);
        interp->debug = enable;
    }
    return picolSetIntResult(interp, interp->debug);
}
PICOL_COMMAND(eval) {
    char buf[PICOL_MAX_STR];
    PICOL_ARITY2(argc >= 2, "eval arg ?arg ...?");
    return picolEval(interp, picolConcat(buf, argc, argv));
}
int picol_EqNe(picolInterp* interp, int argc, char** argv, void* pd) {
    int res;
    PICOL_ARITY2(argc == 3, "eq|ne str1 str2");
    res = PICOL_EQ(argv[1], argv[2]);
    return picolSetBoolResult(interp, PICOL_EQ(argv[0], "ne") ? !res : res);
}
PICOL_COMMAND(error) {
    PICOL_ARITY2(argc == 2, "error message");
    return picolErr(interp, argv[1]);
}
#if PICOL_FEATURE_IO
PICOL_COMMAND(exec) {
    /* This is far from the real thing, but it may be useful. */
    char command[PICOL_MAX_STR] = "\0";
    char buf[256] = "\0";
    char output[PICOL_MAX_STR] = "\0";
    FILE* fd;
    int length;
    int status;
    int too_long = 0;

    if (PICOL_EQ(argv[0], "rawexec")) {
        int i;
        for (i = 1; i < argc; i++) {
            PICOL_APPEND(command, argv[i]);
            if (i < argc - 1) {
                PICOL_APPEND(command, " ");
            }
        }
    } else { /* exec */
        if (picolQuoteForShell(command, argc, argv) == -1) {
            return picolErr(interp, PICOL_ERROR_TOO_LONG);
        }
    }

    fd = PICOL_POPEN(command, "r");
    if (fd == NULL) {
        return picolErr1(interp, "couldn't execute command \"%s\"", command);
    }

    while (fgets(buf, 256, fd)) {
        PICOL_APPEND_BREAK(output, buf, too_long);
    }
    status = PICOL_PCLOSE(fd);

    if (too_long) {
        return picolErr(interp, PICOL_ERROR_TOO_LONG);
    }

    length = strlen(output);
    if (output[length - 1] == '\r') {
        length--;
    }
    if (output[length - 1] == '\n') {
        length--;
    }
    output[length] = '\0';

    picolSetResult(interp, output);
    return status == 0 ? PICOL_OK : PICOL_ERR;
}
#endif
#if PICOL_FEATURE_IO
PICOL_COMMAND(exit) {
    int rc = 0;
    PICOL_ARITY2(argc <= 2, "exit ?returnCode?");
    if (argc==2) {
        rc = atoi(argv[1]) & 0xFF;
    }
    exit(rc);
}
#endif
PICOL_COMMAND(expr) {
    /* Only a simple case is supported: two or more operands with the same
    operator between them. */
    char buf[PICOL_MAX_STR] = "";
    int a;
    PICOL_ARITY2((argc%2)==0, "expr int1 op int2 ...");
    if (argc==2) {
        if (strchr(argv[1], ' ')) { /* braced expression - expand it */
            strcpy(buf, "expr ");
            PICOL_APPEND(buf, argv[1]);
            return picolEval(interp, buf);
        } else {
            return picolSetResult(interp, argv[1]); /* single scalar */
        }
    }
    PICOL_APPEND(buf, argv[2]); /* operator first - Polish notation */
    PICOL_LAPPEND(buf, argv[1]); /* {a + b + c} -> {+ a b c} */
    for (a = 3; a < argc; a += 2) {
        if (a < argc-1 && !PICOL_EQ(argv[a+1], argv[2])) {
            return picolErr(interp, "need equal operators");
        }
        PICOL_LAPPEND(buf, argv[a]);
    }
    return picolEval(interp, buf);
}
PICOL_COMMAND(file) {
    char buf[PICOL_MAX_STR] = "\0", *cp;
    int a;
    PICOL_ARITY2(argc >= 3, "file option ?arg ...?");
    if (PICOL_SUBCMD("dirname")) {
        if (argv[2][0] == '\0') {
            cp = ".";
        } else {
            char* trailing = picolStrFirstTrailing(argv[2], '/');
            if (trailing != NULL) *trailing = '\0';

            cp = strrchr(argv[2], '/');

            if (cp == NULL) {
                if (trailing == NULL) {
                    cp = "";
                } else {
                    cp = "/";
                }
            } else {
                *cp = '\0';

                trailing = picolStrFirstTrailing(argv[2], '/');
                if (trailing != NULL) *trailing = '\0';

                cp = argv[2];
            }
        }

        picolSetResult(interp, cp);
#if PICOL_FEATURE_IO
    } else if (PICOL_SUBCMD("delete")) {
        int is_dir = picolIsDirectory(argv[2]);
        int del_result = 0;
        if (is_dir < 0) {
            if (is_dir == -2) {
                /* Tcl 8.x and Jim Tcl do not throw an error if the file
                   to delete doesn't exist. */
                picolSetResult(interp, "");
                return 0;
            } else {
                return picolErr1(interp, "error deleting \"%s\"", argv[2]);
            }
        }
        if (is_dir) {
            del_result = rmdir(argv[2]);
        } else {
            del_result = unlink(argv[2]);
        }
        if (del_result == 0) {
            picolSetResult(interp, "");
            return 0;
        } else {
            return picolErr1(interp, "error deleting \"%s\"", argv[2]);
        }
    } else if (PICOL_SUBCMD("exists") || PICOL_SUBCMD("size")) {
        FILE* fp = NULL;
        fp = fopen(argv[2], "r");
        if (PICOL_SUBCMD("size")) {
            if (fp == NULL) {
                return picolErr1(interp, "could not open \"%s\"", argv[2]);
            }
            fseek(fp, 0, 2);
            picolSetIntResult(interp, ftell(fp));
        } else {
            picolSetBoolResult(interp, fp);
        }
        if (fp != NULL) {
            fclose(fp);
        }
    } else if (PICOL_SUBCMD("isdir") ||
               PICOL_SUBCMD("isdirectory") ||
               PICOL_SUBCMD("isfile")) {
        int result = picolIsDirectory(argv[2]);
        if (result < 0) {
            picolSetBoolResult(interp, 0);
        } else {
            if (PICOL_SUBCMD("isfile")) {
                result = !result;
            }
            picolSetBoolResult(interp, result);
        }
#endif /* PICOL_FEATURE_IO */
    } else if (PICOL_SUBCMD("join")) {
        strcpy(buf, argv[2]);
        for (a=3; a<argc; a++) {
            if (PICOL_EQ(argv[a], "")) {
                continue;
            }
            if (picolMatch("/*", argv[a]) || picolMatch("?:/*", argv[a])) {
                strcpy(buf, argv[a]);
            } else {
                if (!PICOL_EQ(buf, "") && !picolMatch("*/", buf)) {
                    PICOL_APPEND(buf, "/");
                }
                PICOL_APPEND(buf, argv[a]);
            }
        }
        picolSetResult(interp, buf);
    } else if (PICOL_SUBCMD("split")) {
        char fragment[PICOL_MAX_STR];
        char* start = argv[2];
        char head = 1;

        if (*start == '/') {
            PICOL_APPEND(buf, "/");
            while (*start == '/') start++;

            head = 0;
        }

        while ((cp = strchr(start, '/'))) {
            memcpy(fragment, start, cp - start);
            fragment[cp - start] = '\0';

            if (!head && fragment[0] == '~') {
                PICOL_LAPPEND(buf, "./");
                PICOL_APPEND(buf, fragment);
            } else {
                PICOL_LAPPEND(buf, fragment);
            }

            start = cp + 1;
            while (*start == '/') {
                start++;
            }

            head = 0;
        }

        if (strlen(start) > 0) {
            if (!head && start[0] == '~') {
                PICOL_LAPPEND(buf, "./");
                PICOL_APPEND(buf, start);
            } else {
                PICOL_LAPPEND(buf, start);
            }
        }

        picolSetResult(interp, buf);
    } else if (PICOL_SUBCMD("tail")) {
        char* trailing = picolStrFirstTrailing(argv[2], '/');

        if (trailing != NULL) *trailing = '\0';

        cp = strrchr(argv[2], '/');

        if (cp == NULL) {
            cp = argv[2];
        } else {
            cp++;
        }

        picolSetResult(interp, cp);
    } else {
        return picolErr(interp,
#if PICOL_FEATURE_IO
        "usage: file delete|dirname|exists|isdirectory|isfile|size|split|tail "
        "filename"
#else
        "usage: file dirname|split|tail filename"
#endif
        );
    }
    return PICOL_OK;
}
#if PICOL_FEATURE_IO
int picolFileUtil(picolInterp* interp, int argc, char** argv, void* pd) {
    FILE* fp = NULL;
    PICOL_ARITY(argc == 2 || (PICOL_EQ(argv[0], "seek") && argc == 3));
    /* The command may be:
       - close channelId
       - eof   channelId
       - flush channelId
       - seek  channelId [offset]
       - tell  channelId
     */
    PICOL_SCAN_PTR(fp, argv[1]);
    if (!picolValidPtrFind(interp, PICOL_PTR_CHAN, (void*) fp)) {
        return picolErr1(interp, "can not find channel named \"%s\"", argv[1]);
    }
    if (PICOL_EQ(argv[0], "close")) {
        fclose(fp);
        if (!picolValidPtrRemove(interp, PICOL_PTR_CHAN, (void*) fp)) {
            return picolErr1(interp,
                             "can not remove \"%s\" from valid channel "
                             "pointer list",
                             argv[1]);
        }
    } else if (PICOL_EQ(argv[0], "eof")) {
        picolSetBoolResult(interp, feof(fp));
    } else if (PICOL_EQ(argv[0], "flush")) {
        fflush(fp);
    } else if (PICOL_EQ(argv[0], "seek") && argc==3) {
        int offset = 0;
        PICOL_SCAN_INT(offset, argv[2]);
        fseek(fp, offset, SEEK_SET);
    } else if (PICOL_EQ(argv[0], "tell")) {
        picolSetIntResult(interp, ftell(fp));
    } else {
        return picolErr(interp, "bad use of picolFileUtil()");
    }
    return PICOL_OK;
}
#endif /* PICOL_FEATURE_IO */
PICOL_COMMAND(for) {
    int rc;
    PICOL_ARITY2(argc == 5, "for start test next command");
    if ((rc = picolEval(interp, argv[1])) != PICOL_OK) return rc; /* init */
    while (1) {
        rc = picolEval(interp, argv[4]);            /* body */
        if (rc == PICOL_BREAK) {
            return PICOL_OK;
        }
        if (rc == PICOL_ERR) {
            return rc;
        }
        rc = picolEval(interp, argv[3]);            /* step */
        if (rc != PICOL_OK) {
            return rc;
        }
        rc = picolCondition(interp, argv[2]);       /* condition */
        if (rc != PICOL_OK) {
            return rc;
        }
        if (atoi(interp->result)==0) {
            return picolSetResult(interp, "");
        }
    }
}
int picolLmap(picolInterp* interp, char* vars, char* list, char* body,
              int accumulate) {
    /* Only iterating over a single list is currently supported. */
    char buf[PICOL_MAX_STR] = "", buf2[PICOL_MAX_STR];
    char result[PICOL_MAX_STR] = "";
    char* cp, *varp;
    int rc, set_rc, done = 0;
    if (*list == '\0') {
        return PICOL_OK; /* empty data list */
    }
    varp = picolParseList(vars, buf2);
    cp   = picolParseList(list, buf);
    while (cp || varp) {
        set_rc = picolSetVar(interp, buf2, buf);
        if (set_rc != PICOL_OK) {
            return set_rc;
        }
        varp = picolParseList(varp, buf2);
        if (varp == NULL) {                        /* end of var list reached */
            rc = picolEval(interp, body);
            if (rc == PICOL_ERR) {
                return rc;
            } else if (rc == PICOL_BREAK) {
                break;
            } else { /* rc == PICOL_OK || rc == PICOL_CONTINUE */
                if (accumulate && rc != PICOL_CONTINUE) {
                    PICOL_LAPPEND(result, interp->result);
                }
                varp = picolParseList(vars, buf2); /* cycle back to start */
            }
            done = 1;
        } else {
            done = 0;
        }
        if (cp != NULL) {
            cp = picolParseList(cp, buf);
        }
        if (cp == NULL && done) {
            break;
        }
        if (cp == NULL) {
            strcpy(buf, ""); /* empty string when data list exhausted */
        }
    }
    return picolSetResult(interp, result);
}
PICOL_COMMAND(foreach) {
    PICOL_ARITY2(argc == 4, "foreach varList list command");
    return picolLmap(interp, argv[1], argv[2], argv[3], 0);
}
PICOL_COMMAND(format) {
    /* Limited to a single integer or string argument so far. */
    int value;
    unsigned int j = 0;
    int length = 0;
    char buf[PICOL_MAX_STR];
    PICOL_ARITY2(argc == 2 || argc == 3, "format formatString ?arg?");
    if (argc == 2) {
        return picolSetResult(interp, argv[1]); /* identity */
    }
    length = strlen(argv[1]);
    if (length == 0) {
        return picolSetResult(interp, "");
    }
    if (argv[1][0] != '%') {
        return picolErr1(interp, "bad format string \"%s\"", argv[1]);
    }
    for (j = 1; j < strlen(argv[1]) - 1; j++) {
        switch (argv[1][j]) {
        case '#':
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case ' ':
        case '+':
        case '\'':
            break;
        default:
            return picolErr1(interp, "bad format string \"%s\"", argv[1]);
        }
    }
    switch (argv[1][j]) {
    case '%':
        return picolSetResult(interp, "%");
    case 'd': case 'i': case 'o': case 'u': case 'x': case 'X':
    case 'c':
        PICOL_SCAN_INT(value, argv[2]);
        return picolSetFmtResult(interp, argv[1], value);
    case 's':
        PICOL_SNPRINTF(buf, sizeof(buf), argv[1], argv[2]);
        return picolSetResult(interp, buf);
    default:
        return picolErr1(interp, "bad format string \"%s\"", argv[1]);
    }
}
#if PICOL_FEATURE_IO
PICOL_COMMAND(gets) {
    char buf[PICOL_MAX_STR], *getsrc;
    FILE* fp = stdin;
    PICOL_ARITY2(argc == 2 || argc == 3, "gets channelId ?varName?");
    picolSetResult(interp, "-1");
    if (!PICOL_EQ(argv[1], "stdin")) {
        PICOL_SCAN_PTR(fp, argv[1]);
        if (!picolValidPtrFind(interp, PICOL_PTR_CHAN, (void*)fp)) {
            return picolErr1(interp,
                             "can not find channel named \"%s\"",
                             argv[1]);
        }
    }
    if (!feof(fp)) {
        getsrc = fgets(buf, sizeof(buf), fp);
        if (feof(fp)) {
            buf[0] = '\0';
        } else {
            buf[strlen(buf) - 1] = '\0'; /* chomp newline */
        }
        if (argc == 2) {
            picolSetResult(interp, buf);
        } else if (getsrc) {
            int set_rc;
            set_rc = picolSetVar(interp, argv[2], buf);
            if (set_rc != PICOL_OK) {
                return PICOL_OK;
            }
            picolSetIntResult(interp, strlen(buf));
        }
    }
    return PICOL_OK;
}
#endif
#if PICOL_FEATURE_GLOB
PICOL_COMMAND(glob) {
    /* implicit -nocomplain. */
    char buf[PICOL_MAX_STR] = "\0";
    char file_path[PICOL_MAX_STR] = "\0";
    char old_wd[PICOL_MAX_STR] = "\0";
    char* new_wd = NULL;
    char* pattern;
    int append_slash = 0;
    glob_t pglob;
    size_t j;

    PICOL_ARITY2(argc == 2 || argc == 4, "glob ?-directory directory? pattern");
    if (argc == 2) {
        pattern = argv[1];
    } else {
        if (!PICOL_EQ(argv[1], "-directory") && !PICOL_EQ(argv[1], "-dir")) {
            return picolErr1(interp,
                             "bad option \"%s\": must be -directory or -dir",
                             argv[1]);
        }
        PICOL_GETCWD(old_wd, PICOL_MAX_STR);
        new_wd = argv[2];
        pattern = argv[3];
        if (chdir(new_wd)) {
            return picolErr1(interp, "can't change directory to \"%s\"",
                             new_wd);
        }
        append_slash = new_wd[strlen(new_wd) - 1] != '/';
    }

    glob(pattern, 0, NULL, &pglob);
    for (j = 0; j < pglob.gl_pathc; j++) {
        file_path[0] = '\0';
        if (argc == 4) {
            PICOL_APPEND(file_path, new_wd);
            if (append_slash) {
                PICOL_APPEND(file_path, "/");
            }
        }
        PICOL_APPEND(file_path, pglob.gl_pathv[j]);
        PICOL_LAPPEND(buf, file_path);
    }
    globfree(&pglob);
    /* The following two lines fix a result corruption in MinGW 20130722. */
    pglob.gl_pathc = 0;
    pglob.gl_pathv = NULL;

    if (argc == 4) {
        if (chdir(old_wd)) {
            return picolErr1(interp, "can't change directory to \"%s\"",
                             old_wd);
        }
    }

    picolSetResult(interp, buf);
    return PICOL_OK;
}
#endif /* PICOL_FEATURE_GLOB */
PICOL_COMMAND(global) {
    PICOL_ARITY2(argc > 1, "global varName ?varName ...?");
    if (interp->level > 0) {
        int a, set_rc;
        for (a = 1; a < argc; a++) {
            picolVar* v = picolGetVar(interp, argv[a]);
            if (v != NULL) {
                return picolErr1(interp, "variable \"%s\" already exists",
                                 argv[a]);
            }
            set_rc = picolSetVar(interp, argv[a], NULL);
            if (set_rc != PICOL_OK) {
                return PICOL_OK;
            }
        }
    }
    return PICOL_OK;
}
PICOL_COMMAND(if) {
    int rc, i;
    int last = argc - 1;

    /* Verify the command syntax. */
    PICOL_ARITY2(
        argc >= 3,
        "if expr1 body1 ?elseif expr2 body2 ...? ?else bodyN?"
    );
    for (i = 3; i < argc; i += 3) {
        char *no_script_msg = "wrong # args: no script following "
                              "\"%s\" argument";
        if (PICOL_EQ(argv[i], "elseif")) {
            if (i == last) {
                return picolErr1(interp,
                                 "wrong # args: no expression after "
                                 "\"%s\" argument",
                                 argv[i]);
            }
            if (i + 1 == last) {
                return picolErr1(interp, no_script_msg, argv[i + 1]);
            }
        } else if (PICOL_EQ(argv[i], "else")) {
            if (i == last) {
                return picolErr1(interp, no_script_msg, argv[i]);
            }
            if (i + 1 != last) {
                return picolErr(interp,
                                "wrong # args: extra words after "
                                "\"else\" clause in \"if\" command");
            }
        } else {
            return picolErr(interp, "expected \"elseif\" or \"else\"");
        }
    }

    /* Evaluate the conditional. */
    if ((rc = picolCondition(interp, argv[1])) != PICOL_OK) {
        return rc;
    }
    if (atoi(interp->result)) {
        return picolEval(interp, argv[2]);
    } else {
        for (i = 3; i < argc; i += 3) {
            if (PICOL_EQ(argv[i], "elseif")) {
                if ((rc = picolCondition(interp, argv[i + 1])) != PICOL_OK) {
                    return rc;
                }
                if (atoi(interp->result)) {
                    return picolEval(interp, argv[i + 2]);
                }
             } else { /* argv[i] is "else" */
                return picolEval(interp, argv[i + 1]);
            }
        }
    }

    return picolSetResult(interp, "");
}
int picol_InNi(picolInterp* interp, int argc, char** argv, void* pd) {
    char buf[PICOL_MAX_STR], *cp;
    int in = PICOL_EQ(argv[0], "in");
    PICOL_ARITY2(argc == 3, "in|ni element list");
    PICOL_FOREACH(buf, cp, argv[2])
    if (PICOL_EQ(buf, argv[1])) {
        return picolSetBoolResult(interp, in);
    }
    return picolSetBoolResult(interp, !in);
}
PICOL_COMMAND(incr) {
    int value = 0, increment = 1;
    picolVar* v;
    PICOL_ARITY2(argc == 2 || argc == 3, "incr varName ?increment?");
    v = picolGetVar(interp, argv[1]);
    if (v && !v->val) {
        v = picolGetGlobalVar(interp, argv[1]);
    }
    if (v != NULL) {
        PICOL_SCAN_INT(value, v->val);       /* creates if nonexistent */
    }
    if (argc == 3) {
        PICOL_SCAN_INT(increment, argv[2]);
    }
    picolSetIntVar(interp, argv[1], value += increment);
    return picolSetIntResult(interp, value);
}
PICOL_COMMAND(info) {
    char buf[PICOL_MAX_STR] = "", *pat = "*";
    picolCmd* c = interp->commands;
    int procs;
    PICOL_ARITY2(argc == 2 || argc == 3,
            "info args|body|commands|exists|globals|level|patchlevel|procs|"
            "script|vars");
    procs = PICOL_SUBCMD("procs");
    if (argc == 3) {
        pat = argv[2];
    }
    if (PICOL_SUBCMD("vars") || PICOL_SUBCMD("globals")) {
        picolCallFrame* cf = interp->callframe;
        picolVar*       v;
        if (PICOL_SUBCMD("globals")) {
            while (cf->parent) cf = cf->parent;
        }
        for (v = cf->vars; v; v = v->next) {
            if (picolMatch(pat, v->name) > 0) {
                PICOL_LAPPEND(buf, v->name);
            }
        }
        picolSetResult(interp, buf);
    } else if (PICOL_SUBCMD("args") || PICOL_SUBCMD("body")) {
        if (argc==2) {
            return picolErr1(interp, "usage: info %s procname", argv[1]);
        }
        for (; c; c = c->next) {
            if (PICOL_EQ(c->name, argv[2])) {
                char** privdata = c->privdata;
                if (privdata != NULL) {
                    return picolSetResult(
                        interp,
                        privdata[(PICOL_EQ(argv[1], "args") ? 0 : 1)]
                    );
                } else {
                    return picolErr1(interp, "\"%s\" isn't a procedure",
                                     c->name);
                }
            }
        }
    } else if (PICOL_SUBCMD("commands") || procs) {
        for (; c; c = c->next)
            if ((!procs||c->privdata) && (picolMatch(pat, c->name) > 0)) {
                PICOL_LAPPEND(buf, c->name);
            }
        picolSetResult(interp, buf);
    } else if (PICOL_SUBCMD("exists")) {
        if (argc != 3) {
            return picolErr(interp, "usage: info exists varName");
        }
        picolSetBoolResult(interp, picolGetVar(interp, argv[2]));
    } else if (PICOL_SUBCMD("level")) {
        if (argc==2) {
            picolSetIntResult(interp, interp->level);
        } else if (argc==3) {
            int level;
            PICOL_SCAN_INT(level, argv[2]);
            if (level==0) {
                picolSetResult(interp, interp->callframe->command);
            } else {
                return picolErr1(interp, "unsupported level %s", argv[2]);
            }
        }
    } else if (PICOL_SUBCMD("patchlevel") || PICOL_SUBCMD("pa")) {
        picolSetResult(interp, PICOL_PATCHLEVEL);
    } else if (PICOL_SUBCMD("script")) {
        picolVar* v = picolGetVar(interp, PICOL_INFO_SCRIPT_VAR);
        if (v != NULL) {
            picolSetResult(interp, v->val);
        }
    } else {
        return picolErr1(interp,
                         "bad option \"%s\": must be args, body, commands, "
                         "exists, globals, level, patchlevel, procs, script, "
                         "or vars",
                         argv[1]);
    }
    return PICOL_OK;
}
#if PICOL_FEATURE_INTERP
PICOL_COMMAND(interp) {
    picolInterp* src = interp, *trg = interp;
    if (argc < 2) {
        return picolErr(interp, "usage: interp alias|create|eval ...");
    } else if (PICOL_SUBCMD("alias")) {
        picolCmd* c = NULL;
        PICOL_ARITY2(
            argc == 6,
            "interp alias slavePath slaveCmd masterPath masterCmd"
        );
        if (!PICOL_EQ(argv[2], "")) {
            PICOL_SCAN_PTR(trg, argv[2]);
            if (!picolValidPtrFind(interp, PICOL_PTR_INTERP, (void*)trg)) {
                return picolErr1(interp,
                                 "could not find interpreter \"%s\"",
                                 argv[2]);
            }
        }
        if (!PICOL_EQ(argv[4], "")) {
            PICOL_SCAN_PTR(src, argv[4]);
            if (!picolValidPtrFind(interp, PICOL_PTR_INTERP, (void*)src)) {
                return picolErr1(interp,
                                 "could not find interpreter \"%s\"",
                                 argv[4]);
            }
        }
        c = picolGetCmd(src, argv[5]);
        if (c == NULL) {
            return picolErr(interp, "can only alias existing commands");
        }
        picolRegisterCmd(trg, argv[3], c->func, c->privdata);
        return picolSetResult(interp, argv[3]);
    } else if (PICOL_SUBCMD("create")) {
        char buf[32];
        PICOL_ARITY(argc == 2);
        trg = picolCreateInterp();
        PICOL_SNPRINTF(buf, sizeof(buf), "%p", (void*)trg);
        picolValidPtrAdd(interp, PICOL_PTR_INTERP, (void*)trg);
        return picolSetResult(interp, buf);
    } else if (PICOL_SUBCMD("eval")) {
        int rc;
        PICOL_ARITY(argc == 4);
        PICOL_SCAN_PTR(trg, argv[2]);
        if (!picolValidPtrFind(interp, PICOL_PTR_INTERP, (void*)trg)) {
            return picolErr1(interp,
                             "could not find interpreter \"%s\"",
                             argv[2]);
        }
        rc = picolEval(trg, argv[3]);
        picolSetResult(interp, trg->result);
        return rc;
    } else {
        return picolErr1(interp,
                         "bad option \"%s\": must be alias, create or eval",
                         argv[1]);
    }
    return picolErr(interp, "this should never be reached");
}
#endif /* PICOL_FEATURE_INTERP */
PICOL_COMMAND(join) {
    char buf[PICOL_MAX_STR] = "", buf2[PICOL_MAX_STR]="", *with = " ";
    char *cp, *cp2 = NULL;
    PICOL_ARITY2(argc == 2 || argc == 3, "join list ?joinString?");
    if (argc == 3) {
        with = argv[2];
    }
    for (cp=picolParseList(argv[1], buf); cp; cp=picolParseList(cp, buf)) {
        PICOL_APPEND(buf2, buf);
        cp2 = buf2 + strlen(buf2);
        if (*with) {
            PICOL_APPEND(buf2, with);
        }
    }
    if (cp2) {
        *cp2 = '\0'; /* remove last separator */
    }
    return picolSetResult(interp, buf2);
}
PICOL_COMMAND(lappend) {
    char buf[PICOL_MAX_STR] = "";
    int a, set_rc;
    picolVar* v;
    PICOL_ARITY2(argc >= 2, "lappend varName ?value value ...?");
    if ((v = picolGetVar(interp, argv[1]))) {
        strcpy(buf, v->val);
    }
    for (a = 2; a < argc; a++) {
        PICOL_LAPPEND(buf, argv[a]);
    }
    set_rc = picolSetVar(interp, argv[1], buf);
    if (set_rc != PICOL_OK) {
        return PICOL_OK;
    }
    return picolSetResult(interp, buf);
}
PICOL_COMMAND(lassign) {
    char result[PICOL_MAX_STR] = "", element[PICOL_MAX_STR];
    char* cp;
    int i = 2, set_rc;
    PICOL_ARITY2(argc >= 2, "lassign list ?varName ...?");
    PICOL_FOREACH(element, cp, argv[1]) {
        if (i < argc) {
            set_rc = picolSetVar(interp, argv[i], element);
            if (set_rc != PICOL_OK) {
                return set_rc;
            }
        } else {
            PICOL_LAPPEND(result, element);
        }
        i++;
    }
    for (; i < argc; i++) {
        set_rc = picolSetVar(interp, argv[i], "");
        if (set_rc != PICOL_OK) {
            return set_rc;
        }
    }
    return picolSetResult(interp, result);
}
PICOL_COMMAND(lindex) {
    char buf[PICOL_MAX_STR] = "", *cp;
    int n = 0, idx;
    PICOL_ARITY2((argc == 2) || (argc == 3), "lindex list [index]");
    /* Act as an identity function if no index is given. */
    if (argc == 2) {
        return picolSetResult(interp, argv[1]);
    }
    if (PICOL_EQ(argv[2], "end")) {
        idx = -1;
    } else {
        PICOL_SCAN_INT(idx, argv[2]);
        if (idx < 0) {
            return picolSetResult(interp, "");
        }
    }
    for (cp=picolParseList(argv[1], buf); cp; cp=picolParseList(cp, buf), n++) {
        if (n == idx) {
            return picolSetResult(interp, buf);
        }
    }
    if (idx == -1) {
        return picolSetResult(interp, buf);
    }
    return PICOL_OK;
}
PICOL_COMMAND(linsert) {
    char buf[PICOL_MAX_STR] = "", buf2[PICOL_MAX_STR]="", *cp;
    int pos = -1, j=0, a, atend=0;
    int inserted = 0;
    PICOL_ARITY2(argc >= 3, "linsert list index element ?element ...?");
    if (!PICOL_EQ(argv[2], "end")) {
        PICOL_SCAN_INT(pos, argv[2]);
    } else {
        atend = 1;
    }
    PICOL_FOREACH(buf, cp, argv[1]) {
        if (!inserted && !atend && pos==j) {
            for (a=3; a < argc; a++) {
                PICOL_LAPPEND(buf2, argv[a]);
                inserted = 1;
            }
        }
        PICOL_LAPPEND(buf2, buf);
        j++;
    }
    if (!inserted) {
        atend = 1;
    }
    if (atend) {
        for (a=3; a<argc; a++) {
            PICOL_LAPPEND(buf2, argv[a]);
        }
    }
    return picolSetResult(interp, buf2);
}
PICOL_COMMAND(list) {
    /* usage: list ?value ...? */
    char buf[PICOL_MAX_STR] = "";
    int a;
    for (a=1; a<argc; a++) {
        PICOL_LAPPEND(buf, argv[a]);
    }
    return picolSetResult(interp, buf);
}
PICOL_COMMAND(llength) {
    char buf[PICOL_MAX_STR], *cp;
    int n = 0;
    PICOL_ARITY2(argc == 2, "llength list");
    PICOL_FOREACH(buf, cp, argv[1]) {
        n++;
    }
    return picolSetIntResult(interp, n);
}
PICOL_COMMAND(lmap) {
    PICOL_ARITY2(argc == 4, "lmap varList list command");
    return picolLmap(interp, argv[1], argv[2], argv[3], 1);
}
PICOL_COMMAND(lrange) {
    char buf[PICOL_MAX_STR] = "", buf2[PICOL_MAX_STR] = "", *cp;
    int from, to, a = 0, toend = 0;
    PICOL_ARITY2(argc == 4, "lrange list first last");
    PICOL_SCAN_INT(from, argv[2]);
    if (PICOL_EQ(argv[3], "end")) {
        toend = 1;
    } else {
        PICOL_SCAN_INT(to, argv[3]);
    }
    PICOL_FOREACH(buf, cp, argv[1]) {
        if (a>=from && (toend || a<=to)) {
            PICOL_LAPPEND(buf2, buf);
        }
        a++;
    }
    return picolSetResult(interp, buf2);
}
PICOL_COMMAND(lrepeat) {
    char result[PICOL_MAX_STR] = "";
    int count, i, j;
    PICOL_ARITY2(argc >= 2, "lrepeat count ?element ...?");
    PICOL_SCAN_INT(count, argv[1]);
    for (i = 0; i < count; i++) {
        for (j = 2; j < argc; j++) {
            PICOL_LAPPEND(result, argv[j]);
        }
    }
    return picolSetResult(interp, result);
}
PICOL_COMMAND(lreplace) {
    char buf[PICOL_MAX_STR] = "";
    char buf2[PICOL_MAX_STR] = "";
    char* cp;
    int from, to = INT_MAX, a = 0, done = 0, j, toend = 0;
    PICOL_ARITY2(argc >= 4, "lreplace list first last ?element element ...?");
    PICOL_SCAN_INT(from, argv[2]);
    if (PICOL_EQ(argv[3], "end")) {
        toend = 1;
    } else {
        PICOL_SCAN_INT(to, argv[3]);
    }

    if (from < 0 && to < 0) {
        for (j = 4; j < argc; j++) {
            PICOL_LAPPEND(buf2, argv[j]);
        }
        done = 1;
    }
    PICOL_FOREACH(buf, cp, argv[1]) {
        if (a < from || (a > to && !toend)) {
            PICOL_LAPPEND(buf2, buf);
        } else if (!done) {
            for (j = 4; j < argc; j++) {
                PICOL_LAPPEND(buf2, argv[j]);
            }
            done = 1;
        }
        a++;
    }
    if (!done) {
        for (j = 4; j < argc; j++) {
            PICOL_LAPPEND(buf2, argv[j]);
        }
    }

    return picolSetResult(interp, buf2);
}
PICOL_COMMAND(lreverse) {
    char result[PICOL_MAX_STR] = "", element[PICOL_MAX_STR] = "";
    char* cp;
    int element_len, result_len = 0, i, needs_braces;
    PICOL_ARITY2(argc == 2, "lreverse list");
    if (argv[1][0] == '\0') {
        return picolSetResult(interp, "");
    }
    PICOL_FOREACH(element, cp, argv[1]) {
        element_len = strlen(element);
        needs_braces = picolNeedsBraces(element);
        memmove(result + element_len + needs_braces*2 + 1,
                result,
                result_len + 1);
        for (i = needs_braces; i < element_len + needs_braces; i++) {
            result[i] = element[i - needs_braces];
        }
        if (needs_braces) {
            result[i] = '}';
            result[0] = '{';
            i++;
        }
        result[i] = ' ';
        result_len += element_len + needs_braces*2 + 1;
    }
    result[result_len - 1] = '\0';
    return picolSetResult(interp, result);
}
PICOL_COMMAND(lsearch) {
    char buf[PICOL_MAX_STR] = "", *cp;
    char* list;
    char* pattern;
    int j = 0;
    int match_mode; // 0 for exact, 1 for glob.

    PICOL_ARITY2(
        argc == 3 || argc == 4,
        "lsearch ?-exact|-glob? list pattern"
    );
    if (argc == 4) {
        list = argv[2];
        pattern = argv[3];

        if (PICOL_EQ(argv[1], "-exact")) {
            match_mode = 0;
        } else if (PICOL_EQ(argv[1], "-glob")) {
            match_mode = 1;
        } else {
            return picolErr1(
                interp,
                "bad option \"%s\": must be -exact or -glob",
                argv[1]
            );
        }
    } else {
        list = argv[1];
        pattern = argv[2];
        match_mode = 1;
    }

    PICOL_FOREACH(buf, cp, list) {
        if ((match_mode == 0 && PICOL_EQ(pattern, buf)) ||
            (match_mode == 1 && picolMatch(pattern, buf) > 0)) {
            return picolSetIntResult(interp, j);
        }
        j++;
    }

    return picolSetResult(interp, "-1");
}
PICOL_COMMAND(lset) {
    char buf[PICOL_MAX_STR] = "", buf2[PICOL_MAX_STR]="", *cp;
    picolVar* var;
    int pos, a=0, set_rc;
    PICOL_ARITY2(argc == 4, "lset listVar index value");
    PICOL_GET_VAR(var, argv[1]);
    if (var->val == NULL) {
        var = picolGetGlobalVar(interp, argv[1]);
    }
    if (var == NULL) {
        return picolErr1(interp, "no variable %s", argv[1]);
    }
    PICOL_SCAN_INT(pos, argv[2]);
    PICOL_FOREACH(buf, cp, var->val) {
        if (a==pos) {
            PICOL_LAPPEND(buf2, argv[3]);
        } else {
            PICOL_LAPPEND(buf2, buf);
        }
        a++;
    }
    if (pos < 0 || pos > a) {
        return picolErr(interp, "list index out of range");
    }
    set_rc = picolSetVar(interp, var->name, buf2);
    if (set_rc != PICOL_OK) {
        return set_rc;
    }
    return picolSetResult(interp, buf2);
}
/* ----------- sort functions for lsort ---------------- */
int picolQsortCompStr(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}
int picolQsortCompStrDecr(const void* a, const void* b) {
    return -strcmp(*(const char**)a, *(const char**)b);
}
int picolQsortCompInt(const void* a, const void* b) {
    int diff = atoi(*(const char**)a)-atoi(*(const char**)b);
    return (diff > 0 ? 1 : diff < 0 ? -1 : 0);
}
int picolLsort(picolInterp* interp, int argc, char** argv, void* pd) {
    char buf[PICOL_MAX_STR] = "";
    char** av = argv+1;
    int ac = argc-1, a;
    if (argc<2) {
        return picolSetResult(interp, "");
    }
    if (argc>2 && PICOL_EQ(argv[1], "-decreasing")) {
        qsort(++av, --ac, sizeof(char*), picolQsortCompStrDecr);
    } else if (argc>2 && PICOL_EQ(argv[1], "-integer")) {
        qsort(++av, --ac, sizeof(char*), picolQsortCompInt);
    } else if (argc>2 && PICOL_EQ(argv[1], "-unique")) {
        qsort(++av, --ac, sizeof(char*), picolQsortCompStr);
        for (a=0; a<ac; a++) {
            if (a==0 || !PICOL_EQ(av[a], av[a-1])) {
                PICOL_LAPPEND(buf, av[a]);
            }
        }
        return picolSetResult(interp, buf);
    } else {
        qsort(av, ac, sizeof(char*), picolQsortCompStr);
    }
    picolSetResult(interp, picolList(buf, ac, av));
    return PICOL_OK;
}
PICOL_COMMAND(lsort) {
    /* Dispatch to the helper function picolLsort. */
    char buf[PICOL_MAX_STR] = "_l ";
    PICOL_ARITY2(
        argc == 2 || argc == 3,
        "lsort ?-decreasing|-integer|-unique? list"
    );
    PICOL_APPEND(buf, argv[1]);
    if (argc==3) {
        PICOL_APPEND(buf, " ");
        PICOL_APPEND(buf, argv[2]);
    }
    return picolEval(interp, buf);
}
int picol_Math(picolInterp* interp, int argc, char** argv, void* pd) {
    int a = 0, b = 0, c = -1, p;
    if (argc>=2) {
        PICOL_SCAN_INT(a, argv[1]);
    }
    if (argc==3) {
        PICOL_SCAN_INT(b, argv[2]);
    }
    if (argv[0][0] == '/' && !b) {
        return picolErr(interp, "divide by zero");
    }
    /*ARITY2 "+ ?arg..." */
    if (PICOL_EQ(argv[0], "+" )) {
        PICOL_FOLD(c=0, c += a, 1);
    }
    /*ARITY2 "- arg ?apicolParseCmdrg...?" */
    else if (PICOL_EQ(argv[0], "-" )) {
        if (argc==2) {
            c= -a;
        } else {
            PICOL_FOLD(c=a, c -= a, 2);
        }
    }
    /*ARITY2"* ?arg...?" */
    else if (PICOL_EQ(argv[0], "*" )) {
        PICOL_FOLD(c=1, c *= a, 1);
    }
    /*ARITY2 "** a b" */
    else if (PICOL_EQ(argv[0], "**" )) {
        PICOL_ARITY(argc==3);
        c = 1;
        while (b-- > 0) {
            c = c*a;
        }
    }
    /*ARITY2 "/ a b" */
    else if (PICOL_EQ(argv[0], "/" )) {
        PICOL_ARITY(argc==3);
        c = a / b;
    }
    /*ARITY2"% a b" */
    else if (PICOL_EQ(argv[0], "%" )) {
        PICOL_ARITY(argc==3);
        c = a % b;
    }
    /*ARITY2"&& ?arg...?"*/
    else if (PICOL_EQ(argv[0], "&&")) {
        PICOL_FOLD(c=1, c = c && a, 1);
    }
    /*ARITY2"|| ?arg...?"*/
    else if (PICOL_EQ(argv[0], "||")) {
        PICOL_FOLD(c=0, c = c || a, 1);
    }
    /*ARITY2"& ?arg...?"*/
    else if (PICOL_EQ(argv[0], "&")) {
        PICOL_FOLD(c=INT_MAX, c = c & a, 1);
    }
    /*ARITY2"| ?arg...?"*/
    else if (PICOL_EQ(argv[0], "|")) {
        PICOL_FOLD(c=0, c = c | a, 1);
    }
    /*ARITY2"^ ?arg...?"*/
    else if (PICOL_EQ(argv[0], "^")) {
        PICOL_FOLD(c=0, c = c ^ a, 1);
    }
    /*ARITY2"<< a b" */
    else if (PICOL_EQ(argv[0], "<<" )) {
        PICOL_ARITY(argc==3);
        if (b > (int)sizeof(int)*8 - 1) {
            char buf[PICOL_MAX_STR];
            PICOL_SNPRINTF(buf,
                           sizeof(buf),
                           "can't shift integer left by more than %d bit(s) "
                           "(%d given)",
                           (int)(sizeof(int)*8 - 1),
                           b);
            return picolErr(interp, buf);
        }
        c = a << b;
    }
    /*ARITY2">> a b" */
    else if (PICOL_EQ(argv[0], ">>" )) {
        PICOL_ARITY(argc==3);
        c = a >> b;
    }
    /*ARITY2"> a b" */
    else if (PICOL_EQ(argv[0], ">" )) {
        PICOL_ARITY(argc==3);
        c = a > b;
    }
    /*ARITY2">= a b"*/
    else if (PICOL_EQ(argv[0], ">=")) {
        PICOL_ARITY(argc==3);
        c = a >= b;
    }
    /*ARITY2"< a b" */
    else if (PICOL_EQ(argv[0], "<" )) {
        PICOL_ARITY(argc==3);
        c = a < b;
    }
    /*ARITY2"<= a b"*/
    else if (PICOL_EQ(argv[0], "<=")) {
        PICOL_ARITY(argc==3);
        c = a <= b;
    }
    /*ARITY2"== a b"*/
    else if (PICOL_EQ(argv[0], "==")) {
        PICOL_ARITY(argc==3);
        c = a == b;
    }
    /*ARITY2"!= a b"*/
    else if (PICOL_EQ(argv[0], "!=")) {
        PICOL_ARITY(argc==3);
        c = a != b;
    }
    return picolSetIntResult(interp, c);
}
PICOL_COMMAND(max) {
    int a, c, p;
    PICOL_ARITY2(argc >= 2, "max number ?number ...?");
    PICOL_SCAN_INT(a, argv[1]);
    PICOL_FOLD(c = a, c = c > a ? c : a, 1);
    return picolSetIntResult(interp, c);
}
PICOL_COMMAND(min) {
    int a, c, p;
    PICOL_ARITY2(argc >= 2, "min number ?number ...?");
    PICOL_SCAN_INT(a, argv[1]);
    PICOL_FOLD(c = a, c = c < a ? c : a, 1);
    return picolSetIntResult(interp, c);
}
int picolNeedsBraces(char* str) {
    int i;
    int length = 0;
    for (i = 0;; i++) {
        if (str[i] == '\0') {
            break;
        }
        switch (str[i]) {
        case ' ': return 1;
        case '"': return 1;
        case '$': return 1;
        case '[': return 1;
        case '\\': return 1;
        case '\n': return 1;
        case '\r': return 1;
        case '\t': return 1;
        }
        length++;
    }
    if (length == 0) {
        return 1;
    }
    return 0;
}
PICOL_COMMAND(not) {
    int res;
    PICOL_ARITY2(argc == 2, "! number");
    PICOL_SCAN_INT(res, argv[1]);
    return picolSetBoolResult(interp, !res);
}
#if PICOL_FEATURE_IO
PICOL_COMMAND(open) {
    char* mode = "r";
    FILE* fp = NULL;
    char fp_str[PICOL_MAX_STR];
    PICOL_ARITY2(argc == 2 || argc == 3, "open fileName ?access?");
    if (argc == 3) {
        mode = argv[2];
    }
    fp = fopen(argv[1], mode);
    if (fp == NULL) {
        return picolErr1(interp, "could not open %s", argv[1]);
    }
    PICOL_SNPRINTF(fp_str, sizeof(fp_str), "%p", (void*)fp);
    picolValidPtrAdd(interp, PICOL_PTR_CHAN, (void*)fp);
    return picolSetResult(interp, fp_str);
}
#endif
PICOL_COMMAND(pid) {
    PICOL_ARITY2(argc == 1, "pid");
    return picolSetIntResult(interp, PICOL_GETPID());
}
PICOL_COMMAND(proc) {
    char **procdata = NULL;
    picolCmd *c = picolGetCmd(interp, argv[1]);
    PICOL_ARITY2(argc == 4, "proc name args body");
    if (c != NULL) {
        procdata = c->privdata;
    }
    if (procdata == NULL) {
        procdata = malloc(sizeof(char*)*2);
        if (c != NULL) {
            c->privdata = procdata;
            c->func = picolCallProc; /* may override C-coded commands */
        }
    } else {
        free(procdata[0]);
        free(procdata[1]);
    }
    procdata[0] = strdup(argv[2]); /* arguments list */
    procdata[1] = strdup(argv[3]); /* procedure body */
    if (c == NULL) {
        picolRegisterCmd(interp, argv[1], picolCallProc, procdata);
    }
    return PICOL_OK;
}
#if PICOL_FEATURE_PUTS
PICOL_COMMAND(puts) {
    FILE* fp = stdout;
    char* chan=NULL, *str="", *fmt = "%s\n";
    int rc;
    PICOL_ARITY2(
        argc >= 2 && argc <= 4,
        "puts ?-nonewline? ?channelId? string"
    );
    if (argc==2) {
        str = argv[1];
    } else if (argc==3) {
        str = argv[2];
        if (PICOL_EQ(argv[1], "-nonewline")) {
            fmt = "%s";
        } else {
            chan = argv[1];
        }
    } else { /* argc==4 */
        if (!PICOL_EQ(argv[1], "-nonewline")) {
            return picolErr(interp, "usage: puts ?-nonewline? ?chan? string");
        }
        fmt = "%s";
        chan = argv[2];
        str = argv[3];
    }
    if (chan && !((PICOL_EQ(chan, "stdout")) || PICOL_EQ(chan, "stderr"))) {
#if PICOL_FEATURE_IO
        PICOL_SCAN_PTR(fp, chan);
        if (!picolValidPtrFind(interp, PICOL_PTR_CHAN, (void*)fp)) {
            return picolErr1(interp,
                             "can not find channel named \"%s\"",
                             chan);
        }
#else
        return picolErr1(interp,
                         "bad channel \"%s\": must be stdout, or stderr",
                         chan);
#endif /* PICOL_FEATURE_IO */
    }
    if (chan && PICOL_EQ(chan, "stderr")) {
        fp = stderr;
    }
    rc = fprintf(fp, fmt, str);
    if (!rc || ferror(fp)) {
        return picolErr(interp, "channel is not open for writing");
    }
    return picolSetResult(interp, "");
}
#endif /* PICOL_FEATURE_PUTS */
#if PICOL_FEATURE_IO
PICOL_COMMAND(pwd) {
    char buf[PICOL_MAX_STR] = "\0";
    PICOL_ARITY(argc == 1);
    return picolSetResult(interp, PICOL_GETCWD(buf, PICOL_MAX_STR));
}
#endif
PICOL_COMMAND(rand) {
    int n;
    PICOL_ARITY2(argc == 2, "rand n (returns a random integer 0..<n)");
    PICOL_SCAN_INT(n, argv[1]);
    return picolSetIntResult(interp, n ? rand()%n : rand());
}
#if PICOL_FEATURE_IO
PICOL_COMMAND(read) {
    char buf[PICOL_MAX_STR*2];
    int buf_size = sizeof(buf) - 1;
    int size = buf_size; /* Size argument value. */
    int actual_size = 0;
    FILE* fp = NULL;
    PICOL_ARITY2(argc == 2 || argc == 3, "read channelId ?size?");
    PICOL_SCAN_PTR(fp, argv[1]);
    if (!picolValidPtrFind(interp, PICOL_PTR_CHAN, (void*)fp)) {
        return picolErr1(interp,
                         "can not find channel named \"%s\"",
                         argv[1]);
    }
    if (argc == 3) {
        PICOL_SCAN_INT(size, argv[2]);
        if (size > PICOL_MAX_STR - 1) {
            return picolErr1(interp, "size %s too large", argv[2]);
        }
    }
    actual_size = fread(buf, 1, size, fp);
    if (actual_size > PICOL_MAX_STR - 1) {
        return picolErr(interp, "read contents too long");
    } else {
        buf[actual_size] = '\0';
        return picolSetResult(interp, buf);
    }
}
#endif /* PICOL_FEATURE_IO */
PICOL_COMMAND(rename) {
    int found = 0, deleting = 0;
    picolCmd* c, *last = NULL, *toFree = NULL;
    PICOL_ARITY2(argc == 3, "rename oldName newName");
    deleting = PICOL_EQ(argv[2], "");

    for (c = interp->commands; c; last = c, c=c->next) {
        if (PICOL_EQ(c->name, argv[1])) {
            if (last == NULL && deleting) {
                interp->commands = c->next;  /* delete the first command */
                toFree = c;
            } else if (deleting) {
                last->next = c->next;        /* delete an nth command */
                toFree = c;
            } else {
                c->name = strdup(argv[2]);   /* overwrite, but do not free() */
            }
            found = 1;
            break;
        }
    }

    if (toFree != NULL) {
        if (toFree->privdata != NULL) {
            char **privdata = toFree->privdata;
            free(privdata[0]);
            free(privdata[1]);
            free(privdata);
        }
        free(toFree->name);
        free(toFree);
    }

    if (!found) {
        return picolErr1(
            interp,
            deleting
            ? "can't delete \"%s\": command doesn't exist"
            : "can't rename \"%s\": command doesn't exist",
            argv[1]
        );
    }

    return PICOL_OK;
}
PICOL_COMMAND(return ) {
    PICOL_ARITY2(argc == 1 || argc == 2, "return ?result?");
    picolSetResult(interp, (argc == 2) ? argv[1] : "");
    return PICOL_RETURN;
}
PICOL_COMMAND(scan) {
    /* Limited to one "%<integer><character>" code so far. */
    int result, rc = 1;
    PICOL_ARITY2(argc == 3 || argc == 4, "scan string formatString ?varName?");
    if (strlen(argv[2]) != 2 || argv[2][0] != '%') {
        return picolErr1(interp, "bad format \"%s\"", argv[2]);
    }
    switch(argv[2][1]) {
    case 'c':
        result = (int)(argv[1][0] & 0xFF);
        break;
    case 'd':
    case 'x':
    case 'o':
        rc = sscanf(argv[1], argv[2], &result);
        break;
    default:
        return picolErr1(interp, "bad scan conversion character \"%s\"",
                         argv[2]);
    }
    if (rc != 1) {
        result = 0; /* This is what Tcl 8.6 does. */
    }
    if (argc==4) {
        picolSetIntVar(interp, argv[3], result);
        result = 1;
    }
    return picolSetIntResult(interp, result);
}
PICOL_COMMAND(set) {
    picolVar* pv;
    PICOL_ARITY2(argc == 2 || argc == 3, "set varName ?newValue?");
    if (argc == 2) {
        PICOL_GET_VAR(pv, argv[1]);
        if (pv && pv->val) {
            return picolSetResult(interp, pv->val);
        } else {
            pv = picolGetGlobalVar(interp, argv[1]);
        }
        if (pv == NULL || pv->val == NULL) {
            return picolErr1(interp, "no value of \"%s\"\n", argv[1]);
        }
        return picolSetResult(interp, pv->val);
    } else {
        int set_rc = picolSetVar(interp, argv[1], argv[2]);
        if (set_rc != PICOL_OK) {
            return set_rc;
        }
        return picolSetResult(interp, argv[2]);
    }
}
int picolSource(picolInterp* interp, char* filename) {
    char buf[PICOL_MAX_STR*64];
    char prev[PICOL_MAX_STR];
    int rc;
    picolVar* pv;

    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        return picolErr1(interp, "No such file or directory \"%s\"", filename);
    }

    pv = picolGetGlobalVar(interp, PICOL_INFO_SCRIPT_VAR);
    if (pv != NULL && pv->val != NULL) {
        strncpy(prev, pv->val, sizeof(prev));
    }

    picolSetVar(interp, PICOL_INFO_SCRIPT_VAR, filename);

    buf[fread(buf, 1, sizeof(buf), fp)] = '\0';
    fclose(fp);

    rc = picolEval(interp, buf);

    picolSetVar(interp, PICOL_INFO_SCRIPT_VAR, prev);

    return rc;
}
#if PICOL_FEATURE_IO
PICOL_COMMAND(source) {
    PICOL_ARITY2(argc == 2, "source filename");
    return picolSource(interp, argv[1]);
}
#endif
PICOL_COMMAND(split) {
    char* split = " ", *cp, *start;
    char buf[PICOL_MAX_STR] = "", buf2[PICOL_MAX_STR] = "";
    PICOL_ARITY2(argc == 2 || argc == 3, "split string ?splitChars?");
    if (argc==3) {
        split = argv[2];
    }
    if (PICOL_EQ(split, "")) {
        for (cp = argv[1]; *cp; cp++) {
            buf2[0] = *cp;
            PICOL_LAPPEND(buf, buf2);
        }
    } else {
        for (cp = argv[1], start=cp; *cp; cp++) {
            if (strchr(split, *cp)) {
                strncpy(buf2, start, cp-start);
                buf2[cp-start] = '\0';
                PICOL_LAPPEND(buf, buf2);
                buf2[0] = '\0';
                start = cp+1;
            }
        }
        PICOL_LAPPEND(buf, start);
    }
    return picolSetResult(interp, buf);
}
char* picolStrFirstTrailing(char* str, char chr) {
    char* cp;
    char* end;

    if (*str == '\0') return NULL;

    for (cp = str; *cp; cp++);
    cp--;
    end = cp;

    for (; cp >= str && *cp == chr; cp--);

    if (cp == end) return NULL;
    cp++;
    return cp;
}
char* picolStrRev(char* str) {
    char* cp = str, *cp2 = str + strlen(str)-1, tmp;
    while (cp<cp2) {
        tmp=*cp;
        *cp=*cp2;
        *cp2=tmp;
        cp++;
        cp2--;
    }
    return str;
}
char* picolToLower(char* str) {
    char* cp;
    for (cp = str; *cp; cp++) {
        *cp = tolower(*cp);
    }
    return str;
}
char* picolToUpper(char* str) {
    char* cp;
    for (cp = str; *cp; cp++) {
        *cp = toupper(*cp);
    }
    return str;
}
PICOL_COMMAND(string) {
    char buf[PICOL_MAX_STR] = "\0";
    PICOL_ARITY2(argc >= 3, "string option string ?arg..?");
    if (PICOL_SUBCMD("length")) {
        picolSetIntResult(interp, strlen(argv[2]));
    } else if (PICOL_SUBCMD("compare")) {
        PICOL_ARITY2(argc == 4, "string compare s1 s2");
        picolSetIntResult(interp, strcmp(argv[2], argv[3]));
    } else if (PICOL_SUBCMD("equal")) {
        PICOL_ARITY2(argc == 4, "string equal s1 s2");
        picolSetIntResult(interp, PICOL_EQ(argv[2], argv[3]));
    } else if (PICOL_SUBCMD("first") || PICOL_SUBCMD("last")) {
        int offset = 0, res = -1;
        char* cp;
        if (argc != 4 && argc != 5) {
            return picolErr1(interp,
                             "usage: string %s substr str ?index?",
                             argv[1]);
        }
        if (argc == 5) {
            PICOL_SCAN_INT(offset, argv[4]);
        }
        if (PICOL_SUBCMD("first")) {
            cp = strstr(argv[3]+offset, argv[2]);
        }
        else { /* last */
            char* cp2 = argv[3]+offset;
            while ((cp = strstr(cp2, argv[2]))) {
                cp2 = cp + 1;
            }
            cp = cp2 - 1;
        }
        if (cp != NULL) res = cp - argv[3];
        picolSetIntResult(interp, res);
    } else if (PICOL_SUBCMD("index") || PICOL_SUBCMD("range")) {
        int from, to, maxi = strlen(argv[2])-1;
        if (PICOL_SUBCMD("index")) {
            PICOL_ARITY2(argc == 4, "string index string charIndex");
        } else {
            PICOL_ARITY2(argc == 5, "string range string first last");
        }

        if (PICOL_EQ(argv[3], "end")) {
            from = maxi;
        } else {
            PICOL_SCAN_INT(from, argv[3]);
        }
        if (PICOL_SUBCMD("index")) {
            to = from;
        } else {
            if (PICOL_EQ(argv[4], "end")) {
                to = maxi;
            } else {
                PICOL_SCAN_INT(to, argv[4]);
            }
        }

        from = from < 0  ? 0    : from;
        to   = to > maxi ? maxi : to;
        if (from <= to) {
            strncpy(buf, &argv[2][from], to - from + 1);
            buf[to + 1] = '\0';
            picolSetResult(interp, buf);
        }
    } else if (PICOL_SUBCMD("map")) {
        char* charMap;
        char* str;

        char from[PICOL_MAX_STR] = "\0";
        char to[PICOL_MAX_STR] = "\0";
        char result[PICOL_MAX_STR] = "\0";

        char tiny[2] = "\0\0";

        int nocase = 0;

        if (argc == 4) {
            charMap = argv[2];
            str = argv[3];
        } else if (argc == 5 && PICOL_EQ(argv[2], "-nocase")) {
            charMap = argv[3];
            str = argv[4];
            nocase = 1;
        } else {
            return picolErr(interp, "usage: string map ?-nocase? charMap str");
        }

        for (; *str; str++) {
            int fromLen = 0;
            int matched = 0;
            char* mp = charMap;

            PICOL_FOREACH(from, mp, charMap) {
                mp = picolParseList(mp, to);

                if (mp == NULL) {
                    return picolErr(interp, "char map list unbalanced");
                }
                if (PICOL_EQ(from, "")) {
                    continue;
                }

                fromLen = strlen(from);
                if (picolStrCompare(str, from, fromLen, nocase) == 0) {
                    PICOL_APPEND(result, to);
                    str += fromLen - 1;
                    matched = 1;
                    break;
                }
            }

            if (!matched) {
                tiny[0] = *str;
                PICOL_APPEND(result, tiny);
            }
        }

        return picolSetResult(interp, result);
    } else if (PICOL_SUBCMD("match")) {
        int res = 0;
        if (argc == 4) {
            res = picolMatch(argv[2], argv[3]);
            if (res < 0) {
                return picolErr1(interp,
                                 "unsupported pattern: \"%s\"",
                                 argv[2]);
            }
            return picolSetBoolResult(interp, res);
        } else if (argc == 5 && PICOL_EQ(argv[2], "-nocase")) {
            char* uppercase_pat = strdup(argv[3]);
            picolToUpper(uppercase_pat);
            picolToUpper(argv[4]);
            res = picolMatch(uppercase_pat, argv[4]);
            free(uppercase_pat);
            if (res < 0) {
                return picolErr1(interp,
                                 "unsupported pattern: \"%s\"",
                                 argv[3]);
            }
            return picolSetBoolResult(interp, res);
        } else {
            return picolErr(interp, "usage: string match pat str");
        }
    } else if (PICOL_SUBCMD("is")) {
        PICOL_ARITY2(
            argc == 4 &&
            (PICOL_EQ(argv[2], "int") || PICOL_EQ(argv[2], "integer")),
            "string is integer str"
        );

        /* This is for Tcl 8 compatibility. */
        if (PICOL_EQ(argv[3], "")) {
            return picolSetBoolResult(interp, 1);
        }

        return picolSetBoolResult(interp,  picolIsInt(argv[3]));
    } else if (PICOL_SUBCMD("repeat")) {
        int j, n;
        PICOL_SCAN_INT(n, argv[3]);
        PICOL_ARITY2(argc == 4, "string repeat string count");
        for (j=0; j<n; j++) {
            PICOL_APPEND(buf, argv[2]);
        }
        picolSetResult(interp, buf);

    } else if (PICOL_SUBCMD("reverse")) {
        PICOL_ARITY2(argc == 3, "string reverse str");
        picolSetResult(interp, picolStrRev(argv[2]));
    } else if (PICOL_SUBCMD("tolower") && argc==3) {
        picolSetResult(interp, picolToLower(argv[2]));
    } else if (PICOL_SUBCMD("toupper") && argc==3) {
        picolSetResult(interp, picolToUpper(argv[2]));
    } else if (PICOL_SUBCMD("trim") ||
               PICOL_SUBCMD("trimleft") ||
               PICOL_SUBCMD("trimright")) {
        char* trimchars = " \t\n\r", *start, *end;
        int len;
        PICOL_ARITY2(
            argc ==3 || argc == 4,
            "string trim?left|right? string ?chars?"
        );
        if (argc==4) {
            trimchars = argv[3];
        }
        start = argv[2];
        if (!PICOL_SUBCMD("trimright")) {
            for (; *start; start++) {
                if (strchr(trimchars, *start)==NULL) {
                    break;
                }
            }
        }
        end = argv[2]+strlen(argv[2]);
        if (!PICOL_SUBCMD("trimleft")) {
            for (; end>=start; end--) {
                if (strchr(trimchars, *end)==NULL) {
                    break;
                }
            }
        }
        len = end - start+1;
        strncpy(buf, start, len);
        buf[len] = '\0';
        return picolSetResult(interp, buf);

    } else {
        return picolErr1(interp,
                         "bad option \"%s\": must be compare, equal, first, "
                         "index, is int, last, length, map, match, range, "
                         "repeat, reverse, tolower, or toupper",
                         argv[1]);
    }
    return PICOL_OK;
}
PICOL_COMMAND(subst) {
    PICOL_ARITY2(argc == 2, "subst string");
    return picolSubst(interp, argv[1]);
}
PICOL_COMMAND(switch) {
    char* cp, buf[PICOL_MAX_STR] = "";
    int fallthrough = 0, a;
    PICOL_ARITY2(argc > 2, "switch string pattern body ... ?default body?");
    if (argc==3) { /* braced body variant */
        PICOL_FOREACH(buf, cp, argv[2]) {
            if (fallthrough ||
                PICOL_EQ(buf, argv[1]) ||
                PICOL_EQ(buf, "default")) {
                cp = picolParseList(cp, buf);
                if (cp == NULL) {
                    return picolErr(interp,
                                    "switch: list must have an even number");
                }
                if (PICOL_EQ(buf, "-")) {
                    fallthrough = 1;
                } else {
                    return picolEval(interp, buf);
                }
            }
        }
    } else { /* unbraced body */
        if (argc%2) return picolErr(interp,
                                    "switch: list must have an even number");
        for (a = 2; a < argc; a++) {
            if (fallthrough ||
                PICOL_EQ(argv[a], argv[1]) ||
                PICOL_EQ(argv[a], "default")) {
                if (PICOL_EQ(argv[a + 1], "-")) {
                    fallthrough = 1;
                    a++;
                } else {
                    return picolEval(interp, argv[a + 1]);
                }
            }
        }
    }
    return picolSetResult(interp, "");
}
PICOL_COMMAND(time) {
    int j, n = 1, rc;
#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS
    int win_start;
#elif PICOL_CAN_USE_CLOCK_GETTIME
    struct timespec ts_start, ts_end;
#else
    clock_t start;
#endif
    double dt;
    char buf[PICOL_MAX_STR];
    PICOL_ARITY2(argc==2 || argc==3, "time command ?count?");
    if (argc==3) PICOL_SCAN_INT(n, argv[2]);
#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS
    win_start = GetTickCount();
#elif PICOL_CAN_USE_CLOCK_GETTIME
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
#else
    start = clock();
#endif
    for (j = 0; j < n; j++) {
        if ((rc = picolEval(interp, argv[1])) != PICOL_OK) {
            return rc;
        }
    }
#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS
    dt = 1000.0*(double)(GetTickCount() - win_start);
#elif PICOL_CAN_USE_CLOCK_GETTIME
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    dt = 1000000.0*(double)(ts_end.tv_sec  - ts_start.tv_sec) +
             0.001*(double)(ts_end.tv_nsec - ts_start.tv_nsec);
#else
    /* Very inaccurate. In particular, due to how clock() works you won't be
       able to measure the approximate duration of an [after ms] interval for
       testing. */
    dt = (double)(clock() - start)*1000000.0/CLOCKS_PER_SEC;
#endif
    PICOL_SNPRINTF(buf, sizeof(buf), "%.1f microseconds per iteration", dt/n);
    return picolSetResult(interp, buf);
}
PICOL_COMMAND(try) {
    int body_rc, handler_rc;
    char body_result[PICOL_MAX_STR], handler_result[PICOL_MAX_STR];
    PICOL_ARITY2(argc == 2 || argc == 4 || argc == 6 || argc == 8,
           "try body ?on error varName handler? ?finally script?");
    /*        0    1   2     3       4       5         6      7 */
    if ((argc == 4) && !PICOL_EQ(argv[2], "finally")) {
        return picolErr1(interp,
                         "bad argument \"%s\": expected \"finally\"",
                         argv[2]);
    }
    if ((argc == 6 || argc == 8) && !PICOL_EQ(argv[2], "on")) {
        return picolErr1(interp,
                         "bad argument \"%s\": expected \"on\"",
                         argv[2]);
    }
    if ((argc == 6 || argc == 8) && !PICOL_EQ(argv[3], "error")) {
        return picolErr1(interp,
                         "bad argument \"%s\": expected \"error\"",
                         argv[3]);
    }
    if ((argc == 8) && !PICOL_EQ(argv[6], "finally")) {
        return picolErr1(interp,
                         "bad argument \"%s\": expected \"finally\"",
                         argv[6]);
    }
    body_rc = picolEval(interp, argv[1]);
    strcpy(body_result, interp->result);
    /* Run the error handler if we were given one and there was an error. */
    if ((argc == 6 || argc == 8) && body_rc == PICOL_ERR) {
        int set_rc = picolSetVar(interp, argv[4], interp->result);
        if (set_rc != PICOL_OK) {
            return set_rc;
        }
        handler_rc = picolEval(interp, argv[5]);
        strcpy(handler_result, interp->result);
    }
    /* Run the "finally" script. If it fails, return its result. */
    if (argc == 4 || argc == 8) {
        int finally_rc = picolEval(interp, argv[argc == 4 ? 3 : 7]);
        if (finally_rc != PICOL_OK) {
            return finally_rc;
        }
    }
    /* Return the error handler result if there was an error. */
    if ((argc == 6 || argc == 8) && body_rc == PICOL_ERR) {
        picolSetResult(interp, handler_result);
        return handler_rc;
    }
    /* Return the result of evaluating the body. */
    picolSetResult(interp, body_result);
    return body_rc;
}
PICOL_COMMAND(unset) {
    picolVar* v, *lastv = NULL;
    int found = 0;
    PICOL_ARITY2(argc == 2, "unset varName");
#if PICOL_FEATURE_ARRAYS
    /* Remove an array element. */
    if (strchr(argv[1], '(')) {
        picolArray* ap;
        char key[PICOL_MAX_STR];
        ap = picolArrFindByName(interp, argv[1], 0, key);
        if (ap != NULL) {
            /* The array exists. */
            if (picolArrUnset(ap, key) > 0) {
                found = 1;
            } else {
                return picolErr1(interp,
                                 "can't unset \"%s\": no such element in array",
                                 argv[1]);
            }
        }
    } else {
        picolArray* ap;
        ap = picolArrFindByName(interp, argv[1], 0, NULL);
        if (ap != NULL) {
            /* The variable is an array. */
            picolArrDestroy1(ap);
        }
#endif
        for (v = interp->callframe->vars; v != NULL; lastv = v, v = v->next) {
            if (PICOL_EQ(v->name, argv[1])) {
                found = 1;
                if (lastv == NULL) {
                    interp->callframe->vars = v->next;
                } else {
                    lastv->next = v->next;
                }
                free(v->name);
                free(v->val);
                free(v);
                break;
            }
        }
#if PICOL_FEATURE_ARRAYS
    }
#endif
    if (!found) {
        return picolErr1(interp,
                         "can't unset \"%s\": no such variable",
                         argv[1]);
    }
    return picolSetResult(interp, "");
}
PICOL_COMMAND(uplevel) {
    char buf[PICOL_MAX_STR];
    int rc, delta;
    picolCallFrame* cf = interp->callframe;
    PICOL_ARITY2(argc >= 3, "uplevel level command ?arg...?");
    if (PICOL_EQ(argv[1], "#0")) {
        delta = 9999;
    } else {
        PICOL_SCAN_INT(delta, argv[1]);
    }
    for (; delta>0 && interp->callframe->parent; delta--) {
        interp->callframe = interp->callframe->parent;
    }
    rc = picolEval(interp, picolConcat(buf, argc-1, argv+1));
    interp->callframe = cf; /* back to normal */
    return rc;
}
PICOL_COMMAND(variable) {
    char buf[PICOL_MAX_STR]; /* limited to :: namespace so far */
    int a, rc = PICOL_OK;
    PICOL_ARITY2(argc>1, "variable ?name value...? name ?value?");
    for (a = 1; a < argc && rc == PICOL_OK; a++) {
        strcpy(buf, "global ");
        strcat(buf, argv[a]);
        rc = picolEval(interp, buf);
        if (rc == PICOL_OK && a < argc-1) {
            rc = picolSetGlobalVar(interp, argv[a], argv[a+1]);
            a++;
        }
    }
    return rc;
}
PICOL_COMMAND(while) {
    PICOL_ARITY2(argc == 3, "while test command");
    while (1) {
        int rc = picolCondition(interp, argv[1]);
        if (rc != PICOL_OK) {
            return rc;
        }
        if (atoi(interp->result)) {
            rc = picolEval(interp, argv[2]);
            if (rc == PICOL_CONTINUE || rc == PICOL_OK)  {
                continue;
            } else if (rc == PICOL_BREAK) {
                break;
            } else {
                return rc;
            }
        } else {
            break;
        }
    }
    return picolSetResult(interp, "");
} /* --------------------------------------------------------- Initialization */
void picolRegisterCoreCmds(picolInterp* interp) {
    int j;
    char* name[] = {
        "+", "-", "*", "**", "/", "%",
        ">", ">=", "<", "<=", "==", "!=",
        "&&", "||", "&", "|", "^", "<<", ">>"
    };
    for (j = 0; j < (int)(sizeof(name)/sizeof(char*)); j++) {
        picolRegisterCmd(interp, name[j], picol_Math, NULL);
    }
    picolRegisterCmd(interp, "abs",      picol_abs, NULL);
#if PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_UNIX || \
    PICOL_TCL_PLATFORM_PLATFORM == PICOL_TCL_PLATFORM_WINDOWS
    picolRegisterCmd(interp, "after",    picol_after, NULL);
#endif
    picolRegisterCmd(interp, "append",   picol_append, NULL);
    picolRegisterCmd(interp, "apply",    picol_apply, NULL);
    picolRegisterCmd(interp, "break",    picol_break, NULL);
    picolRegisterCmd(interp, "catch",    picol_catch, NULL);
    picolRegisterCmd(interp, "clock",    picol_clock, NULL);
    picolRegisterCmd(interp, "concat",   picol_concat, NULL);
    picolRegisterCmd(interp, "continue", picol_continue, NULL);
#if PICOL_FEATURE_PUTS
    picolRegisterCmd(interp, "debug",    picol_debug, NULL);
#endif
    picolRegisterCmd(interp, "eq",       picol_EqNe, NULL);
    picolRegisterCmd(interp, "error",    picol_error, NULL);
    picolRegisterCmd(interp, "eval",     picol_eval, NULL);
    picolRegisterCmd(interp, "expr",     picol_expr, NULL);
    picolRegisterCmd(interp, "file",     picol_file, NULL);
    picolRegisterCmd(interp, "for",      picol_for, NULL);
    picolRegisterCmd(interp, "foreach",  picol_foreach, NULL);
    picolRegisterCmd(interp, "format",   picol_format, NULL);
    picolRegisterCmd(interp, "global",   picol_global, NULL);
    picolRegisterCmd(interp, "if",       picol_if, NULL);
    picolRegisterCmd(interp, "in",       picol_InNi, NULL);
    picolRegisterCmd(interp, "incr",     picol_incr, NULL);
    picolRegisterCmd(interp, "info",     picol_info, NULL);
    picolRegisterCmd(interp, "join",     picol_join, NULL);
    picolRegisterCmd(interp, "lappend",  picol_lappend, NULL);
    picolRegisterCmd(interp, "lassign",  picol_lassign, NULL);
    picolRegisterCmd(interp, "lindex",   picol_lindex, NULL);
    picolRegisterCmd(interp, "linsert",  picol_linsert, NULL);
    picolRegisterCmd(interp, "list",     picol_list, NULL);
    picolRegisterCmd(interp, "llength",  picol_llength, NULL);
    picolRegisterCmd(interp, "lmap",     picol_lmap, NULL);
    picolRegisterCmd(interp, "lrange",   picol_lrange, NULL);
    picolRegisterCmd(interp, "lrepeat",  picol_lrepeat, NULL);
    picolRegisterCmd(interp, "lreplace", picol_lreplace, NULL);
    picolRegisterCmd(interp, "lreverse", picol_lreverse, NULL);
    picolRegisterCmd(interp, "lsearch",  picol_lsearch, NULL);
    picolRegisterCmd(interp, "lset",     picol_lset, NULL);
    picolRegisterCmd(interp, "lsort",    picol_lsort, NULL);
    picolRegisterCmd(interp, "max",      picol_max, NULL);
    picolRegisterCmd(interp, "min",      picol_min, NULL);
    picolRegisterCmd(interp, "ne",       picol_EqNe, NULL);
    picolRegisterCmd(interp, "ni",       picol_InNi, NULL);
    picolRegisterCmd(interp, "pid",      picol_pid, NULL);
    picolRegisterCmd(interp, "proc",     picol_proc, NULL);
    picolRegisterCmd(interp, "rand",     picol_rand, NULL);
    picolRegisterCmd(interp, "rename",   picol_rename, NULL);
    picolRegisterCmd(interp, "return",   picol_return, NULL);
    picolRegisterCmd(interp, "scan",     picol_scan, NULL);
    picolRegisterCmd(interp, "set",      picol_set, NULL);
    picolRegisterCmd(interp, "split",    picol_split, NULL);
    picolRegisterCmd(interp, "string",   picol_string, NULL);
    picolRegisterCmd(interp, "subst",    picol_subst, NULL);
    picolRegisterCmd(interp, "switch",   picol_switch, NULL);
    picolRegisterCmd(interp, "time",     picol_time, NULL);
    picolRegisterCmd(interp, "try",      picol_try, NULL);
    picolRegisterCmd(interp, "unset",    picol_unset, NULL);
    picolRegisterCmd(interp, "uplevel",  picol_uplevel, NULL);
    picolRegisterCmd(interp, "variable", picol_variable, NULL);
    picolRegisterCmd(interp, "while",    picol_while, NULL);
    picolRegisterCmd(interp, "!",        picol_not, NULL);
    picolRegisterCmd(interp, "~",        picol_bitwise_not, NULL);
    picolRegisterCmd(interp, "_l",       picolLsort, NULL);
#if PICOL_FEATURE_ARRAYS
    picolRegisterCmd(interp, "array",    picol_array, NULL);
#endif
#if PICOL_FEATURE_GLOB
    picolRegisterCmd(interp, "glob",     picol_glob, NULL);
#endif
#if PICOL_FEATURE_INTERP
    picolRegisterCmd(interp, "interp",   picol_interp, NULL);
#endif
#if PICOL_FEATURE_IO
    picolRegisterCmd(interp, "cd",       picol_cd, NULL);
    picolRegisterCmd(interp, "close",    picolFileUtil, NULL);
    picolRegisterCmd(interp, "eof",      picolFileUtil, NULL);
    picolRegisterCmd(interp, "exec",     picol_exec, NULL);
    picolRegisterCmd(interp, "exit",     picol_exit, NULL);
    picolRegisterCmd(interp, "flush",    picolFileUtil, NULL);
    picolRegisterCmd(interp, "gets",     picol_gets, NULL);
    picolRegisterCmd(interp, "open",     picol_open, NULL);
    picolRegisterCmd(interp, "pwd",      picol_pwd, NULL);
    picolRegisterCmd(interp, "rawexec",  picol_exec, NULL);
    picolRegisterCmd(interp, "read",     picol_read, NULL);
    picolRegisterCmd(interp, "seek",     picolFileUtil, NULL);
    picolRegisterCmd(interp, "source",   picol_source, NULL);
    picolRegisterCmd(interp, "tell",     picolFileUtil, NULL);
#endif
#if PICOL_FEATURE_PUTS
    picolRegisterCmd(interp, "puts",     picol_puts, NULL);
#endif
}
picolInterp* picolCreateInterp(void) {
    return picolCreateInterp2(1, 1);
}
picolInterp* picolCreateInterp2(int register_core_cmds, int randomize) {
    picolInterp* interp = calloc(1, sizeof(picolInterp));
    char buf[8];
    int x;
    picolInitInterp(interp);
    if (register_core_cmds) {
        picolRegisterCoreCmds(interp);
    }
    picolSetVar(interp, "::errorInfo", "");
    if (randomize) {
        srand(time(NULL));
    }

    picolSetVar2(interp,
                 "tcl_platform(platform)",
                 PICOL_TCL_PLATFORM_PLATFORM_STRING,
                 1);
    picolSetVar2(interp,
                 "tcl_platform(engine)",
                 PICOL_TCL_PLATFORM_ENGINE_STRING, 1);

    /* The maximum Picol string length. Subtract one for the final '\0', which
       scripts don't see. */
    PICOL_SNPRINTF(buf, sizeof(buf), "%d", PICOL_MAX_STR - 1);
    picolSetVar2(interp, "tcl_platform(maxLength)", buf, 1);

    PICOL_SNPRINTF(buf, sizeof(buf), "%d", PICOL_MAX_LEVEL);
    picolSetVar2(interp, "tcl_platform(maxLevel)", buf, 1);

    x = 1;
    picolSetVar(interp,
                 "tcl_platform(byteOrder)",
                 ((char*) &x)[0] == x ? "littleEndian" : "bigEndian");
    PICOL_SNPRINTF(buf, sizeof(buf), "%ld", (long) sizeof(long));
    picolSetVar2(interp, "tcl_platform(wordSize)", buf, 1);
    PICOL_SNPRINTF(buf, sizeof(buf), "%ld", (long) sizeof(void *));
    picolSetVar2(interp, "tcl_platform(pointerSize)", buf, 1);
    return interp;
}

#endif /* PICOL_IMPLEMENTATION */
