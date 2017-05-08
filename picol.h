/* Tcl in ~ 500 lines of code by Salvatore antirez Sanfilippo. BSD licensed
 * 2007-04-01 Added by suchenwi: many more commands, see below.
 * 2016-2017  Misc. improvements and fixes by dbohdan. See Fossil timeline.
 *
 * This file provides both the interface and the implementation for Picol.
 * To instantiate the implementation put the line
 *     #define PICOL_IMPLEMENTATION
 * in a single source code file above where you include this file. To override
 * the default compilation options place a modified copy of the entire
 * PICOL_CONFIGURATION section starting below above where you include this file.
 */

/* -------------------------------------------------------------------------- */

#ifndef PICOL_CONFIGURATION
#define PICOL_CONFIGURATION

#define MAXSTR 4096
/* Optional features. Define as zero to disable. */
#define PICOL_FEATURE_ARRAYS  1
#define PICOL_FEATURE_GLOB    1
#define PICOL_FEATURE_INTERP  1
#define PICOL_FEATURE_IO      1
#define PICOL_FEATURE_PUTS    1

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

#define PICOL_PATCHLEVEL "0.2.4"

/* MSVC compatibility. */
#ifdef _MSC_VER
#   include <windows.h>
#   undef  PICOL_FEATURE_GLOB
#   define PICOL_FEATURE_GLOB 0
#   define MAXRECURSION 75
#   define PICOL_GETCWD _getcwd
#   define PICOL_GETPID GetCurrentProcessId
#   define PICOL_POPEN  _popen
#   define PICOL_PCLOSE _pclose
#   define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#else
#   include <unistd.h>
#   define MAXRECURSION 160
#   define PICOL_GETCWD getcwd
#   define PICOL_GETPID getpid
#   define PICOL_POPEN  popen
#   define PICOL_PCLOSE pclose
#endif

#if PICOL_FEATURE_GLOB
#   include <glob.h>
#endif

/* The value for ::tcl_platform(engine). */
#define TCL_PLATFORM_ENGINE_STRING "Picol"

/* The value for ::tcl_platform(platform). */
#define TCL_PLATFORM_UNKNOWN  0
#define TCL_PLATFORM_UNIX     1
#define TCL_PLATFORM_WINDOWS  2
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || \
        defined(_MSC_VER)
#    define TCL_PLATFORM_PLATFORM         TCL_PLATFORM_WINDOWS
#    define TCL_PLATFORM_PLATFORM_STRING  "windows"
#elif defined(_POSIX_VERSION)
#    define TCL_PLATFORM_PLATFORM         TCL_PLATFORM_UNIX
#    define TCL_PLATFORM_PLATFORM_STRING  "unix"
#else
#    define TCL_PLATFORM_PLATFORM         TCL_PLATFORM_UNKNOWN
#    define TCL_PLATFORM_PLATFORM_STRING  "unknown"
#endif

/* ------------------------ Most macros need the picol_ environment (argv, i) */
#define APPEND(dst, src) do {if ((strlen(dst)+strlen(src)) > sizeof(dst) - 1) {\
                        return picolErr(interp, "string too long");} \
                        strcat(dst, src);} while (0)

#define ARITY(x)        do {if (!(x)) {return picolErr1(interp, \
                        "wrong # args for \"%s\"", argv[0]);}} while (0);
#define ARITY2(x,s)     do {if (!(x)) {return picolErr1(interp, \
                        "wrong # args: should be \"%s\"", s);}} while (0)

#define COLONED(_p)     (*(_p) == ':' && *(_p+1) == ':') /* global indicator */
#define COMMAND(c)      int picol_##c(picolInterp* interp, int argc, \
                        char** argv, void* pd)
#define EQ(a,b)         ((*a)==(*b) && !strcmp(a, b))

#define FOLD(init,step,n) do {init; for(p=n;p<argc;p++) { \
                        SCAN_INT(a, argv[p]);step;}} while (0)

#define FOREACH(_v,_p,_s) \
                for(_p = picolParseList(_s,_v); _p; _p = picolParseList(_p,_v))

#define LAPPEND(dst, src) do {\
                        int needbraces = picolNeedsBraces(src); \
                        if(*dst!='\0') {APPEND(dst, " ");} \
                        if(needbraces) {APPEND(dst, "{");} \
                        APPEND(dst, src); \
                        if(needbraces) {APPEND(dst, "}");}} while (0)

/* this is the unchecked version, for functions without access to 'interp' */
#define LAPPEND_X(dst,src) do {\
                        int needbraces = picolNeedsBraces(src); \
                        if(*dst!='\0') strcat(dst, " "); \
                        if(needbraces) {strcat(dst, "{");} \
                        strcat(dst, src); \
                        if(needbraces) {strcat(dst, "}");}} while (0)

#define GET_VAR(_v,_n)    _v = picolGetVar(interp, _n); \
                          if(!_v) return picolErr1(interp, \
                          "can't read \"%s\": no such variable", _n);

#define PARSED(_t)        do {p->end = p->p-1; p->type = _t;} while (0)
#define RETURN_PARSED(_t) do {PARSED(_t); return PICOL_OK;} while (0)

#define SCAN_INT(v,x)     do {if (picolIsInt(x)) {v = atoi(x);} else {\
                          return picolErr1(interp, "expected integer " \
                          "but got \"%s\"", x);}} while (0)

#define SCAN_PTR(v,x)     do {void*_p; if ((_p=picolIsPtr(x))) {v = _p;} else \
                          {return picolErr1(interp, "expected pointer " \
                          "but got \"%s\"", x);}} while (0)

#define SUBCMD(x)         (EQ(argv[1], x))

enum {PICOL_OK, PICOL_ERR, PICOL_RETURN, PICOL_BREAK, PICOL_CONTINUE};
enum {PT_ESC, PT_STR, PT_CMD, PT_VAR, PT_SEP, PT_EOL, PT_EOF, PT_XPND};

