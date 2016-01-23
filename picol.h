/* declarations for Picol - a Tcl-like little scripting language
   Original design: Salvatore Sanfilippo. March 2007, BSD license
   Extended by: Richard Suchenwirth, 2007-..
*/
#ifndef PICOL_H
#define PICOL_H

#define PICOL_PATCHLEVEL "0.1.22"
#define MAXSTR 4096

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

#ifndef _MSC_VER
#   include <unistd.h>
#   define MAXRECURSION 160
#else
#   include <windows.h>
#   define MAXRECURSION 75
#   define getpid GetCurrentProcessId
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

/* ------------------- prototypes -- so far only some needed forward decl's */
picolVar*    picolArrGet1(picolArray *ap, char *key);
picolVar*    picolArrSet1(picolInterp *i, char *name, char *value);
picolInterp* picolCreateInterp(void);
void*        picolIsPtr(char* str);
int          picolSetVar2(picolInterp *i, char *name, char *val,int glob);

#endif
