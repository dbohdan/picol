/* declarations for Picol - a Tcl-like little scripting language
   Original design: Salvatore Sanfilippo. March 2007, BSD license
   Extended by: Richard Suchenwirth, 2007-..
   Refactored by: Danyil Bohdan, 2016.
*/
#ifndef PICOL_H
#define PICOL_H

#define PICOL_PATCHLEVEL "0.1.23"
#define MAXSTR 4096

#include <ctype.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _MSC_VER
#   include <windows.h>
#   define MAXRECURSION 75
#   define getpid GetCurrentProcessId
#else
#   include <unistd.h>
#   define MAXRECURSION 160
#endif

/* The value for ::tcl_platform(platform). */
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || \
        defined(_MSC_VER)
#    define TCL_PLATFORM_PLATFORM         windows
#    define TCL_PLATFORM_PLATFORM_STRING  "windows"
#elif defined(_POSIX_VERSION)
#    define TCL_PLATFORM_PLATFORM         unix
#    define TCL_PLATFORM_PLATFORM_STRING  "unix"
#else
#    define TCL_PLATFORM_PLATFORM         unknown
#    define TCL_PLATFORM_PLATFORM_STRING  "unknown"
#endif

/* -------------------------- Macros mostly need picol_ environment (argv,i) */
#define APPEND(dst,src) {if((strlen(dst)+strlen(src))>=sizeof(dst)-1) \
                        return picolErr(i, "string too long"); \
                        strcat(dst,src);}

#define ARITY(x)        if (!(x)) return picolErr1(i,"wrong # args for '%s'",argv[0]);
#define ARITY2(x,s)     if (!(x)) return picolErr1(i,"wrong # args: should be \"%s\"",s);

#define COLONED(_p)     (*(_p) == ':' && *(_p+1) == ':') /* global indicator */
#define COMMAND(c)      int picol_##c(picolInterp* i, int argc, char** argv, void* pd)
#define EQ(a,b)         ((*a)==(*b) && !strcmp(a,b))

#define FOLD(init,step,n) {init;for(p=n;p<argc;p++) {SCAN_INT(a,argv[p]);step;}}

#define FOREACH(_v,_p,_s) \
                 for(_p = picolParseList(_s,_v); _p; _p = picolParseList(_p,_v))

#define LAPPEND(dst,src) {int needbraces = (strchr(src,' ') || strlen(src)==0); \
                        if(*dst!='\0') APPEND(dst," "); if(needbraces) APPEND(dst,"{");\
                        APPEND(dst,src); if(needbraces) APPEND(dst,"}");}

/* this is the unchecked version, for functions without access to 'i' */
#define LAPPEND_X(dst,src) {int needbraces = (strchr(src,' ')!=NULL)||strlen(src)==0; \
                if(*dst!='\0') strcat(dst," "); if(needbraces) strcat(dst,"{"); \
                strcat(dst,src); if(needbraces) strcat(dst,"}");}

#define GET_VAR(_v,_n) _v = picolGetVar(i,_n); \
        if(!_v) return picolErr1(i,"can't read \"%s\": no such variable", _n);

#define PARSED(_t)        {p->end = p->p-1; p->type = _t;}
#define RETURN_PARSED(_t) {PARSED(_t);return PICOL_OK;}

#define SCAN_INT(v,x)     {if (picolIsInt(x)) v = atoi(x); else \
                          return picolErr1(i,"expected integer but got \"%s\"", x);}

#define SCAN_PTR(v,x)     {void*_p; if ((_p=picolIsPtr(x))) v = _p; else \
                          return picolErr1(i,"expected pointer but got \"%s\"", x);}

#define SUBCMD(x)         (EQ(argv[1],x))
#define QUOTE2(x)         #x
#define QUOTE(x)          QUOTE2(x)

enum {PICOL_OK, PICOL_ERR, PICOL_RETURN, PICOL_BREAK, PICOL_CONTINUE};
enum {PT_ESC,PT_STR,PT_CMD,PT_VAR,PT_SEP,PT_EOL,PT_EOF, PT_XPND};

/* ------------------------------------------------------------------- types */
typedef struct picolParser {
  char  *text;
  char  *p;           /* current text position */
  size_t len;         /* remaining length */
  char  *start;       /* token start */
  char  *end;         /* token end */
  int    type;        /* token type, PT_... */
  int    insidequote; /* True if inside " " */
  int    expand;      /* true after {*} */
} picolParser;

typedef struct picolVar {
  char            *name, *val;
  struct picolVar *next;
} picolVar;

struct picolInterp; /* forward declaration */
typedef int (*picol_Func)(struct picolInterp *i, int argc, char **argv, void *pd);

typedef struct picolCmd {
  char            *name;
  picol_Func       func;
  void            *privdata;
  struct picolCmd *next;
} picolCmd;

typedef struct picolCallFrame {
  picolVar              *vars;
  char                  *command;
  struct picolCallFrame *parent; /* parent is NULL at top level */
} picolCallFrame;

typedef struct picolInterp {
  int             level;              /* Level of nesting */
  picolCallFrame *callframe;
  picolCmd       *commands;
  char           *current;        /* currently executed command */
  char           *result;
  int             trace; /* 1 to display each command, 0 if not */
} picolInterp;