/* ------------------------------------------------------------------- types */
typedef struct picolParser {
  char*  text;
  char*  p;           /* current text position */
  size_t len;         /* remaining length */
  char*  start;       /* token start */
  char*  end;         /* token end */
  int    type;        /* token type, PT_... */
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

typedef struct picolInterp {
  int             level;              /* Level of nesting */
  picolCallFrame* callframe;
  picolCmd*       commands;
  char*           current;        /* currently executed command */
  char*           result;
  int             trace; /* 1 to display each command, 0 if not */
} picolInterp;

#define DEFAULT_ARRSIZE 16

typedef struct picolArray {
  picolVar* table[DEFAULT_ARRSIZE];
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

char* picolArrGet(picolArray *ap, char* pat, char* buf, int mode);
char* picolArrStat(picolArray *ap, char* buf);
char* picolList(char* buf, int argc, char** argv);
int   picolNeedsBraces(char* str);
char* picolParseList(char* start, char* trg);
char* picolStrRev(char* str);
char* picolToLower(char* str);
char* picolToUpper(char* str);
COMMAND(abs);
#if TCL_PLATFORM_PLATFORM == TCL_PLATFORM_UNIX || \
    TCL_PLATFORM_PLATFORM == TCL_PLATFORM_WINDOWS
COMMAND(after);
#endif
COMMAND(append);
COMMAND(apply);
COMMAND(bitwise_not);
COMMAND(break);
COMMAND(catch);
COMMAND(clock);
COMMAND(concat);
COMMAND(continue);
COMMAND(error);
COMMAND(eval);
COMMAND(expr);
/* [file delete|exists|isdirectory|isfile|size] are enabled with
   PICOL_FEATURE_IO. */
COMMAND(file);
COMMAND(for);
COMMAND(foreach);
COMMAND(format);
COMMAND(global);
COMMAND(if);
COMMAND(incr);
COMMAND(info);
COMMAND(join);
COMMAND(lappend);
COMMAND(lassign);
COMMAND(lindex);
COMMAND(linsert);
COMMAND(list);
COMMAND(llength);
COMMAND(lmap);
COMMAND(lrange);
COMMAND(lrepeat);
COMMAND(lreplace);
COMMAND(lreverse);
COMMAND(lsearch);
COMMAND(lsort);
COMMAND(max);
COMMAND(min);
COMMAND(not);
COMMAND(pid);
COMMAND(proc);
COMMAND(rand);
COMMAND(rename);
COMMAND(return);
COMMAND(scan);
COMMAND(set);
COMMAND(split);
COMMAND(string);
COMMAND(subst);
COMMAND(switch);
COMMAND(trace);
COMMAND(try);
COMMAND(unset);
COMMAND(uplevel);
COMMAND(variable);
COMMAND(while);
#if PICOL_FEATURE_ARRAYS
    COMMAND(array);

    int picolHash(char* key, int modul);
    picolArray* picolArrCreate(picolInterp *interp, char *name);
    picolVar* picolArrGet1(picolArray* ap, char* key);
    picolVar* picolArrSet1(picolInterp *interp, char *name, char *value);
    picolVar* picolArrSet(picolArray* ap, char* key, char* value);
#endif
#if PICOL_FEATURE_GLOB
    COMMAND(glob);
#endif
#if PICOL_FEATURE_INTERP
    COMMAND(interp);
#endif
#if PICOL_FEATURE_IO
    COMMAND(cd);
    COMMAND(exec);
    COMMAND(exit);
    COMMAND(gets);
    COMMAND(open);
    COMMAND(pwd);
    COMMAND(read);
    COMMAND(source);

    int picolFileUtil(picolInterp *interp, int argc, char **argv, void *pd);
#endif
#if PICOL_FEATURE_PUTS
    COMMAND(puts);
#endif
int picolCallProc(picolInterp *interp, int argc, char **argv, void *pd);
int picolCondition(picolInterp *interp, char* str);
int picolErr1(picolInterp *interp, char* format, char* arg);
int picolErr(picolInterp *interp, char* str);
int picolEval2(picolInterp *interp, char *t, int mode);
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
int picolSetFmtResult(picolInterp* interp, char* fmt, int result);
int picolSetIntVar(picolInterp *interp, char *name, int value);
int picolSetResult(picolInterp *interp, char *s);
int picolSetVar2(picolInterp *interp, char *name, char *val, int glob);
int picolReplace(char* str, char* from, char* to, int nocase);
int picolSource(picolInterp *interp, char *filename);
int picolQuoteForShell(char* dest, int argc, char** argv);
int picolWildEq(char* pat, char* str, int n);
int qsort_cmp(const void* a, const void *b);
int qsort_cmp_decr(const void* a, const void *b);
int qsort_cmp_int(const void* a, const void *b);
picolCmd *picolGetCmd(picolInterp *interp, char *name);
picolInterp* picolCreateInterp(void);
picolInterp* picolCreateInterp2(int register_core_cmds, int randomize);
picolVar *picolGetVar2(picolInterp *interp, char *name, int glob);
void picolDropCallFrame(picolInterp *interp);
void picolEscape(char *str);
void picolInitInterp(picolInterp *interp);
void picolInitParser(picolParser *p, char *text);
void* picolIsPtr(char* str);
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
    p->type  = PT_EOL;
    p->expand = 0;
}
int picolParseSep(picolParser* p) {
    p->start = p->p;
    while (isspace(*p->p) || (*p->p=='\\' && *(p->p+1)=='\n')) {
        p->p++;
        p->len--;
    }
    RETURN_PARSED(PT_SEP);
}
int picolParseEol(picolParser* p) {
    p->start = p->p;
    while (isspace(*p->p) || *p->p == ';') {
        p->p++;
        p->len--;
    }
    RETURN_PARSED(PT_EOL);
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
    p->type = PT_CMD;
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
                p->type = PT_STR;
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
    int newword = (p->type == PT_SEP || p->type == PT_EOL || p->type == PT_STR);
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
            RETURN_PARSED(PT_ESC);
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
            RETURN_PARSED(PT_ESC);
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case ';':
            if (!p->insidequote) {
                RETURN_PARSED(PT_ESC);
            }
            break;
        case '"':
            if (p->insidequote) {
                p->end = p->p-1;
                p->type = PT_ESC;
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
    p->len--; /* skip the $ */
    if (*p->p == '{') {
        picolParseBrace(p);
        p->type = PT_VAR;
        return PICOL_OK;
    }
    if (COLONED(p->p)) {
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
        /* It's just a single char string "$" */
        picolParseString(p);
        p->start--;
        p->len++; /* back to the $ sign */
        p->type = PT_STR;
        return PICOL_OK;
    } else {
        RETURN_PARSED(PT_VAR);
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
    sprintf(buf, fmt, result);
    return picolSetResult(interp, buf);
}
int picolErr(picolInterp* interp, char* str) {
    char buf[MAXSTR] = "";
    picolCallFrame* cf;
    APPEND(buf, str);
    if (interp->current) {
        APPEND(buf, "\n    while executing\n\"");
        APPEND(buf, interp->current);
        APPEND(buf, "\"");
    }
    for (cf = interp->callframe; cf->command && cf->parent; cf = cf->parent) {
        APPEND(buf, "\n    invoked from within\n\"");
        APPEND(buf, cf->command);
        APPEND(buf, "\"");
    }
    /* Not exactly the same as in Tcl... */
    picolSetVar2(interp, "::errorInfo", buf, 1);
    picolSetResult(interp, str);
    return PICOL_ERR;
}
int picolErr1(picolInterp* interp, char* format, char* arg) {
    /* The format line should contain exactly one %s specifier. */
    char buf[MAXSTR];
    sprintf(buf, format, arg);
    return picolErr(interp, buf);
}
picolVar* picolGetVar2(picolInterp* interp, char* name, int glob) {
    picolVar* v = interp->callframe->vars;
    int global = COLONED(name);
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
        char buf[MAXSTR], buf2[MAXSTR], *cp, *cp2;
        /* array element syntax? */
        if ((cp = strchr(name, '('))) {
            picolArray* ap;
            int found = 0;        strncpy(buf, name, cp - name);
            buf[cp-name] = '\0';
            for (; v; v = v->next) {
                if (EQ(v->name, buf)) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                return NULL;
            }
            if (!((ap = picolIsPtr(v->val)))) {
                return NULL;
            }
            /* copy the key from after the opening paren */
            strcpy(buf2, cp +1);
            if (!((cp = strchr(buf2, ')')))) {
                return NULL;
            }
            /* overwrite closing paren */
            *cp = '\0';
            v = picolArrGet1(ap, buf2);
            if (!v) {
                if (global && EQ(buf, "env")) {
                    if (!((cp2 = getenv(buf2)))) {
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
    for (; v; v = v->next) {
        if (EQ(v->name, name)) {
            return v;
        }
    }
    return NULL;
}
int picolSetVar2(picolInterp* interp, char* name, char* val, int glob) {
    picolVar*       v = picolGetVar(interp, name);
    picolCallFrame* c = interp->callframe, *localc = c;
    int global = COLONED(name);
    if (glob||global) v = picolGetGlobalVar(interp, name);

    if (v) {
        /* existing variable case */
        if (v->val) {
            /* local value */
            free(v->val);
        } else {
            return picolSetGlobalVar(interp, name, val);
        }
    } else {
        /* nonexistent variable */
#if PICOL_FEATURE_ARRAYS
        if (strchr(name, '(')) {
            picolArrSet1(interp, name, val);
            return PICOL_OK;
        }
#endif
        if (glob || global) {
            if (global) name += 2;
            while (c->parent) c = c->parent;
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
    sprintf(buf, "%d", value);
    return picolSetVar(interp, name, buf);
}
int picolGetToken(picolInterp* interp, picolParser* p) {
    int rc;
    while (1) {
        if (!p->len) {
            if (p->type != PT_EOL && p->type != PT_EOF) {
                p->type = PT_EOL;
            } else {
                p->type = PT_EOF;
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
            if (p->type == PT_EOL) {
                picolParseComment(p);
                continue;
            }
        default:
            return picolParseString(p);
        }
    }
}
void picolInitInterp(picolInterp* interp) {
    interp->level     = 0;
    interp->callframe = calloc(1, sizeof(picolCallFrame));
    interp->result    = strdup("");
}
picolCmd* picolGetCmd(picolInterp* interp, char* name) {
    picolCmd* c;
    for (c = interp->commands; c; c = c->next) if (EQ(c->name, name)) return c;
    return NULL;
}
int picolRegisterCmd(picolInterp* interp, char* name, picol_Func f, void* pd) {
    picolCmd* c = picolGetCmd(interp, name);
    if (c) {
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
        LAPPEND_X(buf, argv[a]);
    }
    return buf;
}
char* picolParseList(char* start, char* trg) {
    char* cp = start;
    int bracelevel = 0, offset = 0, quoted = 0, done;
    if (!cp || *cp == '\0') {
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
    char buf[MAXSTR], *cp, *cp2;
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
int picolEval2(picolInterp* interp, char* t, int mode) { /*------------ EVAL! */
    /* mode==0: subst only, mode==1: full eval */
    picolParser p;
    int argc = 0, j;
    char**      argv = NULL, buf[MAXSTR*2];
    int tlen, rc = PICOL_OK;
    picolSetResult(interp, "");
    picolInitParser(&p, t);
    while (1) {
        int prevtype = p.type;
        picolGetToken(interp, &p);
        if (p.type == PT_EOF) {
            break;
        }
        tlen = p.end - p.start + 1;
        if (tlen < 0) {
            tlen = 0;
        }
        t = malloc(tlen+1);
        memcpy(t, p.start, tlen);
        t[tlen] = '\0';

        if (p.type == PT_VAR) {
            picolVar* v = picolGetVar(interp, t);
            if (v && !v->val) {
                v = picolGetGlobalVar(interp, t);
            }
            if (!v) {
                rc = picolErr1(interp,
                               "can't read \"%s\": no such variable",
                               t);
            }
            free(t);
            if (!v) {
                goto err;
            }
            t = strdup(v->val);
        } else if (p.type == PT_CMD) {
            rc = picolEval(interp, t);
            free(t);
            if (rc != PICOL_OK) {
                goto err;
            }
            t = strdup(interp->result);
        } else if (p.type == PT_ESC) {
            if (strchr(t, '\\')) {
                picolEscape(t);
            }
        } else if (p.type == PT_SEP) {
            prevtype = p.type;
            free(t);
            continue;
        }

        /* We have a complete command + args. Call it! */
        if (p.type == PT_EOL) {
            picolCmd* c;
            free(t);
            if (mode == 0) {
                /* do a quasi-subst only */
                rc = picolSetResult(interp, picolList(buf, argc, argv));
                /* not an error if rc == PICOL_OK */
                goto err;
            }
            prevtype = p.type;
            if (argc) {
                if ((c = picolGetCmd(interp, argv[0])) == NULL) {
                    if (EQ(argv[0], "") || *argv[0]=='#') {
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
                        rc = picolErr1(interp, "invalid command name \"%s\"",
                                argv[0]);
                        goto err;
                    }
                }
                if (interp->current) {
                    free(interp->current);
                }
                interp->current = strdup(picolList(buf, argc, argv));
                if (interp->trace) {
                    printf("< %d: %s\n", interp->level, interp->current);
                    fflush(stdout);
                }
                rc = c->func(interp, argc, argv, c->privdata);
                if (interp->trace) {
                    printf("> %d: {%s} -> {%s}\n",
                           interp->level,
                           picolList(buf, argc, argv),
                           interp->result);
                }
                if (rc != PICOL_OK) {
                    goto err;
                }
            }
            /* Prepare for the next command */
            for (j = 0; j < argc; j++) {
                free(argv[j]);
            }
            free(argv);
            argv = NULL;
            argc = 0;
            continue;
        }
        /* We have a new token, append to the previous or as new arg? */
        if (prevtype == PT_SEP || prevtype == PT_EOL) {
            if (!p.expand) {
                argv       = realloc(argv, sizeof(char*)*(argc+1));
                argv[argc] = t;
                argc++;
                p.expand = 0;
            } else if (strlen(t)) {
                char buf2[MAXSTR], *cp;
                FOREACH(buf2, cp, t) {
                    argv       = realloc(argv, sizeof(char*)*(argc+1));
                    argv[argc] = strdup(buf2);
                    argc++;
                }
                free(t);
                p.expand = 0;
            }
        } else if (p.expand) {
            /* slice in the words separately */
            char buf2[MAXSTR], *cp;
            FOREACH(buf2, cp, t) {
                argv       = realloc(argv, sizeof(char*)*(argc+1));
                argv[argc] = strdup(buf2);
                argc++;
            }
            free(t);
            p.expand = 0;
        } else {
            /* Interpolation */
            size_t oldlen = strlen(argv[argc-1]), tlen = strlen(t);
            argv[argc-1]  = realloc(argv[argc-1], oldlen+tlen+1);
            memcpy(argv[argc-1]+oldlen, t, tlen);
            argv[argc-1][oldlen+tlen] = '\0';
            free(t);
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
int picolCondition(picolInterp* interp, char* str) {
    if (str) {
        char buf[MAXSTR], buf2[MAXSTR], *argv[3], *cp;
        int a = 0, rc;
        rc = picolSubst(interp, str);
        if (rc != PICOL_OK) {
            return rc;
        }
        strcpy(buf2, interp->result);
        /* ------- try whether the format suits [expr]... */
        strcpy(buf, "llength ");
        LAPPEND(buf, interp->result);
        rc = picolEval(interp, buf);
        if (rc != PICOL_OK) {
            return rc;
        }
        if (EQ(interp->result, "3")) {
            FOREACH(buf, cp, buf2) {
                argv[a++] = strdup(buf);
            }
            if (picolGetCmd(interp, argv[1])) { /* defined operator in center */
                strcpy(buf, argv[1]); /* translate to Polish :) */
                LAPPEND(buf, argv[0]); /* e.g. {1 > 2} -> {> 1 2} */
                LAPPEND(buf, argv[2]);
                for (a = 0; a < 3; a++) {
                    free(argv[a]);
                }
                rc = picolEval(interp, buf);
                return rc;
            }
        } /* ... otherwise, check for inequality to zero */
        if (*str == '!') {
            /* allow !$x */
            strcpy(buf, "== 0 ");
            str++;
        } else {
            strcpy(buf, "!= 0 ");
        }
        strcat(buf, str);
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
    /* allow leading minus */
    if (*cp == '-') cp++;
    for (; *cp; cp++) {
        if (!isdigit(*cp)) {
            return 0;
        }
    }
    return 1;
}
void* picolIsPtr(char* str) {
    void* p = NULL;
    sscanf(str, "%p", &p);
    return (p && strlen(str) >= 3 ? p : NULL);
}
void picolDropCallFrame(picolInterp* interp) {
    picolCallFrame* cf = interp->callframe;
    picolVar* v, *next;
    for (v = cf->vars; v; v = next) {
        next = v->next;
        free(v->name);
        free(v->val);
        free(v);
    }
    if (cf->command) {
        free(cf->command);
    }
    interp->callframe = cf->parent;
    free(cf);
}
int picolCallProc(picolInterp* interp, int argc, char** argv, void* pd) {
    char** x=pd, *alist=x[0], *body=x[1], *p=strdup(alist), *tofree;
    char buf[MAXSTR];
    picolCallFrame* cf = calloc(1, sizeof(*cf));
    int a = 0, done = 0, errcode = PICOL_OK;
    if (!cf) {
        printf("could not allocate callframe\n");
        exit(1);
    }
    cf->parent   = interp->callframe;
    interp->callframe = cf;
    if (interp->level>MAXRECURSION) {
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
        if (EQ(start, "args") && done) {
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
    /* remove the called proc callframe */
    picolDropCallFrame(interp);
    interp->level--;
    return errcode;
arityerr:
    /* remove the called proc callframe */
    picolDropCallFrame(interp);
    interp->level--;
    return picolErr1(interp, "wrong # args for \"%s\"", argv[0]);
}
int picolWildEq(char* pat, char* str, int n) {
    /* allow '?' in pattern */
    for (; *pat && *str && n; pat++, str++, n--) {
        if (!(*pat==*str || *pat=='?')) {
            return 0;
        }
    }
    return (n==0 || *pat == *str || *pat == '?');
}
int picolMatch(char* pat, char* str) {
    size_t lpat  = strlen(pat)-1, res;
    char*  mypat = strdup(pat);
    if (*pat=='\0' && *str=='\0') {
        return 1;
    }
    /* strip last char */
    mypat[lpat] = '\0';
    if (*pat == '*') {
        if (pat[lpat] == '*') {
            /* <*>, <*xx*> case */
            res = (size_t)strstr(str, mypat+1);
            /* TODO: WildEq */
        } else {
            /* <*xx> case */
            res = picolWildEq(pat+1, str+strlen(str)-lpat, -1);
        }
    } else if (pat[lpat] == '*') {
        /* <xx*> case */
        res = picolWildEq(mypat, str, lpat);
    } else {
        /* <xx> case */
        res = picolWildEq(pat, str, -1);
    }
    free(mypat);
    return res;
}
int picolReplace(char* str, char* from, char* to, int nocase) {
    int strLen = strlen(str);
    int fromLen = strlen(from);
    int toLen = strlen(to);

    int fromIndex = 0;
    char result[MAXSTR] = "\0";
    char buf[MAXSTR] = "\0";
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
            /* Append to to result. */

            for (j = 0; j < toLen; j++) {
                result[resultLen + j] = to[j];
                if (resultLen + j >= MAXSTR - 1) {
                    break;
                }
            }
            resultLen += j;

            fromIndex = 0;
            bufLen = 0;
            count++;

            if (resultLen >= MAXSTR - 1) {
                break;
            }
        }
    }

    if (resultLen < MAXSTR - 1) {
        for (j = 0; j < bufLen; j++) {
            result[resultLen + j] = buf[j];
        }
        resultLen += bufLen;
        result[resultLen] = '\0';
    } else {
        result[MAXSTR - 1] = '\0';
    }

    strcpy(str, result);
    return count;
}
int picolQuoteForShell(char* dest, int argc, char** argv) {
    char command[MAXSTR] = "\0";
    unsigned int j;
    unsigned int k;
    unsigned int offset = 0;
#define ADDCHAR(c) do {command[offset] = c; offset++; \
                   if (offset >= sizeof(command) - 1) {return -1;}} while (0)
#if TCL_PLATFORM_PLATFORM == TCL_PLATFORM_WINDOWS
    /* See http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/
            04/23/everyone-quotes-arguments-the-wrong-way.aspx */
    int backslashes = 0;
    int m;
    int length;
    char command_unquoted[MAXSTR] = "\0";
    for (j = 1; j < argc; j++) {
        ADDCHAR(' ');
        length = strlen(argv[j]);
        if ((j > 1) &&
                strchr(argv[j], ' ') == NULL && \
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
            ADDCHAR('"');
            for (k = 0; k < length; k++) {
                backslashes = 0;
                while (argv[j][k] == '\\') {
                    backslashes++;
                    k++;
                }
                if (k == length) {
                    for (m = 0; m < backslashes * 2; m++) {
                        ADDCHAR('\\');
                    }
                    ADDCHAR(argv[j][k]);
                } else if (argv[j][k] == '"') {
                    for (m = 0; m < backslashes * 2 + 1; m++) {
                        ADDCHAR('\\');
                    }
                    ADDCHAR('"');
                } else {
                    for (m = 0; m < backslashes; m++) {
                        ADDCHAR('\\');
                    }
                    ADDCHAR(argv[j][k]);
                }
            }
            ADDCHAR('"');
        }
    }
    ADDCHAR('\0');
    memcpy(command_unquoted, command, offset);
    length = offset;
    offset = 0;
    /* Skip the first character, which is a space. */
    for (j = 1; j < length; j++) {
        ADDCHAR('^');
        ADDCHAR(command_unquoted[j]);
    }
    ADDCHAR('\0');
#else
    /* Assume a POSIXy platform. */
    for (j = 1; j < argc; j++) {
        ADDCHAR(' ');
        ADDCHAR('\'');
        for (k = 0; k < strlen(argv[j]); k++) {
            if (argv[j][k] == '\'') {
                ADDCHAR('\'');
                ADDCHAR('\\');
                ADDCHAR('\'');
                ADDCHAR('\'');
            } else {
                ADDCHAR(argv[j][k]);
            }
        }
        ADDCHAR('\'');
    }
    ADDCHAR('\0');
#endif
#undef ADDCHAR
    memcpy(dest, command, strlen(command));
    return 0;
}
/* ------------------------------------------- Commands in alphabetical order */
COMMAND(abs) {
    /* This is an example of how to wrap int functions. */
    int x;
    ARITY2(argc == 2, "abs int");
    SCAN_INT(x, argv[1]);
    return picolSetIntResult(interp, abs(x));
}
#if TCL_PLATFORM_PLATFORM == TCL_PLATFORM_UNIX || \
    TCL_PLATFORM_PLATFORM == TCL_PLATFORM_WINDOWS
#ifdef _MSC_VER
COMMAND(after) {
    unsigned int ms;
    ARITY2(argc == 2, "after ms");
    SCAN_INT(ms, argv[1]);
    Sleep(ms);
    return picolSetResult(interp, "");
}
#else
COMMAND(after) {
    unsigned int ms;
    struct timespec t, rem;
    ARITY2(argc == 2, "after ms");
    SCAN_INT(ms, argv[1]);
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
#endif
#endif
COMMAND(append) {
    picolVar* v;
    char buf[MAXSTR] = "";
    int a;
    ARITY2(argc > 1, "append varName ?value value ...?");
    v = picolGetVar(interp, argv[1]);
    if (v) {
        strcpy(buf, v->val);
    }
    for (a = 2; a < argc; a++) {
        APPEND(buf, argv[a]);
    }
    picolSetVar(interp, argv[1], buf);
    return picolSetResult(interp, buf);
}
COMMAND(apply) {
    char* procdata[2], *cp;
    char buf[MAXSTR], buf2[MAXSTR];
    ARITY2(argc >= 2, "apply {argl body} ?arg ...?");
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
        hash = (hash<<1) ^ *cp;
    }
    return hash % modul;
}
picolArray* picolArrCreate(picolInterp* interp, char* name) {
    char buf[MAXSTR];
    picolArray* ap = calloc(1, sizeof(picolArray));
    sprintf(buf, "%p", (void*)ap);
    picolSetVar(interp, name, buf);
    return ap;
}
char* picolArrGet(picolArray* ap, char* pat, char* buf, int mode) {
    int j;
    picolVar* v;
    for (j = 0; j < DEFAULT_ARRSIZE; j++) {
        for (v = ap->table[j]; v; v = v->next) {
            if (picolMatch(pat, v->name)) {
                /* mode==1: array names */
                LAPPEND_X(buf, v->name);
                if (mode==2) {
                    /* array get */
                    LAPPEND_X(buf, v->val);
                }
            }
        }
    }
    return buf;
}
picolVar* picolArrGet1(picolArray* ap, char* key) {
    int hash = picolHash(key, DEFAULT_ARRSIZE), found = 0;
    picolVar* pos = ap->table[hash], *v;
    for (v = pos; v; v = v->next) {
        if (EQ(v->name, key)) {
            found = 1;
            break;
        }
    }
    return (found ? v : NULL);
}
picolVar* picolArrSet(picolArray* ap, char* key, char* value) {
    int hash = picolHash(key, DEFAULT_ARRSIZE);
    picolVar* pos = ap->table[hash], *v;
    for (v = pos; v; v = v->next) {
        if (EQ(v->name, key)) {
            break;
        }
        pos = v;
    }
    if (v) {
        /* found exact key match */
        free(v->val);
    } else {
        /* create a new variable instance */
        v       = malloc(sizeof(*v));
        v->name = strdup(key);
        v->next = pos;
        ap->table[hash] = v;
        ap->size++;
    }
    v->val = strdup(value);
    return v;
}
picolVar* picolArrSet1(picolInterp* interp, char* name, char* value) {
    char buf[MAXSTR], *cp;
    picolArray* ap;
    picolVar*   v;
    cp = strchr(name, '(');
    strncpy(buf, name, cp-name);
    buf[cp-name] = '\0';
    v = picolGetVar(interp, buf);
    if (!v) {
        ap = picolArrCreate(interp, buf);
    } else {
        ap = picolIsPtr(v->val);
    }
    if (!ap) {
        return NULL;
    }
    strcpy(buf, cp+1);
    cp = strchr(buf, ')');
    if (!cp) {
        return NULL; /*picolErr1(interp, "bad array syntax %x", name);*/
    }
    /* overwrite closing paren */
    *cp = '\0';
    return picolArrSet(ap, buf, value);
}
char* picolArrStat(picolArray* ap, char* buf) {
    int a, buckets=0, j, count[11], depth;
    picolVar* v;
    char tmp[128];
    for (j = 0; j < 11; j++) {
        count[j] = 0;
    }
    for (a = 0; a < DEFAULT_ARRSIZE; a++) {
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
    sprintf(buf, "%d entries in table, %d buckets", ap->size, buckets);
    for (j=0; j<11; j++) {
        sprintf(tmp, "\nnumber of buckets with %d entries: %d", j, count[j]);
        strcat(buf, tmp);
    }
    return buf;
}
COMMAND(array) {
    picolVar*   v;
    picolArray* ap = NULL;
    char buf[MAXSTR] = "", buf2[MAXSTR], *cp;
    /* default: array size */
    int mode = 0;
    ARITY2(argc > 2,
            "array exists|get|names|set|size|statistics arrayName ?arg ...?");
    v = picolGetVar(interp, argv[2]);
    if (v) {
        SCAN_PTR(ap, v->val); /* caveat usor */
    }
    if (SUBCMD("exists")) {
        picolSetBoolResult(interp, v);
    }
    else if (SUBCMD("get") || SUBCMD("names") || SUBCMD("size")) {
        char* pat = "*";
        if (!v) {
            return picolErr1(interp, "no such array %s", argv[2]);
        }
        if (SUBCMD("size")) {
            return picolSetIntResult(interp, ap->size);
        }
        if (argc==4) {
            pat = argv[3];
        }
        if (SUBCMD("names")) {
            mode = 1;
        } else if (SUBCMD("get")) {
            mode = 2;
        } else if (argc != 3) {
            return picolErr(interp, "usage: array get|names|size a");
        }
        picolSetResult(interp, picolArrGet(ap, pat, buf, mode));
    } else if (SUBCMD("set")) {
        ARITY2(argc == 4, "array set arrayName list");
        if (!v) {
            ap = picolArrCreate(interp, argv[2]);
        }
        FOREACH(buf, cp, argv[3]) {
            cp = picolParseList(cp, buf2);
            if (!cp) {
                return picolErr(interp,
                                "list must have even number of elements");
            }
            picolArrSet(ap, buf, buf2);
        }
    } else if (SUBCMD("statistics")) {
        ARITY2(argc == 3, "array statistics arrname");
        if (!v) {
            return picolErr1(interp, "no such array %s", argv[2]);
        }
        picolSetResult(interp, picolArrStat(ap, buf));
    } else {
        return picolErr1(interp,
            "bad subcommand \"%s\": must be exists, get, set, size, or names",
            argv[1]);
    }
    return PICOL_OK;
}
#endif /* PICOL_FEATURE_ARRAYS */
COMMAND(break)    {
    ARITY(argc == 1);
    return PICOL_BREAK;
}
COMMAND(bitwise_not) {
    /* Implements the [~ int] command. */
    int res;
    ARITY2(argc == 2, "~ number");
    SCAN_INT(res, argv[1]);
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
COMMAND(concat) {
    char buf[MAXSTR];
    ARITY2(argc > 0, "concat ?arg...?");
    return picolSetResult(interp, picolConcat(buf, argc, argv));
}
COMMAND(continue) {
    ARITY(argc == 1);
    return PICOL_CONTINUE;
}
COMMAND(catch) {
    int rc;
    ARITY2(argc==2 || argc==3, "catch command ?varName?");
    rc = picolEval(interp, argv[1]);
    if (argc == 3) {
        picolSetVar(interp, argv[2], interp->result);
    }
    return picolSetIntResult(interp, rc);
}
#if PICOL_FEATURE_IO
COMMAND(cd) {
    ARITY2(argc == 2, "cd dirName");
    if (chdir(argv[1])) {
        return PICOL_ERR;
    }
    return PICOL_OK;
}
#endif
COMMAND(clock) {
    time_t t;
    ARITY2(argc > 1, "clock clicks|format|seconds ?arg..?");
    if (SUBCMD("clicks")) {
        picolSetIntResult(interp, clock());
    } else if (SUBCMD("format")) {
        SCAN_INT(t, argv[2]);
        if (argc==3 || (argc==5 && EQ(argv[3], "-format"))) {
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
            return picolErr(interp, "usage: clock format $t ?-format $fmt?");
        }
    } else if (SUBCMD("seconds")) {
        ARITY2(argc == 2, "clock seconds");
        picolSetIntResult(interp, (int)time(&t));
    } else {
        return picolErr(interp, "usage: clock clicks|format|seconds ..");
    }
    return PICOL_OK;
}
COMMAND(eval) {
    char buf[MAXSTR];
    ARITY2(argc >= 2, "eval arg ?arg ...?");
    return picolEval(interp, picolConcat(buf, argc, argv));
}
int picol_EqNe(picolInterp* interp, int argc, char** argv, void* pd) {
    int res;
    ARITY2(argc == 3, "eq|ne str1 str2");
    res = EQ(argv[1], argv[2]);
    return picolSetBoolResult(interp, EQ(argv[0], "ne") ? !res : res);
}
COMMAND(error) {
    ARITY2(argc == 2, "error message");
    return picolErr(interp, argv[1]);
}
#if PICOL_FEATURE_IO
COMMAND(exec) {
    /* This is far from the real thing, but it may be useful. */
    char command[MAXSTR] = "\0";
    char buf[256] = "\0";
    char output[MAXSTR] = "\0";
    FILE* fd;
    int status;
    int length;

    if (picolQuoteForShell(command, argc, argv) == -1) {
        picolSetResult(interp, "string too long");
        return PICOL_ERR;
    }

    fd = PICOL_POPEN(command, "r");
    if (fd == NULL) {
        return picolErr1(interp, "couldn't execute command \"%s\"", command);
    }

    while (fgets(buf, 256, fd)) {
        APPEND(output, buf);
    }
    status = PICOL_PCLOSE(fd);

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
COMMAND(exit) {
    int rc = 0;
    ARITY2(argc <= 2, "exit ?returnCode?");
    if (argc==2) {
        rc = atoi(argv[1]) & 0xFF;
    }
    exit(rc);
}
#endif
COMMAND(expr) {
    /* Only a simple case is supported: two or more operands with the same
    operator between them. */
    char buf[MAXSTR] = "";
    int a;
    ARITY2((argc%2)==0, "expr int1 op int2 ...");
    if (argc==2) {
        if (strchr(argv[1], ' ')) { /* braced expression - roll it out */
            strcpy(buf, "expr ");
            APPEND(buf, argv[1]);
            return picolEval(interp, buf);
        } else {
            return picolSetResult(interp, argv[1]); /* single scalar */
        }
    }
    APPEND(buf, argv[2]); /* operator first - Polish notation */
    LAPPEND(buf, argv[1]); /* {a + b + c} -> {+ a b c} */
    for (a = 3; a < argc; a += 2) {
        if (a < argc-1 && !EQ(argv[a+1], argv[2])) {
            return picolErr(interp, "need equal operators");
        }
        LAPPEND(buf, argv[a]);
    }
    return picolEval(interp, buf);
}
COMMAND(file) {
    char buf[MAXSTR] = "\0", *cp;
    int a;
    ARITY2(argc >= 3, "file option ?arg ...?");
    if (SUBCMD("dirname")) {
        cp = strrchr(argv[2], '/');
        if (cp) {
            *cp = '\0';
            cp = argv[2];
        } else cp = "";
        picolSetResult(interp, cp);
#if PICOL_FEATURE_IO
    } else if (SUBCMD("delete")) {
        int is_dir = picolIsDirectory(argv[2]);
        int del_result = 0;
        if (is_dir < 0) {
            if (is_dir == -2) {
                /* Tcl 8.x and Jim Tcl don't throw an error if the file
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
    } else if (SUBCMD("exists") || SUBCMD("size")) {
        FILE* fp = NULL;
        fp = fopen(argv[2], "r");
        if (SUBCMD("size")) {
            if (!fp) {
                return picolErr1(interp, "no file \"%s\"", argv[2]);
            }
            fseek(fp, 0, 2);
            picolSetIntResult(interp, ftell(fp));
        } else {
            picolSetBoolResult(interp, fp);
        }
        if (fp) {
            fclose(fp);
        }
    } else if (SUBCMD("isdir") || SUBCMD("isdirectory") || SUBCMD("isfile")) {
        int result = picolIsDirectory(argv[2]);
        if (result < 0) {
            picolSetBoolResult(interp, 0);
        } else {
            if (SUBCMD("isfile")) {
                result = !result;
            }
            picolSetBoolResult(interp, result);
        }
#endif
    } else if (SUBCMD("join")) {
        strcpy(buf, argv[2]);
        for (a=3; a<argc; a++) {
            if (EQ(argv[a], "")) {
                continue;
            }
            if ((picolMatch("/*", argv[a]) || picolMatch("?:/*", argv[a]))) {
                strcpy(buf, argv[a]);
            } else {
                if (!EQ(buf, "") && !picolMatch("*/", buf)) {
                    APPEND(buf, "/");
                }
                APPEND(buf, argv[a]);
            }
        }
        picolSetResult(interp, buf);
    } else if (SUBCMD("split")) {
        char fragment[MAXSTR];
        char* start = argv[2];
        if (*start == '/') {
            APPEND(buf, "/");
            start++;
        }
        while ((cp = strchr(start, '/'))) {
            memcpy(fragment, start, cp - start);
            fragment[cp - start] = '\0';
            LAPPEND(buf, fragment);
            start = cp + 1;
            while (*start == '/') {
                start++;
            }
        }
        if (strlen(start) > 0) {
            LAPPEND(buf, start);
        }
        picolSetResult(interp, buf);
    } else if (SUBCMD("tail")) {
        cp = strrchr(argv[2], '/');
        if (!cp) cp = argv[2]-1;
        picolSetResult(interp, cp+1);
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
    int offset = 0;
    ARITY(argc == 2 || argc==3);
    /* ARITY2 "close channelId"
       ARITY2 "eof channelId"
       ARITY2 "flush channelId"
       ARITY2 "seek channelId"
       ARITY2 "tell channelId" (for documentation only) */
    SCAN_PTR(fp, argv[1]);
    if (argc==3) {
        SCAN_INT(offset, argv[2]);
    }
    if (EQ(argv[0], "close")) {
        fclose(fp);
    } else if (EQ(argv[0], "eof")) {
        picolSetBoolResult(interp, feof(fp));
    } else if (EQ(argv[0], "flush")) {
        fflush(fp);
    } else if (EQ(argv[0], "seek") && argc==3) {
        fseek(fp, offset, SEEK_SET);
    } else if (EQ(argv[0], "tell")) {
        picolSetIntResult(interp, ftell(fp));
    } else {
        return picolErr(interp, "bad usage of FileUtil");
    }
    return PICOL_OK;
}
#endif
COMMAND(for) {
    int rc;
    ARITY2(argc == 5, "for start test next command");
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
            return PICOL_OK;
        }
    }
}
int picolLmap(picolInterp* interp, char* vars, char* list, char* body,
              int accumulate) {
    /* Only iterating over a single list is currently supported. */
    char buf[MAXSTR] = "", buf2[MAXSTR], result[MAXSTR] = "";
    char* cp, *varp;
    int rc, done=0;
    if (*list == '\0') {
        return PICOL_OK;          /* empty data list */
    }
    varp = picolParseList(vars, buf2);
    cp   = picolParseList(list, buf);
    while (cp || varp) {
        picolSetVar(interp, buf2, buf);
        varp = picolParseList(varp, buf2);
        if (!varp) {                        /* end of var list reached */
            rc = picolEval(interp, body);
            if (rc == PICOL_ERR || rc == PICOL_BREAK) {
                return rc;
            } else {
                if (accumulate) {
                    LAPPEND(result, interp->result);
                }
                varp = picolParseList(vars, buf2); /* cycle back to start */
            }
            done = 1;
        } else {
            done = 0;
        }
        if (cp) {
            cp = picolParseList(cp, buf);
        }
        if (!cp && done) {
            break;
        }
        if (!cp) {
            strcpy(buf, ""); /* empty string when data list exhausted */
        }
    }
    return picolSetResult(interp, result);
}
COMMAND(foreach) {
    ARITY2(argc == 4, "foreach varList list command");
    return picolLmap(interp, argv[1], argv[2], argv[3], 0);
}
COMMAND(format) {
    /* Limited to a single integer or string argument so far. */
    int value;
    unsigned int j = 0;
    int length = 0;
    char buf[MAXSTR];
    ARITY2(argc == 2 || argc == 3, "format formatString ?arg?");
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
        SCAN_INT(value, argv[2]);
        return picolSetFmtResult(interp, argv[1], value);
    case 's':
        sprintf(buf, argv[1], argv[2]);
        return picolSetResult(interp, buf);
    default:
        return picolErr1(interp, "bad format string \"%s\"", argv[1]);
    }
}
#if PICOL_FEATURE_IO
COMMAND(gets) {
    char buf[MAXSTR], *getsrc;
    FILE* fp = stdin;
    ARITY2(argc == 2 || argc == 3, "gets channelId ?varName?");
    picolSetResult(interp, "-1");
    if (!EQ(argv[1], "stdin")) {
        SCAN_PTR(fp, argv[1]); /* caveat usor */
    }
    if (!feof(fp)) {
        getsrc = fgets(buf, sizeof(buf), fp);
        if (feof(fp)) {
            buf[0] = '\0';
        } else {
            buf[strlen(buf)-1] = '\0'; /* chomp newline */
        }
        if (argc == 2) {
            picolSetResult(interp, buf);
        } else if (getsrc) {
            picolSetVar(interp, argv[2], buf);
            picolSetIntResult(interp, strlen(buf));
        }
    }
    return PICOL_OK;
}
#endif
#if PICOL_FEATURE_GLOB
COMMAND(glob) {
    /* implicit -nocomplain. */
    char buf[MAXSTR] = "\0";
    char file_path[MAXSTR] = "\0";
    char old_wd[MAXSTR] = "\0";
    char* new_wd;
    char* pattern;
    int append_slash = 0;
    glob_t pglob;
    size_t j;

    ARITY2(argc == 2 || argc == 4, "glob ?-directory directory? pattern");
    if (argc == 2) {
        pattern = argv[1];
    } else {
        if (!EQ(argv[1], "-directory") && !EQ(argv[1], "-dir")) {
            return picolErr1(interp,
                             "bad option \"%s\": must be -directory or -dir",
                             argv[1]);
        }
        PICOL_GETCWD(old_wd, MAXSTR);
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
            APPEND(file_path, new_wd);
            if (append_slash) {
                APPEND(file_path, "/");
            }
        }
        APPEND(file_path, pglob.gl_pathv[j]);
        LAPPEND(buf, file_path);
    }
    globfree(&pglob);
    /* Fix result corruption in MinGW 20130722. */
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
#endif
COMMAND(global) {
    ARITY2(argc > 1, "global varName ?varName ...?");
    if (interp->level > 0) {
        int a;
        for (a = 1; a < argc; a++) {
            picolVar* v = picolGetVar(interp, argv[a]);
            if (v) {
                return picolErr1(interp, "variable \"%s\" already exists",
                                 argv[a]);
            }
            picolSetVar(interp, argv[a], NULL);
        }
    }
    return PICOL_OK;
}
COMMAND(if) {
    int rc;
    ARITY2(argc==3 || argc==5, "if test script1 ?else script2?");
    if ((rc = picolCondition(interp, argv[1])) != PICOL_OK) {
        return rc;
    }
    if ((rc = atoi(interp->result))) {
        return picolEval(interp, argv[2]);
    } else if (argc == 5) {
        return picolEval(interp, argv[4]);
    }
    return picolSetResult(interp, "");
}
int picol_InNi(picolInterp* interp, int argc, char** argv, void* pd) {
    char buf[MAXSTR], *cp;
    int in = EQ(argv[0], "in");
    ARITY2(argc == 3, "in|ni element list");
    /* ARITY2 "ni element list" */
    FOREACH(buf, cp, argv[2])
    if (EQ(buf, argv[1])) {
        return picolSetBoolResult(interp, in);
    }
    return picolSetBoolResult(interp, !in);
}
COMMAND(incr) {
    int value = 0, increment = 1;
    picolVar* v;
    ARITY2(argc == 2 || argc == 3, "incr varName ?increment?");
    v = picolGetVar(interp, argv[1]);
    if (v && !v->val) {
        v = picolGetGlobalVar(interp, argv[1]);
    }
    if (v) {
        SCAN_INT(value, v->val);       /* creates if nonexistent */
    }
    if (argc == 3) {
        SCAN_INT(increment, argv[2]);
    }
    picolSetIntVar(interp, argv[1], value += increment);
    return picolSetIntResult(interp, value);
}
COMMAND(info) {
    char buf[MAXSTR] = "", *pat = "*";
    picolCmd* c = interp->commands;
    int procs = SUBCMD("procs");
    ARITY2(argc == 2 || argc == 3,
            "info args|body|commands|exists|globals|level|patchlevel|procs|"
            "script|vars");
    if (argc == 3) {
        pat = argv[2];
    }
    if (SUBCMD("vars") || SUBCMD("globals")) {
        picolCallFrame* cf = interp->callframe;
        picolVar*       v;
        if (SUBCMD("globals")) {
            while (cf->parent) cf = cf->parent;
        }
        for (v = cf->vars; v; v = v->next) {
            if (picolMatch(pat, v->name)) {
                LAPPEND(buf, v->name);
            }
        }
        picolSetResult(interp, buf);
    } else if (SUBCMD("args") || SUBCMD("body")) {
        if (argc==2) {
            return picolErr1(interp, "usage: info %s procname", argv[1]);
        }
        for (; c; c = c->next) {
            if (EQ(c->name, argv[2])) {
                char** pd = c->privdata;
                if (pd) {
                    return picolSetResult(interp,
                                          pd[(EQ(argv[1], "args") ? 0 : 1)]);
                } else {
                    return picolErr1(interp, "\"%s\" isn't a procedure",
                                     c->name);
                }
            }
        }
    } else if (SUBCMD("commands") || procs) {
        for (; c; c = c->next)
            if ((!procs||c->privdata)&&picolMatch(pat, c->name)) {
                LAPPEND(buf, c->name);
            }
        picolSetResult(interp, buf);
    } else if (SUBCMD("exists")) {
        if (argc != 3) {
            return picolErr(interp, "usage: info exists varName");
        }
        picolSetBoolResult(interp, picolGetVar(interp, argv[2]));
    } else if (SUBCMD("level")) {
        if (argc==2) {
            picolSetIntResult(interp, interp->level);
        } else if (argc==3) {
            int level;
            SCAN_INT(level, argv[2]);
            if (level==0) {
                picolSetResult(interp, interp->callframe->command);
            } else {
                return picolErr1(interp, "unsupported level %s", argv[2]);
            }
        }
    } else if (SUBCMD("patchlevel") || SUBCMD("pa")) {
        picolSetResult(interp, PICOL_PATCHLEVEL);
    } else if (SUBCMD("script")) {
        picolVar* v = picolGetVar(interp, "::_script_");
        if (v) {
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
COMMAND(interp) {
    picolInterp* src = interp, *trg = interp;
    if (SUBCMD("alias")) {
        picolCmd* c = NULL;
        ARITY2(argc==6, "interp alias slavePath slaveCmd masterPath masterCmd");
        if (!EQ(argv[2], "")) {
            SCAN_PTR(trg, argv[2]);
        }
        if (!EQ(argv[4], "")) {
            SCAN_PTR(src, argv[4]);
        }
        c = picolGetCmd(src, argv[5]);
        if (!c) {
            return picolErr(interp, "can only alias existing commands");
        }
        picolRegisterCmd(trg, argv[3], c->func, c->privdata);
        return picolSetResult(interp, argv[3]);
    } else if (SUBCMD("create")) {
        char buf[32];
        ARITY(argc == 2);
        trg = picolCreateInterp();
        sprintf(buf, "%p", (void*)trg);
        return picolSetResult(interp, buf);
    } else if (SUBCMD("eval")) {
        int rc;
        ARITY(argc == 4);
        SCAN_PTR(trg, argv[2]);
        rc = picolEval(trg, argv[3]);
        picolSetResult(interp, trg->result);
        return rc;
    } else {
        return picolErr(interp, "usage: interp alias|create|eval ...");
    }
}
#endif
COMMAND(join) {
    char buf[MAXSTR] = "", buf2[MAXSTR]="", *with = " ", *cp, *cp2 = NULL;
    ARITY2(argc == 2 || argc == 3, "join list ?joinString?");
    if (argc == 3) {
        with = argv[2];
    }
    for (cp=picolParseList(argv[1], buf); cp; cp=picolParseList(cp, buf)) {
        APPEND(buf2, buf);
        cp2 = buf2 + strlen(buf2);
        if (*with) {
            APPEND(buf2, with);
        }
    }
    if (cp2) {
        *cp2 = '\0'; /* remove last separator */
    }
    return picolSetResult(interp, buf2);
}
COMMAND(lappend) {
    char buf[MAXSTR] = "";
    int a;
    picolVar* v;
    ARITY2(argc >= 2, "lappend varName ?value value ...?");
    if ((v = picolGetVar(interp, argv[1]))) {
        strcpy(buf, v->val);
    }
    for (a = 2; a < argc; a++) {
        LAPPEND(buf, argv[a]);
    }
    picolSetVar(interp, argv[1], buf);
    return picolSetResult(interp, buf);
}
COMMAND(lassign) {
    char result[MAXSTR] = "", element[MAXSTR];
    char* cp;
    int i = 2;
    ARITY2(argc >= 2, "lassign list ?varName ...?");
    FOREACH(element, cp, argv[1]) {
        if (i < argc) {
            picolSetVar(interp, argv[i], element);
        } else {
            LAPPEND(result, element);
        }
        i++;
    }
    for (; i < argc; i++) {
        picolSetVar(interp, argv[i], "");
    }
    return picolSetResult(interp, result);
}
COMMAND(lindex) {
    char buf[MAXSTR] = "", *cp;
    int n = 0, idx;
    ARITY2((argc == 2) || (argc == 3), "lindex list [index]");
    /* Act as an identity function if no index is given. */
    if (argc == 2) {
        return picolSetResult(interp, argv[1]);
    }
    SCAN_INT(idx, argv[2]);
    for (cp=picolParseList(argv[1], buf); cp; cp=picolParseList(cp, buf), n++) {
        if (n==idx) {
            return picolSetResult(interp, buf);
        }
    }
    return PICOL_OK;
}
COMMAND(linsert) {
    char buf[MAXSTR] = "", buf2[MAXSTR]="", *cp;
    int pos = -1, j=0, a, atend=0;
    int inserted = 0;
    ARITY2(argc >= 3, "linsert list index element ?element ...?");
    if (!EQ(argv[2], "end")) {
        SCAN_INT(pos, argv[2]);
    } else {
        atend = 1;
    }
    FOREACH(buf, cp, argv[1]) {
        if (!inserted && !atend && pos==j) {
            for (a=3; a < argc; a++) {
                LAPPEND(buf2, argv[a]);
                inserted = 1;
            }
        }
        LAPPEND(buf2, buf);
        j++;
    }
    if (!inserted) {
        atend = 1;
    }
    if (atend) {
        for (a=3; a<argc; a++) {
            LAPPEND(buf2, argv[a]);
        }
    }
    return picolSetResult(interp, buf2);
}
COMMAND(list) {
    char buf[MAXSTR] = "";
    int a;
    /* ARITY2 "list ?value ...?" for documentation */
    for (a=1; a<argc; a++) {
        LAPPEND(buf, argv[a]);
    }
    return picolSetResult(interp, buf);
}
COMMAND(llength) {
    char buf[MAXSTR], *cp;
    int n = 0;
    ARITY2(argc == 2, "llength list");
    FOREACH(buf, cp, argv[1]) {
        n++;
    }
    return picolSetIntResult(interp, n);
}
COMMAND(lmap) {
    ARITY2(argc == 4, "lmap varList list command");
    return picolLmap(interp, argv[1], argv[2], argv[3], 1);
}
COMMAND(lrange) {
    char buf[MAXSTR] = "", buf2[MAXSTR] = "", *cp;
    int from, to, a = 0, toend = 0;
    ARITY2(argc == 4, "lrange list first last");
    SCAN_INT(from, argv[2]);
    if (EQ(argv[3], "end")) {
        toend = 1;
    } else {
        SCAN_INT(to, argv[3]);
    }
    FOREACH(buf, cp, argv[1]) {
        if (a>=from && (toend || a<=to)) {
            LAPPEND(buf2, buf);
        }
        a++;
    }
    return picolSetResult(interp, buf2);
}
COMMAND(lrepeat) {
    char result[MAXSTR] = "";
    int count, i, j;
    ARITY2(argc >= 2, "lrepeat count ?element ...?");
    SCAN_INT(count, argv[1]);
    for (i = 0; i < count; i++) {
        for (j = 2; j < argc; j++) {
            LAPPEND(result, argv[j]);
        }
    }
    return picolSetResult(interp, result);
}
COMMAND(lreplace) {
    char buf[MAXSTR] = "";
    char buf2[MAXSTR] = "";
    char* cp;
    int from, to = INT_MAX, a = 0, done = 0, j, toend = 0;
    ARITY2(argc >= 4, "lreplace list first last ?element element ...?");
    SCAN_INT(from, argv[2]);
    if (EQ(argv[3], "end")) {
        toend = 1;
    } else {
        SCAN_INT(to, argv[3]);
    }

    if (from < 0 && to < 0) {
        for (j = 4; j < argc; j++) {
            LAPPEND(buf2, argv[j]);
        }
        done = 1;
    }
    FOREACH(buf, cp, argv[1]) {
        if (a < from || (a > to && !toend)) {
            LAPPEND(buf2, buf);
        } else if (!done) {
            for (j = 4; j < argc; j++) {
                LAPPEND(buf2, argv[j]);
            }
            done = 1;
        }
        a++;
    }
    if (from > a) {
        return picolErr1(interp, "list doesn't contain element %s", argv[2]);
    }
    if (!done) {
        for (j = 4; j < argc; j++) {
            LAPPEND(buf2, argv[j]);
        }
    }

    return picolSetResult(interp, buf2);
}
COMMAND(lreverse) {
    char result[MAXSTR] = "", element[MAXSTR] = "";
    char* cp;
    int element_len, result_len = 0, i, needs_braces;
    ARITY2(argc == 2, "lreverse list");
    if (argv[1][0] == '\0') {
        return picolSetResult(interp, "");
    }
    FOREACH(element, cp, argv[1]) {
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
COMMAND(lsearch) {
    char buf[MAXSTR] = "", *cp;
    int j = 0;
    ARITY2(argc == 3, "lsearch list pattern");
    FOREACH(buf, cp, argv[1]) {
        if (picolMatch(argv[2], buf)) {
            return picolSetIntResult(interp, j);
        }
        j++;
    }
    return picolSetResult(interp, "-1");
}
COMMAND(lset) {
    char buf[MAXSTR] = "", buf2[MAXSTR]="", *cp;
    picolVar* var;
    int pos, a=0;
    ARITY2(argc == 4, "lset listVar index value");
    GET_VAR(var, argv[1]);
    if (!var->val) {
        var = picolGetGlobalVar(interp, argv[1]);
    }
    if (!var) {
        return picolErr1(interp, "no variable %s", argv[1]);
    }
    SCAN_INT(pos, argv[2]);
    FOREACH(buf, cp, var->val) {
        if (a==pos) {
            LAPPEND(buf2, argv[3]);
        } else {
            LAPPEND(buf2, buf);
        }
        a++;
    }
    if (pos < 0 || pos > a) {
        return picolErr(interp, "list index out of range");
    }
    picolSetVar(interp, var->name, buf2);
    return picolSetResult(interp, buf2);
}
/* ----------- sort functions for lsort ---------------- */
int qsort_cmp(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}
int qsort_cmp_decr(const void* a, const void* b) {
    return -strcmp(*(const char**)a, *(const char**)b);
}
int qsort_cmp_int(const void* a, const void* b) {
    int diff = atoi(*(const char**)a)-atoi(*(const char**)b);
    return (diff > 0 ? 1 : diff < 0 ? -1 : 0);
}
COMMAND(lsort) {
    char buf[MAXSTR] = "_l "; /* dispatch to helper function picolLsort */
    ARITY2(argc == 2 || argc == 3, "lsort ?-decreasing|-integer|-unique? list");
    APPEND(buf, argv[1]);
    if (argc==3) {
        APPEND(buf, " ");
        APPEND(buf, argv[2]);
    }
    return picolEval(interp, buf);
}
int picolLsort(picolInterp* interp, int argc, char** argv, void* pd) {
    char buf[MAXSTR] = "";
    char** av = argv+1;
    int ac = argc-1, a;
    if (argc<2) {
        return picolSetResult(interp, "");
    }
    if (argc>2 && EQ(argv[1], "-decreasing")) {
        qsort(++av, --ac, sizeof(char*), qsort_cmp_decr);
    } else if (argc>2 && EQ(argv[1], "-integer")) {
        qsort(++av, --ac, sizeof(char*), qsort_cmp_int);
    } else if (argc>2 && EQ(argv[1], "-unique")) {
        qsort(++av, --ac, sizeof(char*), qsort_cmp);
        for (a=0; a<ac; a++) {
            if (a==0 || !EQ(av[a], av[a-1])) {
                LAPPEND(buf, av[a]);
            }
        }
        return picolSetResult(interp, buf);
    } else {
        qsort(av, ac, sizeof(char*), qsort_cmp);
    }
    picolSetResult(interp, picolList(buf, ac, av));
    return PICOL_OK;
}
int picol_Math(picolInterp* interp, int argc, char** argv, void* pd) {
    int a = 0, b = 0, c = -1, p;
    if (argc>=2) {
        SCAN_INT(a, argv[1]);
    }
    if (argc==3) {
        SCAN_INT(b, argv[2]);
    }
    if (argv[0][0] == '/' && !b) {
        return picolErr(interp, "divide by zero");
    }
    /*ARITY2 "+ ?arg..." */
    if (EQ(argv[0], "+" )) {
        FOLD(c=0, c += a, 1);
    }
    /*ARITY2 "- arg ?apicolParseCmdrg...?" */
    else if (EQ(argv[0], "-" )) {
        if (argc==2) {
            c= -a;
        } else {
            FOLD(c=a, c -= a, 2);
        }
    }
    /*ARITY2"* ?arg...?" */
    else if (EQ(argv[0], "*" )) {
        FOLD(c=1, c *= a, 1);
    }
    /*ARITY2 "** a b" */
    else if (EQ(argv[0], "**" )) {
        ARITY(argc==3);
        c = 1;
        while (b-- > 0) {
            c = c*a;
        }
    }
    /*ARITY2 "/ a b" */
    else if (EQ(argv[0], "/" )) {
        ARITY(argc==3);
        c = a / b;
    }
    /*ARITY2"% a b" */
    else if (EQ(argv[0], "%" )) {
        ARITY(argc==3);
        c = a % b;
    }
    /*ARITY2"&& ?arg...?"*/
    else if (EQ(argv[0], "&&")) {
        FOLD(c=1, c = c && a, 1);
    }
    /*ARITY2"|| ?arg...?"*/
    else if (EQ(argv[0], "||")) {
        FOLD(c=0, c = c || a, 1);
    }
    /*ARITY2"& ?arg...?"*/
    else if (EQ(argv[0], "&")) {
        FOLD(c=INT_MAX, c = c & a, 1);
    }
    /*ARITY2"| ?arg...?"*/
    else if (EQ(argv[0], "|")) {
        FOLD(c=0, c = c | a, 1);
    }
    /*ARITY2"^ ?arg...?"*/
    else if (EQ(argv[0], "^")) {
        FOLD(c=0, c = c ^ a, 1);
    }
    /*ARITY2"<< a b" */
    else if (EQ(argv[0], "<<" )) {
        ARITY(argc==3);
        c = a << b;
    }
    /*ARITY2">> a b" */
    else if (EQ(argv[0], ">>" )) {
        ARITY(argc==3);
        c = a >> b;
    }
    /*ARITY2"> a b" */
    else if (EQ(argv[0], ">" )) {
        ARITY(argc==3);
        c = a > b;
    }
    /*ARITY2">= a b"*/
    else if (EQ(argv[0], ">=")) {
        ARITY(argc==3);
        c = a >= b;
    }
    /*ARITY2"< a b" */
    else if (EQ(argv[0], "<" )) {
        ARITY(argc==3);
        c = a < b;
    }
    /*ARITY2"<= a b"*/
    else if (EQ(argv[0], "<=")) {
        ARITY(argc==3);
        c = a <= b;
    }
    /*ARITY2"== a b"*/
    else if (EQ(argv[0], "==")) {
        ARITY(argc==3);
        c = a == b;
    }
    /*ARITY2"!= a b"*/
    else if (EQ(argv[0], "!=")) {
        ARITY(argc==3);
        c = a != b;
    }
    return picolSetIntResult(interp, c);
}
COMMAND(max) {
    int a, c, p;
    ARITY2(argc >= 2, "max number ?number ...?");
    SCAN_INT(a, argv[1]);
    FOLD(c = a, c = c > a ? c : a, 1);
    return picolSetIntResult(interp, c);
}
COMMAND(min) {
    int a, c, p;
    ARITY2(argc >= 2, "min number ?number ...?");
    SCAN_INT(a, argv[1]);
    FOLD(c = a, c = c < a ? c : a, 1);
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
COMMAND(not) {
    /* Implements the [! int] command. */
    int res;
    ARITY2(argc == 2, "! number");
    SCAN_INT(res, argv[1]);
    return picolSetBoolResult(interp, !res);
}
#if PICOL_FEATURE_IO
COMMAND(open) {
    char* mode = "r";
    FILE* fp = NULL;
    char fp_str[MAXSTR];
    ARITY2(argc == 2 || argc == 3, "open fileName ?access?");
    if (argc == 3) {
        mode = argv[2];
    }
    fp = fopen(argv[1], mode);
    if (!fp) {
        return picolErr1(interp, "could not open %s", argv[1]);
    }
    sprintf(fp_str, "%p", (void*)fp);
    return picolSetResult(interp, fp_str);
}
#endif
COMMAND(pid) {
    ARITY2(argc == 1, "pid");
    return picolSetIntResult(interp, PICOL_GETPID());
}
COMMAND(proc) {
    char** procdata = NULL;
    picolCmd* c = picolGetCmd(interp, argv[1]);
    ARITY2(argc == 4, "proc name args body");
    if (c) {
        procdata = c->privdata;
    }
    if (!procdata) {
        procdata = malloc(sizeof(char*)*2);
        if (c) {
            c->privdata = procdata;
            c->func = picolCallProc; /* may override C-coded commands */
        }
    }
    procdata[0] = strdup(argv[2]); /* arguments list */
    procdata[1] = strdup(argv[3]); /* procedure body */
    if (!c) {
        picolRegisterCmd(interp, argv[1], picolCallProc, procdata);
    }
    return PICOL_OK;
}
#if PICOL_FEATURE_PUTS
COMMAND(puts) {
    FILE* fp = stdout;
    char* chan=NULL, *str="", *fmt = "%s\n";
    int rc;
    ARITY2(argc >= 2 && argc <= 4, "puts ?-nonewline? ?channelId? string");
    if (argc==2) {
        str = argv[1];
    } else if (argc==3) {
        str = argv[2];
        if (EQ(argv[1], "-nonewline")) {
            fmt = "%s";
        } else {
            chan = argv[1];
        }
    } else { /* argc==4 */
        if (!EQ(argv[1], "-nonewline")) {
            return picolErr(interp, "usage: puts ?-nonewline? ?chan? string");
        }
        fmt = "%s";
        chan = argv[2];
        str = argv[3];
    }
    if (chan && !((EQ(chan, "stdout")) || EQ(chan, "stderr"))) {
#if PICOL_FEATURE_IO
        SCAN_PTR(fp, chan);
#else
        return picolErr1(interp,
                         "bad channel \"%s\": must be stdout, or stderr",
                         chan);
#endif
    }
    if (chan && EQ(chan, "stderr")) {
        fp = stderr;
    }
    rc = fprintf(fp, fmt, str);
    if (!rc || ferror(fp)) {
        return picolErr(interp, "channel is not open for writing");
    }
    return picolSetResult(interp, "");
}
#endif
#if PICOL_FEATURE_IO
COMMAND(pwd) {
    ARITY(argc == 1);
    char buf[MAXSTR] = "\0";
    return picolSetResult(interp, PICOL_GETCWD(buf, MAXSTR));
}
#endif
COMMAND(rand) {
    int n;
    ARITY2(argc == 2, "rand n (returns a random integer 0..<n)");
    SCAN_INT(n, argv[1]);
    return picolSetIntResult(interp, n ? rand()%n : rand());
}
#if PICOL_FEATURE_IO
COMMAND(read) {
    char buf[MAXSTR*2];
    int buf_size = sizeof(buf) - 1;
    int size = buf_size; /* Size argument value. */
    int actual_size = 0;
    FILE* fp = NULL;
    ARITY2(argc == 2 || argc == 3, "read channelId ?size?");
    SCAN_PTR(fp, argv[1]); /* caveat usor */
    if (argc == 3) {
        SCAN_INT(size, argv[2]);
        if (size > MAXSTR - 1) {
            return picolErr1(interp, "size %s too large", argv[2]);
        }
    }
    actual_size = fread(buf, 1, size, fp);
    if (actual_size > MAXSTR - 1) {
        return picolErr(interp, "read contents too long");
    } else {
        buf[actual_size] = '\0';
        return picolSetResult(interp, buf);
    }
}
#endif
COMMAND(rename) {
    int found = 0;
    picolCmd* c, *last = NULL;
    ARITY2(argc == 3, "rename oldName newName");
    for (c = interp->commands; c; last = c, c=c->next) {
        if (EQ(c->name, argv[1])) {
            if (!last && EQ(argv[2], "")) {
                interp->commands = c->next; /* delete first */
            } else if (EQ(argv[2], "")) {
                last->next = c->next; /* delete other */
            } else {
                c->name = strdup(argv[2]);   /* overwrite, do not free */
            }
            found = 1;
            break;
        }
    }
    if (!found) {
        return picolErr1(interp, "can't rename %s: no such command", argv[1]);
    }
    return PICOL_OK;
}
COMMAND(return ) {
    ARITY2(argc == 1 || argc == 2, "return ?result?");
    picolSetResult(interp, (argc == 2) ? argv[1] : "");
    return PICOL_RETURN;
}
COMMAND(scan) {
    /* Limited to one integer/character code so far. */
    int result, rc = 1;
    ARITY2(argc == 3 || argc == 4, "scan string formatString ?varName?");
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
COMMAND(set) {
    picolVar* pv;
    ARITY2(argc == 2 || argc == 3, "set varName ?newValue?");
    if (argc == 2) {
        GET_VAR(pv, argv[1]);
        if (pv && pv->val) {
            return picolSetResult(interp, pv->val);
        } else {
            pv = picolGetGlobalVar(interp, argv[1]);
        }
        if (!(pv && pv->val)) {
            return picolErr1(interp, "no value of \"%s\"\n", argv[1]);
        }
        return picolSetResult(interp, pv->val);
    } else {
        picolSetVar(interp, argv[1], argv[2]);
        return picolSetResult(interp, argv[2]);
    }
}
int picolSource(picolInterp* interp, char* filename) {
    char buf[MAXSTR*64];
    int rc;
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        return picolErr1(interp, "No such file or directory \"%s\"", filename);
    }
    picolSetVar(interp, "::_script_", filename);
    buf[fread(buf, 1, sizeof(buf), fp)] = '\0';
    fclose(fp);
    rc = picolEval(interp, buf);
    /* script only known during [source] */
    picolSetVar(interp, "::_script_", "");
    return rc;
}
#if PICOL_FEATURE_IO
COMMAND(source) {
    ARITY2(argc == 2, "source filename");
    return picolSource(interp, argv[1]);
}
#endif
COMMAND(split) {
    char* split = " ", *cp, *start;
    char buf[MAXSTR] = "", buf2[MAXSTR] = "";
    ARITY2(argc == 2 || argc == 3, "split string ?splitChars?");
    if (argc==3) {
        split = argv[2];
    }
    if (EQ(split, "")) {
        for (cp = argv[1]; *cp; cp++) {
            buf2[0] = *cp;
            LAPPEND(buf, buf2);
        }
    } else {
        for (cp = argv[1], start=cp; *cp; cp++) {
            if (strchr(split, *cp)) {
                strncpy(buf2, start, cp-start);
                buf2[cp-start] = '\0';
                LAPPEND(buf, buf2);
                buf2[0] = '\0';
                start = cp+1;
            }
        }
        LAPPEND(buf, start);
    }
    return picolSetResult(interp, buf);
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
COMMAND(string) {
    char buf[MAXSTR] = "\0";
    ARITY2(argc >= 3, "string option string ?arg..?");
    if (SUBCMD("length")) {
        picolSetIntResult(interp, strlen(argv[2]));
    } else if (SUBCMD("compare")) {
        ARITY2(argc == 4, "string compare s1 s2");
        picolSetIntResult(interp, strcmp(argv[2], argv[3]));
    } else if (SUBCMD("equal")) {
        ARITY2(argc == 4, "string equal s1 s2");
        picolSetIntResult(interp, EQ(argv[2], argv[3]));
    } else if (SUBCMD("first") || SUBCMD("last")) {
        int offset = 0, res = -1;
        char* cp;
        if (argc != 4 && argc != 5) {
            return picolErr1(interp,
                             "usage: string %s substr str ?index?",
                             argv[1]);
        }
        if (argc == 5) {
            SCAN_INT(offset, argv[4]);
        }
        if (SUBCMD("first")) {
            cp = strstr(argv[3]+offset, argv[2]);
        }
        else { /* last */
            char* cp2 = argv[3]+offset;
            while ((cp = strstr(cp2, argv[2]))) {
                cp2 = cp + 1;
            }
            cp = cp2 - 1;
        }
        if (cp) res = cp - argv[3];
        picolSetIntResult(interp, res);

    } else if (SUBCMD("index")) {
        int maxi = strlen(argv[2])-1, from;
        ARITY2(argc == 4, "string index string charIndex");
        SCAN_INT(from, argv[3]);
        if (from < 0) {
            from = 0;
        } else if (from > maxi) {
            from = maxi;
        }
        buf[0] = argv[2][from];
        picolSetResult(interp, buf);

    } else if (SUBCMD("map")) {
        char* charMap;
        char* str;
        char* cp;
        char from[MAXSTR] = "\0";
        char to[MAXSTR] = "\0";
        char result[MAXSTR] = "\0";
        int nocase = 0;

        if (argc == 4) {
            charMap = argv[2];
            str = argv[3];
        } else if (argc == 5 && EQ(argv[2], "-nocase")) {
            charMap = argv[3];
            str = argv[4];
            nocase = 1;
        } else {
            return picolErr(interp, "usage: string map ?-nocase? charMap str");
        }

        cp = charMap;
        strcpy(result, str);
        FOREACH(from, cp, charMap) {
            cp = picolParseList(cp, to);
            if (!cp) {
                return picolErr(interp, "char map list unbalanced");
            }
            if (EQ(from, "")) {
                continue;
            }
            picolReplace(result, from, to, nocase);
        }

        return picolSetResult(interp, result);
    } else if (SUBCMD("match")) {
        if (argc == 4)
            return picolSetBoolResult(interp, picolMatch(argv[2], argv[3]));
        else if (argc == 5 && EQ(argv[2], "-nocase")) {
            picolToUpper(argv[3]);
            picolToUpper(argv[4]);
            return picolSetBoolResult(interp, picolMatch(argv[3], argv[4]));
        } else {
            return picolErr(interp, "usage: string match pat str");
        }
    } else if (SUBCMD("is")) {
        ARITY2(argc == 4 && EQ(argv[2], "int"), "string is int str");
        picolSetBoolResult(interp, picolIsInt(argv[3]));
    } else if (SUBCMD("range")) {
        int from, to, maxi = strlen(argv[2]);
        ARITY2(argc == 5, "string range string first last");
        SCAN_INT(from, argv[3]);
        if (EQ(argv[4], "end")) {
            to = maxi;
        } else {
            SCAN_INT(to, argv[4]);
        }
        if (from < 0) {
            from = 0;
        } else if (from > maxi) {
            from = maxi;
        }
        if (to < 0) {
            to = 0;
        } else if (to > maxi) {
            to = maxi;
        }
        strncpy(buf, &argv[2][from], to-from+1);
        buf[to] = '\0';
        picolSetResult(interp, buf);

    } else if (SUBCMD("repeat")) {
        int j, n;
        SCAN_INT(n, argv[3]);
        ARITY2(argc == 4, "string repeat string count");
        for (j=0; j<n; j++) {
            APPEND(buf, argv[2]);
        }
        picolSetResult(interp, buf);

    } else if (SUBCMD("reverse")) {
        ARITY2(argc == 3, "string reverse str");
        picolSetResult(interp, picolStrRev(argv[2]));
    } else if (SUBCMD("tolower") && argc==3) {
        picolSetResult(interp, picolToLower(argv[2]));
    } else if (SUBCMD("toupper") && argc==3) {
        picolSetResult(interp, picolToUpper(argv[2]));
    } else if (SUBCMD("trim") || SUBCMD("trimleft") || SUBCMD("trimright")) {
        char* trimchars = " \t\n\r", *start, *end;
        int len;
        ARITY2(argc ==3 || argc == 4, "string trim?left|right? string ?chars?");
        if (argc==4) {
            trimchars = argv[3];
        }
        start = argv[2];
        if (!SUBCMD("trimright")) {
            for (; *start; start++) {
                if (strchr(trimchars, *start)==NULL) {
                    break;
                }
            }
        }
        end = argv[2]+strlen(argv[2]);
        if (!SUBCMD("trimleft")) {
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
COMMAND(subst) {
    ARITY2(argc == 2, "subst string");
    return picolSubst(interp, argv[1]);
}
COMMAND(switch) {
    char* cp, buf[MAXSTR] = "";
    int fallthrough = 0, a;
    ARITY2(argc > 2, "switch string pattern body ... ?default body?");
    if (argc==3) { /* braced body variant */
        FOREACH(buf, cp, argv[2]) {
            if (fallthrough || EQ(buf, argv[1]) || EQ(buf, "default")) {
                cp = picolParseList(cp, buf);
                if (!cp) {
                    return picolErr(interp,
                                    "switch: list must have an even number");
                }
                if (EQ(buf, "-")) {
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
            if (fallthrough || EQ(argv[a], argv[1]) || EQ(argv[a], "default")) {
                if (EQ(argv[a + 1], "-")) {
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
COMMAND(time) {
    int j, n = 1, rc;
    clock_t t0 = clock(), dt;
    ARITY2(argc==2 || argc==3, "time command ?count?");
    if (argc==3) SCAN_INT(n, argv[2]);
    for (j = 0; j < n; j++) {
        if ((rc = picolEval(interp, argv[1])) != PICOL_OK) {
            return rc;
        }
    }
    dt = (clock()-t0)*1000/n;
    return picolSetFmtResult(interp, "%d clicks per iteration", dt);
}
COMMAND(try) {
    int body_rc, handler_rc, finally_rc;
    char body_result[MAXSTR], handler_result[MAXSTR];
    ARITY2(argc == 2 || argc == 4 || argc == 6 || argc == 8,
           "try body ?on error varName handler? ?finally script?");
    /*        0    1   2     3       4       5         6      7 */
    if ((argc == 4) && !EQ(argv[2], "finally")) {
        return picolErr1(interp,
                         "bad argument \"%s\": expected \"finally\"",
                         argv[2]);
    }
    if ((argc == 6 || argc == 8) && !EQ(argv[2], "on")) {
        return picolErr1(interp,
                         "bad argument \"%s\": expected \"on\"",
                         argv[2]);
    }
    if ((argc == 6 || argc == 8) && !EQ(argv[3], "error")) {
        return picolErr1(interp,
                         "bad argument \"%s\": expected \"error\"",
                         argv[3]);
    }
    if ((argc == 8) && !EQ(argv[6], "finally")) {
        return picolErr1(interp,
                         "bad argument \"%s\": expected \"finally\"",
                         argv[6]);
    }
    body_rc = picolEval(interp, argv[1]);
    strcpy(body_result, interp->result);
    /* Run the error handler if we were given one and there was an error. */
    if ((argc == 6 || argc == 8) && body_rc == PICOL_ERR) {
        picolSetVar(interp, argv[4], interp->result);
        handler_rc = picolEval(interp, argv[5]);
        strcpy(handler_result, interp->result);
    }
    /* Run the "finally" script. If it fails, return its result. */
    if (argc == 4 || argc == 8) {
        finally_rc = picolEval(interp, argv[argc == 4 ? 3 : 7]);
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
COMMAND(trace) {
    ARITY2(argc==2 || argc == 6, "trace add|remove ?execution * mode cmd?");
    if (EQ(argv[1], "add")) {
        interp->trace = 1;
    } else if (EQ(argv[1], "remove")) {
        interp->trace = 0;
    } else {
        return picolErr1(interp, "bad option \"%s\": must be add, or remove",
                argv[1]);
    }
    return PICOL_OK;
}
COMMAND(unset) {
    picolVar* v, *lastv = NULL;
    ARITY2(argc == 2, "unset varName");
    for (v = interp->callframe->vars; v; lastv = v, v = v->next) {
        if (EQ(v->name, argv[1])) {
            if (!lastv) {
                interp->callframe->vars = v->next;
            } else {
                lastv->next = v->next; /* may need to free? */
            }
            break;
        }
    }
    return picolSetResult(interp, "");
}
COMMAND(uplevel) {
    char buf[MAXSTR];
    int rc, delta;
    picolCallFrame* cf = interp->callframe;
    ARITY2(argc >= 3, "uplevel level command ?arg...?");
    if (EQ(argv[1], "#0")) {
        delta = 9999;
    } else {
        SCAN_INT(delta, argv[1]);
    }
    for (; delta>0 && interp->callframe->parent; delta--) {
        interp->callframe = interp->callframe->parent;
    }
    rc = picolEval(interp, picolConcat(buf, argc-1, argv+1));
    interp->callframe = cf; /* back to normal */
    return rc;
}
COMMAND(variable) {
    char buf[MAXSTR]; /* limited to :: namespace so far */
    int a, rc = PICOL_OK;
    ARITY2(argc>1, "variable ?name value...? name ?value?");
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
COMMAND(while) {
    ARITY2(argc == 3, "while test command");
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
#if TCL_PLATFORM_PLATFORM == TCL_PLATFORM_UNIX || \
    TCL_PLATFORM_PLATFORM == TCL_PLATFORM_WINDOWS
    picolRegisterCmd(interp, "after",    picol_after, NULL);
#endif
    picolRegisterCmd(interp, "append",   picol_append, NULL);
    picolRegisterCmd(interp, "apply",    picol_apply, NULL);
    picolRegisterCmd(interp, "break",    picol_break, NULL);
    picolRegisterCmd(interp, "catch",    picol_catch, NULL);
    picolRegisterCmd(interp, "clock",    picol_clock, NULL);
    picolRegisterCmd(interp, "concat",   picol_concat, NULL);
    picolRegisterCmd(interp, "continue", picol_continue, NULL);
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
    picolRegisterCmd(interp, "trace",    picol_trace, NULL);
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
    /* Maximum string length. */
    char maxLength[8];
    /* Subtract one for the final '\0', which scripts don't see. */
    sprintf(maxLength, "%d", MAXSTR - 1);
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
                 TCL_PLATFORM_PLATFORM_STRING,
                 1);
    picolSetVar2(interp, "tcl_platform(engine)", TCL_PLATFORM_ENGINE_STRING, 1);
    picolSetVar2(interp, "tcl_platform(maxLength)", maxLength, 1);
    return interp;
}

#endif /* PICOL_IMPLEMENTATION */
