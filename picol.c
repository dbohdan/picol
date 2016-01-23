/* Tcl in ~ 500 lines of code by Salvatore antirez Sanfilippo. BSD licensed */
/* 2007-04-01 Added by suchenwi: many more commands, see below */
/* Howto: gcc -O2 -Wall -o picol.exe picol.c */

#include "picol.h"

/* ----------------------------------------------------- parser functions */
void picolInitParser(picolParser *p, char *text) {
  p->text  = p->p = text;
  p->len   = strlen(text);
  p->start = 0; p->end = 0; p->insidequote = 0;
  p->type  = PT_EOL;
  p->expand = 0;
}
int picolParseSep(picolParser *p) {
  p->start = p->p;
  while(isspace(*p->p) || (*p->p=='\\' && *(p->p+1)=='\n')) {
    p->p++; p->len--;
  }
  RETURN_PARSED(PT_SEP);
}
int picolParseEol(picolParser *p) {
  p->start = p->p;
  while(isspace(*p->p) || *p->p == ';') { p->p++; p->len--; }
  RETURN_PARSED(PT_EOL);
}
int picolParseCmd(picolParser *p) {
  int level = 1, blevel = 0;
  p->start = ++p->p; p->len--;
  while (p->len) {
    if (*p->p == '[' && blevel == 0)       level++;
    else if (*p->p == ']' && blevel == 0) {if (!--level) break;}
    else if (*p->p == '\\')               {p->p++; p->len--;}
    else if (*p->p == '{')                 blevel++;
    else if (*p->p == '}')                {if (blevel != 0) blevel--;}
    p->p++; p->len--;
  }
  p->end  = p->p-1;
  p->type = PT_CMD;
  if (*p->p == ']') { p->p++; p->len--; }
  return PICOL_OK;
}
int picolParseBrace(picolParser *p) {
  int level = 1;
  p->start = ++p->p; p->len--;
  while(1) {
    if (p->len >= 2 && *p->p == '\\') {
      p->p++; p->len--;
    } else if (p->len == 0 || *p->p == '}') {
      level--;
      if (level == 0 || p->len == 0) {
        p->end = p->p-1;
        if (p->len) { p->p++; p->len--; } /* Skip final closed brace */
        p->type = PT_STR;
        return PICOL_OK;
      }
    } else if (*p->p == '{') level++;
    p->p++; p->len--;
  }
}
int picolParseString(picolParser *p) {
  int newword = (p->type == PT_SEP || p->type == PT_EOL || p->type == PT_STR);
  if(p->len >= 3 && !strncmp(p->p,"{*}",3)) {
    p->expand = 1;
    p->p += 3; p->len -= 3; /* skip the {*} expand indicator */
  }
  if (newword && *p->p == '{') {return picolParseBrace(p);}
  else if (newword && *p->p == '"') {
    p->insidequote = 1;
    p->p++; p->len--;
  }
  for(p->start = p->p; 1; p->p++, p->len--) {
    if (p->len == 0)       RETURN_PARSED(PT_ESC);
    switch(*p->p) {
    case '\\':             if (p->len >= 2) { p->p++; p->len--; }; break;
    case '$': case '[':    RETURN_PARSED(PT_ESC);
    case ' ': case '\t': case '\n': case '\r': case ';':
      if (!p->insidequote) RETURN_PARSED(PT_ESC);
      break;
    case '"':
      if (p->insidequote) {
        p->end = p->p-1;
        p->type = PT_ESC;
        p->p++; p->len--;
        p->insidequote = 0;
        return PICOL_OK;
      }
      break;
    }
  }
}
int picolParseVar(picolParser *p) {
  int parened = 0;
  p->start = ++p->p; p->len--; /* skip the $ */
  if (*p->p == '{') {
    picolParseBrace(p);
    p->type = PT_VAR;
    return PICOL_OK;
  }
  if(COLONED(p->p)) {p->p += 2; p->len -= 2;}
/*  while(isalnum(*p->p) || strchr("()_",*p->p)) { p->p++; p->len--; }*/
  while(isalnum(*p->p) || *p->p == '_' || *p->p=='('||*p->p==')') {
    if(*p->p=='(') parened = 1;
    p->p++; p->len--;
  }
  if(!parened && *(p->p-1)==')') {p->p--; p->len++;}
  if (p->start == p->p) { /* It's just a single char string "$" */
    picolParseString(p);
    p->start--; p->len++; /* back to the $ sign */
    p->type = PT_STR;
    return PICOL_OK;
  } else RETURN_PARSED(PT_VAR);
}
int picolParseComment(picolParser *p) {
  while(p->len && *p->p != '\n') {
    if(*p->p=='\\' && *(p->p+1)=='\n') {p->p++; p->len--;}
    p->p++; p->len--;
  }
  return PICOL_OK;
} /*------------------------------------ General functions: variables, errors */
int picolSetResult(picolInterp *i, char *s) {
  free(i->result);
  i->result = strdup(s);
  return PICOL_OK;
}
#define picolSetBoolResult(i,x) picolSetFmtResult(i,"%d",!!x)
#define picolSetIntResult(i,x)  picolSetFmtResult(i,"%d",x)
int picolSetFmtResult(picolInterp* i, char* fmt, int result) {
  char buf[32];
  sprintf(buf,fmt,result);
  return picolSetResult(i,buf);
}
int picolErr(picolInterp *i, char* str) {
  char buf[MAXSTR] = "";
  picolCallFrame *cf;
  APPEND(buf,str);
  if(i->current) {
    APPEND(buf,"\n    while executing\n\"");
    APPEND(buf,i->current); APPEND(buf,"\"");
  }
  for(cf = i->callframe; cf->command && cf->parent; cf = cf->parent) {
    APPEND(buf,"\n    invoked from within\n\"");
    APPEND(buf,cf->command); APPEND(buf,"\"");
  }
  picolSetVar2(i,"::errorInfo",buf,1); /* not exactly the same as in Tcl... */
  picolSetResult(i,str);
  return PICOL_ERR;
}
int picolErr1(picolInterp *i, char* format, char* arg) {
  /* 'format' should contain exactly one %s specifier */
  char buf[MAXSTR];
  sprintf(buf,format,arg);
  return picolErr(i,buf);
}
#define   picolGetVar(_i,_n)       picolGetVar2(_i,_n,0)
#define   picolGetGlobalVar(_i,_n) picolGetVar2(_i,_n,1)
picolVar *picolGetVar2(picolInterp *i, char *name, int glob) {
  picolVar *v = i->callframe->vars;
  int       global = COLONED(name);
  char buf[MAXSTR], buf2[MAXSTR], *cp, *cp2;
  if(global || glob) {
    picolCallFrame *c = i->callframe;
    while(c->parent) c = c->parent;
    v = c->vars;
    if(global) name += 2;  /* skip the "::" */
  }
  if((cp = strchr(name,'('))) { /* array element syntax? */
      picolArray* ap;
      int found = 0;
      strncpy(buf,name,cp-name);
      buf[cp-name] = '\0';
      for( ;v; v = v->next) if (EQ(v->name,buf)) {found = 1; break;}
      if(!found) return NULL;
      if(!((ap = picolIsPtr(v->val)))) return NULL;
      strcpy(buf2,cp+1); /* copy the key from after the opening paren*/
      if(!((cp = strchr(buf2,')')))) return NULL;
      *cp = '\0';       /* overwrite closing paren */
      v = picolArrGet1(ap,buf2);
      if(!v) {
        if(!((cp2 = getenv(buf2)))) return NULL;
        strcpy(buf,"::env("); strcat(buf,buf2); strcat(buf,")");
        return picolArrSet1(i, buf, cp2);
      }
      return v;
  }
  for( ;v; v = v->next) {if (EQ(v->name,name)) return v;}
  return NULL;
}
#define picolSetVar(_i,_n,_v)       picolSetVar2(_i,_n,_v,0)
#define picolSetGlobalVar(_i,_n,_v) picolSetVar2(_i,_n,_v,1)
int     picolSetVar2(picolInterp *i, char *name, char *val,int glob) {
  picolVar       *v = picolGetVar(i,name);
  picolCallFrame *c = i->callframe, *localc = c;
  int             global = COLONED(name);
  if(glob||global) v = picolGetGlobalVar(i,name);
  /*printf("SetVar (%s) (%s) %d @ %p\n", name,val,glob,v);*/
  if (v) {           /* existing variable case */
    if(v->val) {     /* local value */
      free(v->val);
    } else return picolSetGlobalVar(i,name,val);
  }
  if(!v) {           /* non-existing variable */
    if(strchr(name,'(')) {
        picolArrSet1(i,name,val);
        return PICOL_OK;
    }
    if(glob || global) {
      if(global) name += 2;
      while(c->parent) c = c->parent;
    }
    v       = malloc(sizeof(*v));
    v->name = strdup(name);
    v->next = c->vars;
    c->vars = v;
    i->callframe = localc;
  }
  v->val = (val? strdup(val) : NULL);
  return PICOL_OK;
}
int picolSetIntVar(picolInterp *i, char *name, int value) {
  char buf[32];
  sprintf(buf,"%d",value);
  return picolSetVar(i,name,buf);
}
int picolGetToken(picolInterp *i, picolParser *p) {
  int rc;
  while(1) {
    if (!p->len) {
      if (p->type != PT_EOL && p->type != PT_EOF)
        p->type = PT_EOL;
      else p->type = PT_EOF;
      return PICOL_OK;
    }
    switch(*p->p) {
    case ' ': case '\t':
      if (p->insidequote) return picolParseString(p);
      else                return picolParseSep(p);
    case '\n': case '\r': case ';':
      if (p->insidequote) return picolParseString(p);
      else                return picolParseEol(p);
    case '[': rc = picolParseCmd(p);
      if(rc == PICOL_ERR) return picolErr(i,"missing close-bracket");
      return rc;
    case '$':             return picolParseVar(p);
    case '#': if (p->type == PT_EOL) {picolParseComment(p);continue;}
    default:              return picolParseString(p);
    }
  }
}
void picolInitInterp(picolInterp *i) {
  i->level     = 0;
  i->callframe = calloc(1,sizeof(picolCallFrame));
  i->result    = strdup("");
}
picolCmd *picolGetCmd(picolInterp *i, char *name) {
  picolCmd *c;
  for(c = i->commands; c; c = c->next) if (EQ(c->name,name)) return c;
  return NULL;
}
int picolRegisterCmd(picolInterp *i, char *name, picol_Func f, void *pd) {
  picolCmd *c = picolGetCmd(i,name);
  if (c) return picolErr1(i,"command '%s' already defined", name);
  c = malloc(sizeof(picolCmd));
  c->name     = strdup(name);
  c->func     = f;
  c->privdata = pd;
  c->next     = i->commands;
  i->commands = c;
  return PICOL_OK;
}
char* picolList(char* buf, int argc, char** argv) {
  int a;
  buf[0] = '\0'; /* caller responsible for supplying large enough buffer */
  for(a=0; a<argc; a++) LAPPEND_X(buf,argv[a]);
  return buf;
}
char* picolParseList(char* start,char* trg) {
  char *cp = start;
  int bracelevel = 0, offset = 0, quoted = 0, done;
  if(!cp || *cp == '\0') return NULL;
  for(done=0; 1; cp++) {
    if(     *cp == '{') bracelevel++;
    else if(*cp == '}') bracelevel--;
    else if(bracelevel==0 && *cp == '\"') {
        if(quoted) {done = 1; quoted=0;}
        else       quoted = 1;
    } else if(bracelevel==0 &&!quoted && (*cp == ' ' || *cp=='\0')) done = 1;
    if(done && !quoted) {
      if(*start == '{')  {start++; offset = 1;}
      if(*start == '\"') {start++; offset = 1; cp++;}
      strncpy(trg, start, cp-start-offset);
      trg[cp-start-offset] = '\0';
      while(*cp == ' ') cp++;
      break;
    }
    if(!*cp) break;
  }
  return cp;
}
void picolEscape(char *str) {
  char buf[MAXSTR], *cp, *cp2;
  int ichar;
  for(cp = str, cp2 = buf; *cp; cp++) {
    if(*cp == '\\') {
      switch(*(cp+1)) {
      case 'n':  *cp2++ = '\n'; cp++; break;
      case 'r':  *cp2++ = '\r'; cp++; break;
      case 't':  *cp2++ = '\t'; cp++; break;
      case 'x':  sscanf(cp+2,"%x",&ichar);
                 *cp2++ = (char)ichar&0xFF; cp += 3; break;
      case '\\': *cp2++ = '\\'; cp++; break;
      case '\n': cp+=2; while(isspace(*cp)) cp++; cp--; break;
      default:  ; /* drop backslash */
      }
    } else *cp2++ = *cp;
  }
  *cp2 = '\0';
  strcpy(str, buf);
}
#define picolEval(_i,_t)  picolEval2(_i,_t,1)
#define picolSubst(_i,_t) picolEval2(_i,_t,0)
int     picolEval2(picolInterp *i, char *t, int mode) { /*----------- EVAL! */
  /* mode==0: subst only, mode==1: full eval */
  picolParser p;
  int         argc = 0, j;
  char      **argv = NULL, buf[MAXSTR];
  int         tlen, rc = PICOL_OK;
  picolSetResult(i,"");
  picolInitParser(&p,t);
  while(1) {
    int prevtype = p.type;
    picolGetToken(i, &p);
    if (p.type == PT_EOF) break;
    tlen = p.end - p.start + 1;
    if (tlen < 0) tlen = 0;
    t = malloc(tlen+1);
    memcpy(t, p.start, tlen);
    t[tlen] = '\0';
    /*printf("token (%s)\n", t);*/
    if (p.type == PT_VAR) {
      picolVar *v = picolGetVar(i,t);
      if (v && !v->val) v = picolGetGlobalVar(i,t);
      if(!v) rc = picolErr1(i,"can't read \"%s\": no such variable", t);
      free(t);
      if(!v) goto err;
      t = strdup(v->val);
    } else if (p.type == PT_CMD) {
      rc = picolEval(i,t);
      free(t);
      if (rc != PICOL_OK) goto err;
      t = strdup(i->result);
    } else if (p.type == PT_ESC) {
      if(strchr(t,'\\')) picolEscape(t);
    } else if (p.type == PT_SEP) {
      prevtype = p.type;
      free(t);
      continue;
    }
    /* We have a complete command + args. Call it! */
    if (p.type == PT_EOL) {
      picolCmd *c;
      free(t);
      if(mode==0) { /* do a quasi-subst only */
        rc = picolSetResult(i,strdup(picolList(buf,argc,argv)));
        goto err; /* not an error, if rc == PICOL_OK */
      }
      prevtype = p.type;
      if (argc) {
        if ((c = picolGetCmd(i,argv[0])) == NULL) {
          if(EQ(argv[0],"")||*argv[0]=='#') goto err;
          if((c = picolGetCmd(i,"unknown"))) {
            argv = realloc(argv, sizeof(char*)*(argc+1));
            for(j = argc; j > 0; j--) argv[j] = argv[j-1]; /* copy up */
            argv[0] = strdup("unknown");
            argc++;
          } else {
            rc = picolErr1(i,"invalid command name \"%s\"",argv[0]);
            goto err;
          }
        }
        if(i->current) free(i->current);
        i->current = strdup(picolList(buf,argc,argv));
        if(i->trace) {printf("< %d: %s\n",i->level,i->current); fflush(stdout);}
        rc = c->func(i,argc,argv,c->privdata);
        if(i->trace) {printf("> %d: {%s} -> {%s}\n",
                             i->level,picolList(buf,argc,argv),i->result);}
        if (rc != PICOL_OK) goto err;
      }
      /* Prepare for the next command */
      for (j = 0; j < argc; j++) free(argv[j]);
      free(argv); argv = NULL;
      argc = 0;
      continue;
    }
    /* We have a new token, append to the previous or as new arg? */
/*    printf("new token (%s), prevtype %d, expand %d\n",t,prevtype,p.expand);*/
    if (prevtype == PT_SEP || prevtype == PT_EOL) {
      if(!p.expand || strlen(t)) {
        argv       = realloc(argv, sizeof(char*)*(argc+1));
        argv[argc] = t;
        argc++;
        p.expand = 0;
      }
    } else if(p.expand) { /* slice in the words separately */
      char buf[MAXSTR], *cp;
      FOREACH(buf,cp,t) {
        argv       = realloc(argv, sizeof(char*)*(argc+1));
        argv[argc] = strdup(buf);
        argc++;
      }
      free(t);
      p.expand = 0;
    } else { /* Interpolation */
      size_t oldlen = strlen(argv[argc-1]), tlen = strlen(t);
      argv[argc-1]  = realloc(argv[argc-1], oldlen+tlen+1);
      memcpy(argv[argc-1]+oldlen, t, tlen);
      argv[argc-1][oldlen+tlen] = '\0';
      free(t);
    }
    prevtype = p.type;
  }
err:
  for (j = 0; j < argc; j++) free(argv[j]);
  free(argv);
  return rc;
}
int picolCondition(picolInterp *i, char* str) {
  if(str) {
    char buf[MAXSTR], buf2[MAXSTR], *argv[3], *cp;
    int a = 0, rc;
    rc = picolSubst(i,str);
    if(rc != PICOL_OK) return rc;
    /*printf("Condi: (%s) ->(%s)\n",str,i->result);*/
    strcpy(buf2,i->result);
    /* ------- try whether the format suits [expr]... */
    strcpy(buf,"llength "); LAPPEND(buf,i->result);
    rc = picolEval(i,buf);
    if(rc != PICOL_OK) return rc;
    if(EQ(i->result,"3")) {
      FOREACH(buf,cp,buf2) argv[a++] = strdup(buf);
      if(picolGetCmd(i,argv[1])) { /* defined operator in center */
        strcpy(buf,argv[1]);       /* translate to Polish :) */
        LAPPEND(buf,argv[0]);      /* e.g. {1 > 2} -> {> 1 2} */
        LAPPEND(buf,argv[2]);
        for(a = 0; a < 3; a++) free(argv[a]);
        rc = picolEval(i, buf);
        return rc;
      }
    } /* .. otherwise, check for inequality to zero */
    if(*str == '!') {strcpy(buf,"== 0 "); str++;} /* allow !$x */
    else             strcpy(buf,"!= 0 ");
    strcat(buf, str);
    return picolEval(i, buf);
  } else return picolErr(i, "NULL condition");
}
int picolIsInt(char* str) {
  char* cp = str;
  if(*cp == '-') cp++; /* allow leading minus */
  for(; *cp; cp++) {if(!isdigit(*cp)) return 0;}
  return 1;
}
void* picolIsPtr(char* str) {
  void* p = NULL;
  sscanf(str,"%p",&p);
  return (p && strlen(str)>=8? p: NULL);
}
void picolDropCallFrame(picolInterp *i) {
  picolCallFrame *cf = i->callframe;
  picolVar *v, *next;
  for(v = cf->vars; v; v = next) {
    next = v->next;
    free(v->name); free(v->val); free(v);
  }
  if(cf->command) free(cf->command);
  i->callframe = cf->parent;
  free(cf);
}
int picolCallProc(picolInterp *i, int argc, char **argv, void *pd) {
  char **x=pd, *alist=x[0], *body=x[1], *p=strdup(alist), *tofree;
  char buf[MAXSTR];
  picolCallFrame *cf = calloc(1,sizeof(*cf));
  int a = 0, done = 0, errcode = PICOL_OK;
  if(!cf) {printf("could not allocate callframe\n"); exit(1);}
  cf->parent   = i->callframe;
  i->callframe = cf;
  if(i->level>MAXRECURSION) return picolErr(i,"too many nested evaluations (infinite loop?)");
  i->level++;
  tofree = p;
  while(1) {
    char *start = p;
    while(*p != ' ' && *p != '\0') p++;
    if (*p != '\0' && p == start) { p++; continue; }
    if (p == start) break;
    if (*p == '\0') done=1; else *p = '\0';
    if(EQ(start,"args") && done) {
      picolSetVar(i,start,picolList(buf,argc-a-1,argv+a+1));
      a = argc-1;
      break;
    }
    if (++a > argc-1) goto arityerr;
    picolSetVar(i,start,argv[a]);
    p++;
    if (done) break;
  }
  free(tofree);
  if (a != argc-1) goto arityerr;
  cf->command = strdup(picolList(buf,argc,argv));
  errcode     = picolEval(i,body);
  if (errcode == PICOL_RETURN) errcode = PICOL_OK;
  picolDropCallFrame(i); /* remove the called proc callframe */
  i->level--;
  return errcode;
arityerr:
  picolDropCallFrame(i); /* remove the called proc callframe */
  i->level--;
  return picolErr1(i,"wrong # args for '%s'",argv[0]);
}
int picolWildEq(char* pat, char* str, int n) { /* allow '?' in pattern */
  for( ; *pat && *str && n; pat++, str++, n--)
    if(!(*pat==*str || *pat=='?')) return 0;
  return (n==0 || *pat == *str || *pat == '?');
}
int picolMatch(char* pat, char* str) {
  size_t lpat  = strlen(pat)-1, res;
  char  *mypat = strdup(pat);
  if(*pat=='\0' && *str=='\0') return 1;
  mypat[lpat] = '\0'; /* strip last char */
  if(*pat == '*') {
    if(pat[lpat] == '*') {                /* <*>,<*xx*> case */
      res = (size_t)strstr(str,mypat+1);  /* TODO: WildEq */
    } else                                    /* <*xx> case */
      res = picolWildEq(pat+1, str+strlen(str)-lpat,-1);
  } else if (pat[lpat] == '*') {              /* <xx*> case */
         res = picolWildEq(mypat,str,lpat);
  } else res = picolWildEq(pat,str,-1);        /* <xx> case */
  free(mypat);
  return res;
}
/* ------------------------------------------------ Commands in ABC.. order */
COMMAND(abs) {
  int x;     /* ---------- example for wrapping int functions */
  ARITY2(argc == 2, "abs int");
  SCAN_INT(x,argv[1]);
  return picolSetIntResult(i,abs(x));
}
COMMAND(append) {
  picolVar* v;
  char buf[MAXSTR] = "";
  int a;
  ARITY2(argc > 1, "append varName ?value value ...?");
  v = picolGetVar(i,argv[1]);
  if(v) strcpy(buf,v->val);
  for(a = 2; a < argc; a++) APPEND(buf,argv[a]);
  picolSetVar(i,argv[1],buf);
  return picolSetResult(i,buf);
}
COMMAND(apply) {
  char* procdata[2], *cp;
  char buf[MAXSTR], buf2[MAXSTR];
  ARITY2(argc >= 2, "apply {argl body} ?arg ...?");
  cp = picolParseList(argv[1],buf);
  picolParseList(cp,buf2);
  procdata[0] = buf;
  procdata[1] = buf2;
  return picolCallProc(i, argc-1, argv+1, (void*)procdata);
}
/*---------------------------------------------------- Array stuff */
int picolHash(char* key, int modul) {
  char *cp; int hash = 0;
  for(cp = key; *cp; cp++) hash = (hash<<1) ^ *cp;
  return hash % modul;
}
picolArray* picolArrCreate(picolInterp *i, char *name) {
    char buf[MAXSTR];
    picolArray *ap = calloc(1,sizeof(picolArray));
    sprintf(buf,"%p",ap);
    picolSetVar(i,name,buf);
    return ap;
}
char* picolArrGet(picolArray *ap, char* pat, char* buf, int mode) {
  int j;
  picolVar *v;
  for(j = 0; j < DEFAULT_ARRSIZE; j++) {
    for(v = ap->table[j]; v; v = v->next) {
      if (picolMatch(pat, v->name)) { /* mode==1: array names */
        LAPPEND_X(buf,v->name);
        if(mode==2) LAPPEND_X(buf,v->val); /* array get */
      }
    }
  }
  return buf;
}
picolVar* picolArrGet1(picolArray* ap, char* key) {
  int hash = picolHash(key,DEFAULT_ARRSIZE), found = 0;
  picolVar *pos = ap->table[hash], *v;
  for(v = pos; v; v = v->next) {
    if(EQ(v->name,key)) {found = 1; break;}
  }
  return (found? v : NULL);
}
picolVar* picolArrSet(picolArray* ap, char* key, char* value) {
  int hash = picolHash(key,DEFAULT_ARRSIZE);
  picolVar *pos = ap->table[hash], *v;
  for(v = pos; v; v = v->next) {
    if(EQ(v->name,key)) break;
    pos = v;
  }
  if (v) free(v->val);  /* found exact key match */
  else {       /* create a new variable instance */
    v       = malloc(sizeof(*v));
    v->name = strdup(key);
    v->next = pos;
    ap->table[hash] = v;
    ap->size++;
  }
  v->val  = strdup(value);
  return v;
}
picolVar* picolArrSet1(picolInterp *i, char *name, char *value) {
    char buf[MAXSTR], *cp;
    picolArray *ap;
    picolVar   *v;
    cp = strchr(name,'(');
    strncpy(buf,name,cp-name);
    buf[cp-name] = '\0';
    v = picolGetVar(i,buf);
    if(!v) ap = picolArrCreate(i,buf);
    else   ap = picolIsPtr(v->val);
    if(!ap) return NULL;
    strcpy(buf,cp+1);
    cp = strchr(buf,')');
    if(!cp) return NULL; /*picolErr1(i, "bad array syntax %x", name);*/
    *cp = '\0'; /* overwrite closing paren */
    return picolArrSet(ap, buf, value);
}
char* picolArrStat(picolArray *ap,char* buf) {
  int a, buckets=0, j, count[11], depth;
  picolVar *v;
  char     tmp[128];
  for(j = 0; j < 11; j++) count[j] = 0;
  for(a = 0; a < DEFAULT_ARRSIZE; a++) {
    depth = 0;
    if((v = ap->table[a])) {
      buckets++;
      depth = 1;
      while((v = v->next)) depth++;
      if(depth > 10) depth = 10;
    }
    count[depth]++;
  }
  sprintf(buf,"%d entries in table, %d buckets",ap->size,buckets);
  for(j=0; j<11; j++) {
    sprintf(tmp,"\nnumber of buckets with %d entries: %d",j,count[j]);
    strcat(buf,tmp);
  }
  return buf;
}
COMMAND(array) {
  picolVar   *v;
  picolArray *ap = NULL;
  char buf[MAXSTR] = "", buf2[MAXSTR], *cp;
  int mode = 0; /*default: array size */
  ARITY2(argc > 2, "array exists|get|names|set|size|statistics arrayName ?arg ...?");
  v = picolGetVar(i,argv[2]);
  if(v) SCAN_PTR(ap,v->val); /* caveat usor */
  if(       SUBCMD("exists")) picolSetBoolResult(i,(int)v);
  else if(  SUBCMD("get") || SUBCMD("names") || SUBCMD("size")) {
    char* pat = "*";
    if(!v) return picolErr1(i,"no such array %s", argv[2]);
    if(     SUBCMD("size"))   return picolSetIntResult(i,ap->size);
    if(argc==4) pat = argv[3];
    if(     SUBCMD("names")) mode = 1;
    else if(SUBCMD("get"))   mode = 2;
    else if(argc != 3) return picolErr(i,"usage: array get|names|size a");
    picolSetResult(i,picolArrGet(ap,pat,buf,mode));
  } else if(SUBCMD("set")) {
    ARITY2(argc == 4, "array set arrayName list")
    if(!v) ap = picolArrCreate(i,argv[2]);
    FOREACH(buf,cp,argv[3]) {
      cp = picolParseList(cp, buf2);
      if(!cp) return picolErr(i,"list must have even number of elements");
      picolArrSet(ap,buf,buf2);
    }
  } else if(SUBCMD("statistics")) {
    ARITY2(argc == 3, "array statistics arrname");
    if(!v) return picolErr1(i,"no such array %s", argv[2]);
    picolSetResult(i,picolArrStat(ap,buf));
  } else return picolErr1(i,
      "bad subcommand %s, must be: exists, get, set, size, or names",argv[1]);
  return PICOL_OK;
}
COMMAND(break)    {ARITY(argc == 1); return PICOL_BREAK;}
char* picolConcat(char* buf, int argc, char** argv) {
  int a;
  buf[0] = '\0'; /* caller responsible for supplying large enough buffer */
  for(a = 1; a < argc; a++) {
    strcat(buf,argv[a]);
    if(*argv[a] && a < argc-1) strcat(buf," ");
  }
  return buf;
}
COMMAND(concat) {
  char buf[MAXSTR];
  ARITY2(argc > 0, "concat ?arg...?")
  return picolSetResult(i,picolConcat(buf,argc,argv));
}
COMMAND(continue) {ARITY(argc == 1); return PICOL_CONTINUE;}
COMMAND(catch) {
  int rc;
  ARITY2(argc==2 || argc==3, "catch command ?varName?");
  rc = picolEval(i, argv[1]);
  if(argc == 3) picolSetVar(i,argv[2],i->result);
  return picolSetIntResult(i,rc);
}
COMMAND(clock) {
  time_t t;
  ARITY2(argc > 1, "clock clicks|format|seconds ?arg..?")
    if(     SUBCMD("clicks")) picolSetIntResult(i,clock());
    else if(SUBCMD("format")) {
      SCAN_INT(t, argv[2]);
      if (argc==3 || (argc==5 && EQ(argv[3],"-format"))) {
        char buf[128], *cp;
        struct tm* mytm = localtime(&t);
        if(argc==3) cp = "%a %b %d %H:%M:%S %Y"; else cp = argv[4];
        strftime(buf,sizeof(buf),cp,mytm);
        picolSetResult(i,buf);
      } else return picolErr(i,"usage: clock format $t ?-format $fmt?");
    } else if(SUBCMD("seconds")) {
        ARITY2(argc == 2, "clock seconds");
        picolSetIntResult(i,(int)time(&t));
    } else return picolErr(i,"usage: clock clicks|format|seconds ..");
  return PICOL_OK;
}
COMMAND(eval) {
  char buf[MAXSTR];
  ARITY2(argc >= 2, "eval arg ?arg ...?");
  return picolEval(i, picolConcat(buf,argc,argv));
}
int picol_EqNe(picolInterp *i, int argc, char **argv, void *pd) {
  int res;
  ARITY2(argc == 3, "eq|ne str1 str2")
  /* ARITY2(argc == 3, "ne str1 str2") */
  res = EQ(argv[1],argv[2]);
  return picolSetBoolResult(i, EQ(argv[0],"ne")? !res : res);
}
COMMAND(error) {
  ARITY2(argc == 2, "error message")
  return picolErr(i,argv[1]);
}
COMMAND(exec) {
  char buf[MAXSTR]; /* This is far from the real thing, but may be useful */
  ARITY2(argc > 1, "exec command ?arg...?")
  fflush(stdout);
  system(picolConcat(buf,argc,argv));
  return picolSetResult(i,"");
}
COMMAND(exit) {
  int rc = 0;
  ARITY2(argc <= 2, "exit ?returnCode?");
  if(argc==2) rc = atoi(argv[1]) & 0xFF;
  exit(rc);
}
COMMAND(expr) {
  char buf[MAXSTR] = "";        /* only simple cases supported */
  int a;
  ARITY2((argc%2)==0, "expr int1 op int2 ...");
  if(argc==2) {
    if(strchr(argv[1],' ')) { /* braced expression - roll it out */
        strcpy(buf,"expr ");
        APPEND(buf,argv[1]);
        return picolEval(i,buf);
    } else return picolSetResult(i,argv[1]); /* single scalar */
  }
  APPEND(buf,argv[2]);   /* operator first - Polish notation */
  LAPPEND(buf,argv[1]);  /* {a + b + c} -> {+ a b c} */
  for(a = 3; a < argc; a += 2) {
    if(a < argc-1 && !EQ(argv[a+1],argv[2]))
      return picolErr(i,"need equal operators");
    LAPPEND(buf,argv[a]);
  }
  return picolEval(i, buf);
}
COMMAND(file) {
  char buf[MAXSTR], *cp;
  FILE* fp = NULL;
  int a;
  ARITY2(argc >= 3, "file option ?arg ...?");
  if(SUBCMD("dirname")) {
    cp = strrchr(argv[2],'/');
    if(cp) {*cp = '\0'; cp = argv[2];}
    else cp = "";
    picolSetResult(i,cp);
  } else if(SUBCMD("exists") || SUBCMD("size")) {
    fp = fopen(argv[2],"r");
    if(SUBCMD("size")) {
      if(!fp) return picolErr1(i,"no file '%s'",argv[2]);
      fseek(fp,0,2);
      picolSetIntResult(i,ftell(fp));
    } else picolSetBoolResult(i,(int)fp);
    if(fp) fclose(fp);
  } else if(SUBCMD("join")) {
    strcpy(buf,argv[2]);
    for(a=3; a<argc; a++) {
        if(picolMatch("/*",argv[a]) || picolMatch("?:/*",argv[a]))
            strcpy(buf,argv[a]);
        else {APPEND(buf,"/"); APPEND(buf,argv[a]);}
    }
    picolSetResult(i,buf);
  } else if(SUBCMD("tail")) {
    cp = strrchr(argv[2],'/');
    if(!cp) cp = argv[2]-1;
    picolSetResult(i,cp+1);        
  } else return picolErr(i,"usage: file dirname|exists|size|tail $filename");
  return PICOL_OK;
}
int picolFileUtil(picolInterp *i, int argc, char **argv, void *pd) {
  FILE* fp = NULL;
  int offset = 0;
  ARITY(argc == 2 || argc==3);
    /* ARITY2 "close channelId"
       ARITY2 "eof channelId"
       ARITY2 "flush channelId"
       ARITY2 "seek channelId"
       ARITY2 "tell channelId" (for documentation only) */
  SCAN_PTR(fp,argv[1]);
  if(argc==3) SCAN_INT(offset,argv[2]);
  if     (EQ(argv[0],"close")) fclose(fp);
  else if(EQ(argv[0],"eof"))   picolSetBoolResult(i,feof(fp));
  else if(EQ(argv[0],"flush")) fflush(fp);
  else if(EQ(argv[0],"seek") && argc==3) fseek(fp,offset,SEEK_SET);
  else if(EQ(argv[0],"tell"))  picolSetIntResult(i,ftell(fp));
  else return picolErr(i,"bad usage of FileUtil");
  return PICOL_OK;
}
COMMAND(for) {
  int rc;
  ARITY2(argc == 5, "for start test next command")
    if ((rc = picolEval(i,argv[1])) != PICOL_OK) return rc; /* init */
  while(1) {
    rc = picolEval(i,argv[4]);                /* body */
    if (rc == PICOL_BREAK) return PICOL_OK;
    if (rc == PICOL_ERR)   return rc;
    rc = picolEval(i,argv[3]);                /* step */
    if (rc != PICOL_OK)    return rc;
    rc = picolCondition(i,argv[2]);           /* condition */
    if (rc != PICOL_OK)    return rc;
    if(atoi(i->result)==0) return PICOL_OK;
  }
}
COMMAND(foreach) {
  char buf[MAXSTR] = "", buf2[MAXSTR]; /* only single list supported */
  char *cp, *varp;
  int rc, done=0;
  ARITY2((argc%2)==0, "foreach varList list ?varList list ...? command");
  if(*argv[2] == '\0') return PICOL_OK;            /* empty data list */
  varp = picolParseList(argv[1],buf2);
  cp   = picolParseList(argv[2],buf);
  while(cp || varp) {
    picolSetVar(i,buf2,buf);
    varp = picolParseList(varp,buf2);
    if(!varp) {                            /* end of var list reached */
      rc = picolEval(i,argv[argc-1]);
      if(rc == PICOL_ERR || rc == PICOL_BREAK) return rc;
      else varp = picolParseList(argv[1],buf2); /* cycle back to start */
      done = 1;
    } else done=0;
    if(cp)  cp = picolParseList(cp,buf);
    if(!cp && done) break;
    if(!cp) strcpy(buf,"");   /* empty string when data list exhausted */
  }
  return picolSetResult(i,"");
}
COMMAND(format) {
  int value;     /* limited to single integer or string argument so far */
  ARITY2(argc == 2 || argc == 3, "format formatString ?arg?")
  if(argc==2) return picolSetResult(i, argv[1]); /* identity */
  if(strchr(argv[1],'s')) {
    char buf[MAXSTR];
    sprintf(buf,argv[1],argv[2]);
    return picolSetResult(i, buf);
  }
  SCAN_INT(value, argv[2]);
  return picolSetFmtResult(i, argv[1], value);
}
COMMAND(gets) {
  char buf[MAXSTR], *getsrc;
  FILE* fp = stdin;
  ARITY2(argc == 2 || argc == 3, "gets channelId ?varName?")
  picolSetResult(i,"-1");
  if(!EQ(argv[1],"stdin")) SCAN_PTR(fp,argv[1]); /* caveat usor */
  if(!feof(fp)) {
    getsrc = fgets(buf,sizeof(buf),fp);
    if(feof(fp)) buf[0] = '\0';
    else buf[strlen(buf)-1] = '\0'; /* chomp newline */
    if (argc == 2) picolSetResult(i,buf);
    else if(getsrc) {
      picolSetVar(i,argv[2],buf);
      picolSetIntResult(i,strlen(buf));
    }
  }
  return PICOL_OK;
}
COMMAND(global) {
  ARITY2(argc > 1, "global varName ?varName ...?");
  if(i->level > 0) {
    int a; for(a = 1; a < argc; a++) {
      picolVar* v = picolGetVar(i, argv[a]);
      if(v) return picolErr1(i, "variable \"%s\" already exists",argv[a]);
      picolSetVar(i,argv[a],NULL);
    }
  }
  return PICOL_OK;
}
COMMAND(if) {
  int rc;
  ARITY2(argc==3 || argc==5, "if test script1 ?else script2?")
  if ((rc = picolCondition(i,argv[1])) != PICOL_OK) return rc;
  if ((rc = atoi(i->result))) return picolEval(i,argv[2]);
  else if (argc == 5)         return picolEval(i,argv[4]);
  return picolSetResult(i,"");
}
int picol_InNi(picolInterp *i, int argc, char **argv, void *pd) {
  char buf[MAXSTR], *cp;
  int in = EQ(argv[0],"in");
  ARITY2(argc == 3, "in|ni element list");
  /* ARITY2 "ni element list" */
  FOREACH(buf,cp,argv[2])
    if(EQ(buf,argv[1])) return picolSetBoolResult(i,in);
  return picolSetBoolResult(i,!in);
}
COMMAND(incr) {
  int value = 0, increment = 1;
  picolVar* v;
  ARITY2(argc == 2 || argc == 3,"incr varName ?increment?")
  v = picolGetVar(i,argv[1]);
  if(v && !v->val)   v = picolGetGlobalVar(i,argv[1]);
  if(v)         SCAN_INT(value,     v->val); /* creates if not exists */
  if(argc == 3) SCAN_INT(increment, argv[2]);
  picolSetIntVar(i,argv[1], value += increment);
  return picolSetIntResult(i, value);
}
COMMAND(info) {
  char   buf[MAXSTR] = "", *pat = "*";
  picolCmd *c = i->commands;
  int procs = SUBCMD("procs");
  ARITY2(argc == 2 || argc == 3,
  /*ARITY2*/"info args|body|commands|exists|globals|level|patchlevel|procs|script|vars")
  if(argc == 3) pat = argv[2];
  if (SUBCMD("vars") || SUBCMD("globals")) {
    picolCallFrame *cf = i->callframe;
    picolVar       *v;
    if(SUBCMD("globals")) {while(cf->parent) cf = cf->parent;}
    for(v = cf->vars; v; v = v->next) {
      if(picolMatch(pat,v->name)) LAPPEND(buf,v->name);
    }
    picolSetResult(i,buf);
  } else if (SUBCMD("args") || SUBCMD("body")) {
    if(argc==2) return picolErr1(i,"usage: info %s procname", argv[1]);
    for( ; c; c = c->next) {
      if(EQ(c->name,argv[2])) {
        char **pd = c->privdata;
        if(pd) return picolSetResult(i,pd[(EQ(argv[1],"args")?0 : 1)]);
        else   return picolErr1(i,"\"%s\" isn't a procedure", c->name);
      }
    }
  } else if (SUBCMD("commands") || procs) {
    for( ; c; c = c->next)
      if((!procs||c->privdata)&&picolMatch(pat,c->name)) LAPPEND(buf,c->name);
    picolSetResult(i,buf);
  } else if (SUBCMD("exists")) {
    if(argc != 3) return picolErr(i,"usage: info exists varName");
    picolSetBoolResult(i, (int)picolGetVar(i,argv[2]));
  } else if (SUBCMD("level")) {
    if(argc==2) picolSetIntResult(i, i->level);
    else if(argc==3) {
      int level;
      SCAN_INT(level,argv[2]);
      if(level==0) picolSetResult(i,i->callframe->command);
      else return picolErr1(i,"unsupported level %s",argv[2]);
    }
  } else if (SUBCMD("patchlevel") || SUBCMD("pa")) {
    picolSetResult(i, PICOL_PATCHLEVEL);
  } else if (SUBCMD("script")) {
    picolVar* v = picolGetVar(i,"::_script_");
    if(v) picolSetResult(i,v->val);
  } else return picolErr1(i, "bad option %s, must be: args, body, commands,\
 exists, globals, level, patchlevel, procs, script, or vars", argv[1]);
  return PICOL_OK;
}
COMMAND(interp) {
  picolInterp *src = i, *trg = i;
  if(SUBCMD("alias")) {
    picolCmd *c = NULL;
    ARITY2(argc==6,"interp alias slavePath slaveCmd masterPath masterCmd")
    if(!EQ(argv[2],"")) SCAN_PTR(trg,argv[2]);
    if(!EQ(argv[4],"")) SCAN_PTR(src,argv[4]);
    c = picolGetCmd(src,argv[5]);
    if(!c) return picolErr(i,"can only alias existing commands");
    picolRegisterCmd(trg,argv[3],c->func,c->privdata);
    return picolSetResult(i,argv[3]);    
  } else if(SUBCMD("create")) {
    char buf[32];
    ARITY(argc == 2)
    trg = picolCreateInterp();
    sprintf(buf,"%p", trg);
    return picolSetResult(i,buf);
  } else if(SUBCMD("eval")) {
    int rc;
    ARITY(argc == 4)
    SCAN_PTR(trg, argv[2]);        
    rc = picolEval(trg, argv[3]);
    picolSetResult(i, trg->result);
    return rc;
  } else return picolErr(i,"usage: interp alias|create|eval ...");
}
COMMAND(join) {
  char buf[MAXSTR] = "", buf2[MAXSTR]="", *with = " ", *cp, *cp2 = NULL;
  ARITY2(argc == 2 || argc == 3, "join list ?joinString?")
  if(argc == 3) with = argv[2];
  for(cp=picolParseList(argv[1],buf); cp; cp=picolParseList(cp,buf)) {
    APPEND(buf2,buf);
    cp2 = buf2 + strlen(buf2);
    if(*with) APPEND(buf2,with);
  }
  if(cp2) *cp2 = '\0'; /* remove last separator */
  return picolSetResult(i,buf2);
}
COMMAND(lappend) {
  char buf[MAXSTR] = "";
  int a;
  picolVar *v;
  ARITY2(argc >= 2, "lappend varName ?value value ...?");
  if((v = picolGetVar(i,argv[1]))) strcpy(buf, v->val);
  for(a = 2; a < argc; a++) LAPPEND(buf,argv[a]);
  picolSetVar(i,argv[1],buf);
  return picolSetResult(i,buf);
}
COMMAND(lindex) {
  char buf[MAXSTR] = "", *cp;
  int n = 0, idx;
  ARITY2(argc == 3, "lindex list index")
  SCAN_INT(idx,argv[2]);
  for(cp=picolParseList(argv[1],buf); cp; cp=picolParseList(cp,buf), n++)
    if(n==idx) return picolSetResult(i,buf);
  return PICOL_OK;
}
COMMAND(linsert) {
  char buf[MAXSTR] = "", buf2[MAXSTR]="", *cp;
  int pos = -1, j=0, a, atend=0;
  ARITY2(argc >= 3, "linsert list index element ?element ...?")
  if(!EQ(argv[2],"end")) {SCAN_INT(pos,argv[2]);}
  else atend = 1;
  FOREACH(buf,cp,argv[1]) {
    if(!atend && pos==j) for(a=3; a < argc; a++) LAPPEND(buf2, argv[a]);
    LAPPEND(buf2,buf);
    j++;
  }
  if(atend) for(a=3; a<argc; a++) LAPPEND(buf2, argv[a]);
  return picolSetResult(i, buf2);
}
COMMAND(list) {
  char buf[MAXSTR] = "";
  int a;
  /* ARITY2 "list ?value ...?" for documentation */
  for(a=1; a<argc; a++) LAPPEND(buf,argv[a]);
  return picolSetResult(i,buf);
}
COMMAND(llength) {
  char buf[MAXSTR], *cp;
  int n = 0;
  ARITY2(argc == 2, "llength list")
  FOREACH(buf,cp,argv[1])  n++;
  return picolSetIntResult(i,n);
}
COMMAND(lrange) {
  char buf[MAXSTR] = "", buf2[MAXSTR] = "", *cp;
  int from, to = LONG_MAX, a = 0;
  ARITY2(argc == 4, "lrange list first last")
  SCAN_INT(from,argv[2]);
  if(!EQ(argv[3],"end")) SCAN_INT(to,argv[3]);
  FOREACH(buf,cp,argv[1]) {
    if(a>=from && a<=to) LAPPEND(buf2,buf);
    a++;
  }
  return picolSetResult(i,buf2);
}
COMMAND(lreplace) {
  char buf[MAXSTR] = "", *what = "", *cp;
  char buf2[MAXSTR]="";
  int from, to = LONG_MAX, a = 0, done = 0, j;
  if(argc == 5) what = argv[4];
  ARITY2(argc >= 4, "lreplace list first last ?element element ...?")
  SCAN_INT(from,argv[2]);
  if(!EQ(argv[3],"end")) SCAN_INT(to,argv[3]);
  FOREACH(buf,cp,argv[1]) {
    if(a<from || a>to) {LAPPEND(buf2,buf);}
    else if(!done) {
      for(j=4; j<argc; j++) LAPPEND(buf2,argv[j]);
      done = 1;
    }
    a++;
  }
  return picolSetResult(i,buf2);
}
COMMAND(lsearch) {
  char buf[MAXSTR] = "", *cp;
  int j = 0; 
  ARITY2(argc == 3, "lsearch list pattern")
  FOREACH(buf,cp,argv[1]) {
    if(picolMatch(argv[2],buf)) return picolSetIntResult(i,j);
    j++;
  }
  return picolSetResult(i,"-1");
}
COMMAND(lset) {
  char buf[MAXSTR] = "", buf2[MAXSTR]="", *cp;
  picolVar *var; int pos, a=0;
  ARITY2(argc == 4, "lset listVar index value")
  GET_VAR(var,argv[1]);
  if(!var->val) var = picolGetGlobalVar(i,argv[1]);
  if(!var) return picolErr1(i, "no variable %s", argv[1]);
  SCAN_INT(pos,argv[2]);
  FOREACH(buf,cp,var->val) {
    if(a==pos) {LAPPEND(buf2,argv[3]);}
    else        LAPPEND(buf2,buf);
    a++;
  }
  if(pos < 0 || pos > a) return picolErr(i,"list index out of range");
  picolSetVar(i,var->name,buf2);
  return picolSetResult(i,buf2);
}
/* ----------- sort functions for lsort ---------------- */
int qsort_cmp(const void* a, const void *b) {
  return strcmp(*(const char**)a, *(const char**)b);}