#define DEFAULT_ARRSIZE 16

typedef struct picolArray {
  picolVar *table[DEFAULT_ARRSIZE];
  int       size;
} picolArray;

/* Ease of use macros. */

#define picolEval(_i,_t)  picolEval2(_i,_t,1)
#define picolGetGlobalVar(_i,_n) picolGetVar2(_i,_n,1)
#define picolGetVar(_i,_n)       picolGetVar2(_i,_n,0)
#define picolSetBoolResult(i,x) picolSetFmtResult(i,"%d",!!x)
#define picolSetGlobalVar(_i,_n,_v) picolSetVar2(_i,_n,_v,1)
#define picolSetIntResult(i,x)  picolSetFmtResult(i,"%d",x)
#define picolSetVar(_i,_n,_v)       picolSetVar2(_i,_n,_v,0)
#define picolSubst(_i,_t) picolEval2(_i,_t,0)

/* prototypes */

char* picolArrGet(picolArray *ap, char* pat, char* buf, int mode);
char* picolArrStat(picolArray *ap,char* buf);
char* picolList(char* buf, int argc, char** argv);
char* picolParseList(char* start,char* trg);
char* picolStrRev(char *str);
char* picolToLower(char *str);
char* picolToUpper(char *str);
COMMAND(abs);
COMMAND(append);
COMMAND(apply);
COMMAND(array);
COMMAND(break);
COMMAND(catch);
COMMAND(cd);
COMMAND(clock);
COMMAND(concat);
COMMAND(continue);
COMMAND(error);
COMMAND(eval);
COMMAND(exec);
COMMAND(exit);
COMMAND(expr);
COMMAND(file);
COMMAND(for);
COMMAND(foreach);
COMMAND(format);
COMMAND(gets);
COMMAND(glob);
COMMAND(global);
COMMAND(if);
COMMAND(incr);
COMMAND(info);
COMMAND(interp);
COMMAND(join);
COMMAND(lappend);
COMMAND(lindex);
COMMAND(linsert);
COMMAND(list);
COMMAND(llength);
COMMAND(lrange);
COMMAND(lreplace);
COMMAND(lsearch);
COMMAND(lsort);
COMMAND(not);
COMMAND(open);
COMMAND(pid);
COMMAND(proc);
COMMAND(puts);
COMMAND(pwd);
COMMAND(rand);
COMMAND(read);
COMMAND(rename);
COMMAND(return);
COMMAND(scan);
COMMAND(set);
COMMAND(source);
COMMAND(split);
COMMAND(string);
COMMAND(subst);
COMMAND(switch);
COMMAND(trace);
COMMAND(unset);
COMMAND(uplevel);
COMMAND(variable);
COMMAND(while);
int picolCallProc(picolInterp *i, int argc, char **argv, void *pd);
int picolCondition(picolInterp *i, char* str);
int picolErr1(picolInterp *i, char* format, char* arg);
int picolErr(picolInterp *i, char* str);
int picolEval2(picolInterp *i, char *t, int mode);
int picolFileUtil(picolInterp *i, int argc, char **argv, void *pd);
int picolGetToken(picolInterp *i, picolParser *p);
int picolHash(char* key, int modul);
int picol_InNi(picolInterp *i, int argc, char **argv, void *pd);
int picolIsInt(char* str);
int picolLsort(picolInterp *i, int argc, char **argv, void *pd);
int picolMatch(char* pat, char* str);
int picol_Math(picolInterp *i, int argc, char **argv, void *pd);
int picolParseBrace(picolParser *p);
int picolParseCmd(picolParser *p);
int picolParseComment(picolParser *p);
int picolParseEol(picolParser *p);
int picolParseSep(picolParser *p);
int picolParseString(picolParser *p);
int picolParseVar(picolParser *p);
int picolRegisterCmd(picolInterp *i, char *name, picol_Func f, void *pd);
int picolSetFmtResult(picolInterp* i, char* fmt, int result);
int picolSetIntVar(picolInterp *i, char *name, int value);
int picolSetResult(picolInterp *i, char *s);
int picolSetVar2(picolInterp *i, char *name, char *val, int glob);
int picolSource(picolInterp *i,char *filename);
int picolQuoteForShell(char* dest, int argc, char** argv);
int picolWildEq(char* pat, char* str, int n);
int qsort_cmp(const void* a, const void *b);
int qsort_cmp_decr(const void* a, const void *b);
int qsort_cmp_int(const void* a, const void *b);
picolArray* picolArrCreate(picolInterp *i, char *name);
picolCmd *picolGetCmd(picolInterp *i, char *name);
picolInterp* picolCreateInterp(void);
picolVar* picolArrGet1(picolArray* ap, char* key);
picolVar* picolArrSet1(picolInterp *i, char *name, char *value);
picolVar* picolArrSet(picolArray* ap, char* key, char* value);
picolVar *picolGetVar2(picolInterp *i, char *name, int glob);
void picolDropCallFrame(picolInterp *i);
void picolEscape(char *str);
void picolInitInterp(picolInterp *i);
void picolInitParser(picolParser *p, char *text);
void* picolIsPtr(char* str);
void picolRegisterCoreCmds(picolInterp *i);

#endif