int qsort_cmp_decr(const void* a, const void *b) {
  return -strcmp(*(const char**)a, *(const char**)b);}
int qsort_cmp_int(const void* a, const void *b) {
  int diff = atoi(*(const char**)a)-atoi(*(const char**)b);
  return (diff > 0? 1: diff < 0? -1: 0);}

COMMAND(lsort) {
  char buf[MAXSTR] = "_l "; /* dispatch to helper function picolLsort */
  ARITY2(argc == 2 || argc == 3, "lsort ?-decreasing|-integer|-unique? list")
  APPEND(buf,argv[1]);
  if(argc==3) {APPEND(buf," "); APPEND(buf,argv[2]);}
  return picolEval(i,buf);
}
int picolLsort(picolInterp *i, int argc, char **argv, void *pd) {
  char buf[MAXSTR] = "";
  char** av = argv+1;
  int ac = argc-1, a;
  if(argc<2) return picolSetResult(i,"");
  if(argc>2 && EQ(argv[1],"-decreasing")) {
    qsort(++av,--ac,sizeof(char*),qsort_cmp_decr);
  } else if(argc>2 && EQ(argv[1],"-integer")) {
    qsort(++av,--ac,sizeof(char*),qsort_cmp_int);
  } else if(argc>2 && EQ(argv[1],"-unique")) {
    qsort(++av,--ac,sizeof(char*),qsort_cmp);
    for(a=0; a<ac; a++) {
      if(a==0 || !EQ(av[a],av[a-1])) LAPPEND(buf,av[a]);
    }
    return picolSetResult(i,buf);
  } else qsort(av,ac,sizeof(char*),qsort_cmp);
  picolSetResult(i,picolList(buf,ac,av));
  return PICOL_OK;
}
int picol_Math(picolInterp *i, int argc, char **argv, void *pd) {
  int a = 0, b = 0, c = -1, p;
  if(argc>=2) SCAN_INT(a, argv[1]);
  if(argc==3) SCAN_INT(b, argv[2]);
  if (argv[0][0] == '/' && !b) {return picolErr(i, "divide by zero");}
  /*ARITY2 "+ ?arg..." */
  if      (EQ(argv[0],"+" )) {FOLD(c=0,c += a,1)}
  /*ARITY2 "- arg ?arg...?" */
  else if (EQ(argv[0],"-" )) {if(argc==2) c=-a; else FOLD(c=a,c -= a,2)}
  /*ARITY2"* ?arg...?" */
  else if (EQ(argv[0],"*" )) {FOLD(c=1,c *= a,1)}
  /*ARITY2 "** a b" */
  else if (EQ(argv[0],"**" )) {ARITY(argc==3) c = 1; while(b-- > 0) c = c*a;}
  /*ARITY2 "/ a b" */
  else if (EQ(argv[0],"/" )) {ARITY(argc==3) c = a / b;}
  /*ARITY2"% a b" */
  else if (EQ(argv[0],"%" )) {ARITY(argc==3) c = a % b;}
  /*ARITY2"&& ?arg...?"*/
  else if (EQ(argv[0],"&&")) {FOLD(c=1,c = c && a,1)}
  /*ARITY2"|| ?arg...?"*/
  else if (EQ(argv[0],"||")) {FOLD(c=0,c = c || a,1)}
  /*ARITY2"> a b" */
  else if (EQ(argv[0],">" )) {ARITY(argc==3) c = a > b;}
  /*ARITY2">= a b"*/
  else if (EQ(argv[0],">=")) {ARITY(argc==3) c = a >= b;}
  /*ARITY2"< a b" */
  else if (EQ(argv[0],"<" )) {ARITY(argc==3) c = a < b;}
  /*ARITY2"<= a b"*/
  else if (EQ(argv[0],"<=")) {ARITY(argc==3) c = a <= b;}
  /*ARITY2"== a b"*/
  else if (EQ(argv[0],"==")) {ARITY(argc==3) c = a == b;}
  /*ARITY2"!= a b"*/
  else if (EQ(argv[0],"!=")) {ARITY(argc==3) c = a != b;}
  return picolSetIntResult(i,c);
}
COMMAND(not) {
  int res;                  /* implements the [! int] command */
  ARITY2(argc == 2,"! expression")
  SCAN_INT(res, argv[1]);
  return picolSetBoolResult(i, !res);
}
COMMAND(open) {
  char *mode = "r";
  FILE* fp = NULL;
  ARITY2(argc == 2 || argc == 3, "open fileName ?access?")
  if(argc == 3) mode = argv[2];
  fp = fopen(argv[1], mode);
  if(!fp) return picolErr1(i,"could not open %s", argv[1]);
  return picolSetFmtResult(i,"%p",(int)fp);
}
COMMAND(pid) {
  ARITY2(argc == 1, "pid")
  return picolSetIntResult(i,getpid());
}
COMMAND(proc) {
  char **procdata = NULL;
  picolCmd* c = picolGetCmd(i,argv[1]);
  ARITY2(argc == 4, "proc name args body");
  if(c) procdata = c->privdata;
  if(!procdata) {
    procdata = malloc(sizeof(char*)*2);
    if(c) {
      c->privdata = procdata;
      c->func = picolCallProc; /* may override C-coded cmds */
    }
  }
  procdata[0] = strdup(argv[2]); /* arguments list */
  procdata[1] = strdup(argv[3]); /* procedure body */
  if(!c) picolRegisterCmd(i,argv[1],picolCallProc,procdata);
  return PICOL_OK;
}
COMMAND(puts) {
  FILE* fp = stdout;
  char  *chan=NULL, *str="", *fmt = "%s\n";
  int rc;
  ARITY2(argc >= 2 && argc <= 4, "puts ?-nonewline? ?channelId? string")
  if(argc==2) str = argv[1];
  else if(argc==3) {
    str = argv[2];
    if(EQ(argv[1],"-nonewline")) fmt = "%s";
    else chan = argv[1];
  } else { /* argc==4 */
    if(!EQ(argv[1],"-nonewline"))
      return picolErr(i, "usage: puts ?-nonewline? ?chan? string");
    fmt = "%s"; chan = argv[2]; str = argv[3];
  }
  if(chan  && !((EQ(chan,"stdout")) || EQ(chan,"stderr")))
    SCAN_PTR(fp, chan);
  if(chan && EQ(chan, "stderr")) fp = stderr;
  rc = fprintf(fp, fmt, str);
  if(!rc || ferror(fp)) return picolErr(i,"channel is not open for writing");
  return picolSetResult(i,"");
}
COMMAND(rand) {
  int n;
  ARITY2(argc == 2, "rand n (returns a random integer 0..<n)");
  SCAN_INT(n,argv[1]);
  return picolSetIntResult(i, n? rand()%n : rand());
}
COMMAND(read) {
  char  buf[MAXSTR*64];
  int   size = sizeof(buf)-1;
  FILE *fp = NULL;
  ARITY2(argc == 2 || argc == 3, "read channelId ?size?")
  SCAN_PTR(fp, argv[1]); /* caveat usor */
  if(argc==3) SCAN_INT(size,argv[2]);
  buf[fread(buf,1,size,fp)] = '\0';
  return picolSetResult(i,buf); 
}
COMMAND(rename) {
  int found = 0;
  picolCmd *c, *last = NULL;
  ARITY2(argc == 3, "rename oldName newName")
    for(c = i->commands; c; last = c, c=c->next) {
      if(EQ(c->name,argv[1])) {
        if(!last && EQ(argv[2],"")) i->commands = c->next; /* delete first */
        else if(EQ(argv[2],""))     last->next = c->next;  /* delete other */
        else           c->name = strdup(argv[2]); /* overwrite, don't free */
        found = 1;
        break;
      }
    }
  if(!found) return picolErr1(i,"can't rename %s: no such command",argv[1]);
  return PICOL_OK;
}
COMMAND(return) {
  ARITY2(argc == 1 || argc == 2, "return ?result?")
  picolSetResult(i, (argc == 2) ? argv[1] : "");
  return PICOL_RETURN;
}
COMMAND(scan) {
  int result, rc = 1; /* limited to one integer so far */
  ARITY2(argc == 3 || argc == 4, "scan string formatString ?varName?")
  if(strlen(argv[2])!=2 || argv[2][0]!='%')
      return picolErr1(i,"bad format %s",argv[2]);
  switch(argv[2][1]) {
  case 'c' :  result = (int)(argv[1][0]&0xFF); break;
  case 'd': case 'x': case 'o':
              rc = sscanf(argv[1],argv[2],&result); break;
  default:    return picolErr1(i,"bad format %s", argv[2]);
  }
  if(rc != 1) return picolErr1(i,"bad format %s",argv[2]);
  if(argc==4) { picolSetIntVar(i, argv[3], result); result = 1; }
  return picolSetIntResult(i,result);
}
COMMAND(set) {
  picolVar* pv;
  ARITY2(argc == 2 || argc == 3, "set varName ?newValue?")
    if (argc == 2) {
      GET_VAR(pv,argv[1]);
      if(pv && pv->val) return picolSetResult(i,pv->val);
      else pv = picolGetGlobalVar(i,argv[1]);
      if(!(pv && pv->val)) return picolErr1(i,"no value of '%s'\n",argv[1]);
      return picolSetResult(i,pv->val);
    } else {
      picolSetVar(i,argv[1],argv[2]);
      return picolSetResult(i,argv[2]);
    }
}
int picolSource(picolInterp *i,char *filename) {
  char buf[MAXSTR*64];
  int rc;
  FILE *fp = fopen(filename,"r");
  if (!fp) return picolErr1(i,"No such file or directory '%s'",filename);
  picolSetVar(i,"::_script_",filename);
  buf[fread(buf,1,sizeof(buf),fp)] = '\0';
  fclose(fp);
  rc = picolEval(i,buf);
  picolSetVar(i,"::_script_",""); /* script only known during [source] */
  return rc;
}
COMMAND(source) {
  ARITY2(argc == 2, "source filename")
  return picolSource(i, argv[1]);
}
COMMAND(split) {
  char *split = " ", *cp, *start;
  char buf[MAXSTR] = "", buf2[MAXSTR] = "";
  ARITY2(argc == 2 || argc == 3, "split string ?splitChars?");
  if(argc==3) split = argv[2];
  if(EQ(split,"")) {
    for(cp = argv[1]; *cp; cp++) {
      buf2[0] = *cp;
      LAPPEND(buf, buf2);
    }
  } else {
    for(cp = argv[1], start=cp; *cp; cp++) {
      if(strchr(split,*cp)) {
        strncpy(buf2,start,cp-start);
        buf2[strlen(buf2)] = '\0';
        LAPPEND(buf,buf2);
        buf2[0] = '\0';
        start = cp+1;
      }
    }
    LAPPEND(buf,start);
  }
  return  picolSetResult(i,buf);
}
char* picolStrRev(char *str) {
  char *cp = str, *cp2 = str + strlen(str)-1, tmp;
  while(cp<cp2) {tmp=*cp; *cp=*cp2; *cp2=tmp; cp++; cp2--;}
  return str;
}
char* picolToLower(char *str) {
  char *cp; for(cp = str; *cp; cp++) *cp = tolower(*cp);
  return str;
}
char* picolToUpper(char *str) {
  char *cp; for(cp = str; *cp; cp++) *cp = toupper(*cp);
  return str;
  }
COMMAND(string) {
  char buf[MAXSTR] = "\0";
  ARITY2(argc >= 3, "string option string ?arg..?")
    if(SUBCMD("length")) picolSetIntResult(i,strlen(argv[2]));

    else if(SUBCMD("compare")) {
      ARITY2(argc == 4, "string compare s1 s2");
      picolSetIntResult(i, strcmp(argv[2],argv[3]));

    } else if(SUBCMD("equal")) {
      ARITY2(argc == 4, "string equal s1 s2");
      picolSetIntResult(i, EQ(argv[2],argv[3]));

    } else if(SUBCMD("first") || SUBCMD("last")) {
      int offset = 0, res = -1;
      char* cp;
      if(argc != 4 && argc != 5)
        return picolErr1(i,"usage: string %s substr str ?index?", argv[1]);
      if(argc == 5) SCAN_INT(offset, argv[4]);
      if(SUBCMD("first")) cp = strstr(argv[3]+offset,argv[2]);
      else { /* last */
        char* cp2 = argv[3]+offset;
        while((cp = strstr(cp2,argv[2]))) cp2=cp+1;
        cp = cp2 - 1;
      }
      if(cp) res = cp - argv[3]; 
      picolSetIntResult(i,res);

    } else if(SUBCMD("index")) {
      int maxi = strlen(argv[2])-1, from;
      ARITY2(argc == 4, "string index string charIndex")
      SCAN_INT(from, argv[3]);
      if(from < 0) from = 0; else if(from > maxi) from = maxi;
      buf[0] = argv[2][from];
      picolSetResult(i, buf);

    } else if(SUBCMD("match")) {
      if(argc == 4) 
        return picolSetBoolResult(i,picolMatch(argv[2],argv[3]));
      else if(argc == 5 && EQ(argv[2],"-nocase")) {
        picolToUpper(argv[3]);
        picolToUpper(argv[4]);
        return picolSetBoolResult(i,picolMatch(argv[3],argv[4]));
      } else return picolErr(i,"usage: string match pat str");

    } else if(SUBCMD("is")) {
      ARITY2(argc == 4  && EQ(argv[2],"int"),
             /*ARITY2*/ "string is int str");
      picolSetBoolResult(i, picolIsInt(argv[3]));

    } else if(SUBCMD("range")) {
      int from, to, maxi = strlen(argv[2])-1;
      ARITY2(argc == 5, "string range string first last")
        SCAN_INT(from, argv[3]);
      if(EQ(argv[4],"end")) to = maxi; else SCAN_INT(to,argv[4]);
      if(from < 0) from = 0; else if(from > maxi) from = maxi;
      if(to < 0)   to = 0;   else if(to > maxi)   to   = maxi;
      strncpy(buf,&argv[2][from],to-from+1);
      buf[to] = '\0';
      picolSetResult(i,buf);

    } else if(SUBCMD("repeat")) {
      int j, n; SCAN_INT(n,argv[3]);
      ARITY2(argc == 4, "string repeat string count") 
      for(j=0;j<n;j++) APPEND(buf,argv[2]);
      picolSetResult(i,buf);

    } else if(SUBCMD("reverse")) {
      ARITY2(argc == 3, "string reverse str")
        picolSetResult(i,picolStrRev(argv[2]));
    } else if(SUBCMD("tolower") && argc==3) {
      picolSetResult(i, picolToLower(argv[2]));
    } else if(SUBCMD("toupper") && argc==3) {
      picolSetResult(i, picolToUpper(argv[2]));
      
    } else if(SUBCMD("trim") || SUBCMD("trimleft") || SUBCMD("trimright")) {
      char *trimchars = " \t\n\r", *start, *end;
      int len;
      ARITY2(argc ==3 || argc == 4, "string trim?left|right? string ?chars?");
      if(argc==4) trimchars = argv[3];
      start = argv[2];
      if(!SUBCMD("trimright")) {
        for( ;*start;start++) {if(strchr(trimchars,*start)==NULL) break;}
      }
      end = argv[2]+strlen(argv[2]);
      if(!SUBCMD("trimleft")) {
        for( ; end>=start;end--) {if(strchr(trimchars,*end)==NULL) break;}
      }
      len = end - start+1;
      strncpy(buf,start,len);
      buf[len] = '\0';
      return picolSetResult(i,buf);
      
    } else return picolErr1(i,
          "bad option '%s', must be compare, equal, first, index, is int, last,\
 length, match, range, repeat, reverse, tolower, or toupper",
                            argv[1]);
  return PICOL_OK;
}
COMMAND(subst) {
  ARITY2(argc == 2, "subst string")
  return picolSubst(i,argv[1]);
}
COMMAND(switch) {
  char *cp, buf[MAXSTR] = "";
  int fallthrough = 0, a;
  ARITY2(argc > 2, "switch string pattern body ... ?default body?")
    if(argc==3) { /* braced body variant */
      FOREACH(buf,cp,argv[2]) {
        if(fallthrough || EQ(buf,argv[1]) || EQ(buf,"default")) {
          cp = picolParseList(cp,buf);
          if(!cp) return picolErr(i,"switch: list must have an even number");
          if(EQ(buf,"-")) fallthrough = 1;
          else return picolEval(i,buf);
        }
      }
    } else {     /* unbraced body */
      if(argc%2) return picolErr(i,"switch: list must have an even number");
      for(a = 2; a < argc; a++) {
        if(fallthrough || EQ(argv[a],argv[1]) || EQ(argv[a],"default")) {
          if(EQ(argv[a+1],"-")) {fallthrough = 1; a++;}
          else return picolEval(i,argv[a+1]);
        }
      }
    }
  return picolSetResult(i,"");
}
COMMAND(time) {
  int j, n = 1, rc;
  clock_t t0 = clock(), dt;
  ARITY2(argc==2 || argc==3, "time command ?count?");
  if(argc==3) SCAN_INT(n,argv[2]);
  for(j = 0; j < n; j++) {
    if((rc = picolEval(i,argv[1])) != PICOL_OK) return rc;
  }
  dt = (clock()-t0)*1000/n;
  return picolSetFmtResult(i,"%d clicks per iteration",dt);
}
COMMAND(trace) {
  ARITY2(argc==2 || argc == 6, "trace add|remove ?execution * mode cmd?");
  if     (EQ(argv[1],"add"))    i->trace = 1;
  else if(EQ(argv[1],"remove")) i->trace = 0;
  else return picolErr1(i,"Bad option %s, must be add, or remove",argv[1]);
  return PICOL_OK;
}
COMMAND(unset) {
  picolVar *v, *lastv = NULL;
  ARITY2(argc == 2, "unset varName")
    for(v = i->callframe->vars; v; lastv = v, v = v->next) {
      if(EQ(v->name,argv[1])) {
        if(!lastv) i->callframe->vars = v->next;
        else       lastv->next = v->next; /* may need to free? */
        break;
      }
    }
  return picolSetResult(i,"");
}
COMMAND(uplevel) {
  char buf[MAXSTR];
  int rc, delta;
  picolCallFrame* cf = i->callframe;
  ARITY2(argc >= 3, "uplevel level command ?arg...?");
  if(EQ(argv[1],"#0")) delta = 9999;
  else SCAN_INT(delta,argv[1]);
  for( ;delta>0 && i->callframe->parent; delta--)
    i->callframe = i->callframe->parent; 
  rc = picolEval(i, picolConcat(buf,argc-1,argv+1));
  i->callframe = cf; /* back to normal */
  return rc;
}
COMMAND(variable) {
  char buf[MAXSTR];     /* limited to :: namespace so far */
  int a, rc = PICOL_OK;
  ARITY2(argc>1, "variable ?name value...? name ?value?")
    for(a = 1; a < argc && rc == PICOL_OK; a++) {
      strcpy(buf,"global "); strcat(buf,argv[a]);
      rc = picolEval(i,buf);
      if(rc == PICOL_OK && a < argc-1) {
        rc = picolSetGlobalVar(i,argv[a],argv[a+1]); a++;
      }
    }
  return rc;
}
COMMAND(while) {
  ARITY2(argc == 3, "while test command")
    while(1) {
      int rc = picolCondition(i,argv[1]);
      if (rc != PICOL_OK) return rc;
      if (atoi(i->result)) {
        rc = picolEval(i,argv[2]);
        if (rc == PICOL_CONTINUE || rc == PICOL_OK) continue;
        else if (rc == PICOL_BREAK) break;
        else return rc;
      } else break;
    }
  return picolSetResult(i,"");
} /* --------------------------------------------------- now registering... */
void picolRegisterCoreCmds(picolInterp *i) {
  int j;
  char *name[] =
    {"+","-","*","**","/","%",">",">=","<","<=","==","!=","&&","||"};
  for (j = 0; j < (int)(sizeof(name)/sizeof(char*)); j++)
    picolRegisterCmd(i,name[j],picol_Math,NULL);
  picolRegisterCmd(i,"abs",    picol_abs,NULL);
  picolRegisterCmd(i,"append", picol_append,NULL);
  picolRegisterCmd(i,"apply",  picol_apply,NULL);
  picolRegisterCmd(i,"array",  picol_array,NULL);
  picolRegisterCmd(i,"break",  picol_break,NULL);
  picolRegisterCmd(i,"catch",  picol_catch,NULL);
  picolRegisterCmd(i,"clock",  picol_clock,NULL);
  picolRegisterCmd(i,"close",  picolFileUtil,NULL);
  picolRegisterCmd(i,"concat", picol_concat,NULL);
  picolRegisterCmd(i,"continue",picol_continue,NULL);
  picolRegisterCmd(i,"eof",    picolFileUtil,NULL);
  picolRegisterCmd(i,"eq",     picol_EqNe,NULL);
  picolRegisterCmd(i,"error",  picol_error,NULL);
  picolRegisterCmd(i,"eval",   picol_eval,NULL);
  picolRegisterCmd(i,"exec",   picol_exec,NULL);
  picolRegisterCmd(i,"exit",   picol_exit,NULL);
  picolRegisterCmd(i,"expr",   picol_expr,NULL);
  picolRegisterCmd(i,"file",   picol_file,NULL);
  picolRegisterCmd(i,"flush",  picolFileUtil,NULL);
  picolRegisterCmd(i,"for",    picol_for,NULL);
  picolRegisterCmd(i,"foreach",picol_foreach,NULL);
  picolRegisterCmd(i,"format", picol_format,NULL);
  picolRegisterCmd(i,"gets",   picol_gets,NULL);
  picolRegisterCmd(i,"global", picol_global,NULL);
  picolRegisterCmd(i,"if",     picol_if,NULL);
  picolRegisterCmd(i,"in",     picol_InNi,NULL);
  picolRegisterCmd(i,"incr",   picol_incr,NULL);
  picolRegisterCmd(i,"info",   picol_info,NULL);
  picolRegisterCmd(i,"interp", picol_interp,NULL);
  picolRegisterCmd(i,"join",   picol_join,NULL);
  picolRegisterCmd(i,"lappend",picol_lappend,NULL);
  picolRegisterCmd(i,"lindex", picol_lindex,NULL);
  picolRegisterCmd(i,"linsert",picol_linsert,NULL);
  picolRegisterCmd(i,"list",   picol_list,NULL);
  picolRegisterCmd(i,"llength",picol_llength,NULL);
  picolRegisterCmd(i,"lrange", picol_lrange,NULL);
  picolRegisterCmd(i,"lreplace",picol_lreplace,NULL);
  picolRegisterCmd(i,"lsearch",picol_lsearch,NULL);
  picolRegisterCmd(i,"lset",   picol_lset,NULL);
  picolRegisterCmd(i,"lsort",  picol_lsort,NULL);
  picolRegisterCmd(i,"ne",     picol_EqNe,NULL);
  picolRegisterCmd(i,"ni",     picol_InNi,NULL);
  picolRegisterCmd(i,"open",   picol_open,NULL);
  picolRegisterCmd(i,"pid",    picol_pid,NULL);
  picolRegisterCmd(i,"proc",   picol_proc,NULL);
  picolRegisterCmd(i,"puts",   picol_puts,NULL);
  picolRegisterCmd(i,"rand",   picol_rand,NULL);
  picolRegisterCmd(i,"read",   picol_read,NULL);
  picolRegisterCmd(i,"rename", picol_rename,NULL);
  picolRegisterCmd(i,"return", picol_return,NULL);
  picolRegisterCmd(i,"scan",   picol_scan,NULL);
  picolRegisterCmd(i,"seek",   picolFileUtil,NULL);
  picolRegisterCmd(i,"set",    picol_set,NULL);
  picolRegisterCmd(i,"source", picol_source,NULL);
  picolRegisterCmd(i,"split",  picol_split,NULL);
  picolRegisterCmd(i,"string", picol_string,NULL);
  picolRegisterCmd(i,"subst",  picol_subst,NULL);
  picolRegisterCmd(i,"switch", picol_switch,NULL);
  picolRegisterCmd(i,"tell",   picolFileUtil,NULL);
  picolRegisterCmd(i,"time",   picol_time,NULL);
  picolRegisterCmd(i,"trace",  picol_trace,NULL);
  picolRegisterCmd(i,"unset",  picol_unset,NULL);
  picolRegisterCmd(i,"uplevel",picol_uplevel,NULL);
  picolRegisterCmd(i,"variable",picol_variable,NULL);
  picolRegisterCmd(i,"while",  picol_while,NULL);
  picolRegisterCmd(i,"!",      picol_not,NULL);
  picolRegisterCmd(i,"_l",     picolLsort,NULL);
}
picolInterp* picolCreateInterp(void) {
  picolInterp* i = calloc(1,sizeof(picolInterp));
  picolInitInterp(i);
  picolRegisterCoreCmds(i);
  picolSetVar(i,"::errorInfo","");
  srand(clock());
  return i;
} /* ----------------------------------------------------------------- MAIN! */
int main(int argc, char **argv) {
  picolInterp *i = picolCreateInterp();
  char buf[MAXSTR] = "";
  int  rc;
  FILE* fp = fopen("init.pcl","r");
  picolSetVar(i,"argv0", argv[0]);
  picolSetVar(i,"argv",  "");
  picolSetVar(i,"argc",  "0");
  picolSetVar(i,"auto_path", "");
  picolEval(i,"array set env {}"); /* populated from real env on demand */
  if(fp) {
    fclose(fp);
    rc = picolSource(i,"init.pcl");
    if(rc != PICOL_OK) puts(i->result);
    i->current = NULL; /* avoid misleading error traceback */
  }
  if (argc == 1) {           /* no arguments - interactive mode */
    while(1) {
      printf("picol> "); fflush(stdout);
      if (fgets(buf,sizeof(buf),stdin) == NULL) return 0;
      rc = picolEval(i,buf);
      if (i->result[0] != '\0' || rc != PICOL_OK)
        printf("[%d] %s\n", rc, i->result);
    }
  } else if(argc==3 && EQ(argv[1],"-e")) {   /* script in argv[2] */
    rc = picolEval(i,argv[2]);
    if(rc != PICOL_OK) {
      picolVar *v = picolGetVar(i,"::errorInfo");
      if(v) puts(v->val);
    } else puts(i->result);
  } else {    /* first arg is file to source, rest goes to argv */
    picolSetVar(i,   "argv0",argv[1]);
    picolSetVar(i,   "argv", picolConcat(buf,argc-1,argv+1));
    picolSetIntVar(i,"argc",argc-2);
    rc = picolSource(i,argv[1]);
    if(rc != PICOL_OK) {
      picolVar *v = picolGetVar(i,"::errorInfo");
      if(v) puts(v->val);
    }
  }
  return rc & 0xFF;
}

