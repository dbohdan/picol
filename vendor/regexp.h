#ifndef REGEXP_H
#define REGEXP_H

#ifndef REG_MM
    #define REG_MM

    #define REG_MALLOC malloc
    #define REG_FREE free
#endif

typedef struct RegProg RegProg;
typedef struct RegSub RegSub;

RegProg *reg_comp(const char *pattern, int cflags, const char **errorp);
int reg_exec(RegProg *prog, const char *string, RegSub *sub, int eflags);
void reg_free(RegProg *prog);

enum {
	/* reg_comp flags */
	REG_ICASE = 1,
	REG_NEWLINE = 2,

	/* reg_exec flags */
	REG_NOTBOL = 4,

	/* limits */
	REG_MAXSUB = 10
};

struct RegSub {
	unsigned int nsub;
	struct {
		const char *sp;
		const char *ep;
	} sub[REG_MAXSUB];
};

#endif /* REGEXP_H */

#ifdef REGEXP_IMPLEMENTATION

#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define reg_nelem(a) (sizeof (a) / sizeof (a)[0])

typedef unsigned int RegRune;

static int reg_isalpharune(RegRune c)
{
	/* TODO: Add unicode support */
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static RegRune reg_toupperrune(RegRune c)
{
	/* TODO: Add unicode support */
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 'A';
	return c;
}

static int reg_chartorune(RegRune *r, const char *s)
{
	/* TODO: Add UTF-8 reg_decoding */
	*r = *s;
	return 1;
}

#define REG_REPINF 255
#define REG_MAXPROG (32 << 10)

typedef struct RegClass RegClass;
typedef struct RegNode RegNode;
typedef struct RegInst RegInst;
typedef struct RegThread RegThread;

struct RegClass {
	RegRune *end;
	RegRune spans[64];
};

struct RegProg {
	RegInst *start, *end;
	int flags;
	unsigned int nsub;
	RegClass cclass[16];
};

static struct {
	RegProg *prog;
	RegNode *pstart, *pend;

	const char *source;
	unsigned int ncclass;
	unsigned int nsub;
	RegNode *sub[REG_MAXSUB];

	int lookahead;
	RegRune yychar;
	RegClass *yycc;
	int yymin, yymax;

	const char *error;
	jmp_buf kaboom;
} g;

static void reg_die(const char *message)
{
	g.error = message;
	longjmp(g.kaboom, 1);
}

static RegRune reg_canon(RegRune c)
{
	RegRune u = reg_toupperrune(c);
	if (c >= 128 && u < 128)
		return c;
	return u;
}

/* Scan */

enum {
	REG_L_CHAR = 256,
	REG_L_CCLASS,	/* character class */
	REG_L_NCCLASS,	/* negative character class */
	REG_L_NC,		/* "(?:" no capture */
	REG_L_PLA,		/* "(?=" positive lookahead */
	REG_L_NLA,		/* "(?!" negative lookahead */
	REG_L_WORD,		/* "\b" word boundary */
	REG_L_NWORD,	/* "\B" non-word boundary */
	REG_L_REF,		/* "\1" back-reference */
	REG_L_COUNT		/* {M,N} */
};

static int reg_hex(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xA;
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xA;
	reg_die("invalid escape sequence");
	return 0;
}

static int reg_dec(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	reg_die("invalid quantifier");
	return 0;
}

#define REG_ESCAPES "BbDdSsWw^$\\.*+?()[]{}|0123456789"

static int reg_isunicodeletter(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || reg_isalpharune(c);
}

static int reg_nextrune(void)
{
	g.source += reg_chartorune(&g.yychar, g.source);
	if (g.yychar == '\\') {
		g.source += reg_chartorune(&g.yychar, g.source);
		switch (g.yychar) {
		case 0: reg_die("unterminated escape sequence"); break;
		case 'f': g.yychar = '\f'; return 0;
		case 'n': g.yychar = '\n'; return 0;
		case 'r': g.yychar = '\r'; return 0;
		case 't': g.yychar = '\t'; return 0;
		case 'v': g.yychar = '\v'; return 0;
		case 'c':
			g.yychar = (*g.source++) & 31;
			return 0;
		case 'x':
			g.yychar = reg_hex(*g.source++) << 4;
			g.yychar += reg_hex(*g.source++);
			if (g.yychar == 0) {
				g.yychar = '0';
				return 1;
			}
			return 0;
		case 'u':
			g.yychar = reg_hex(*g.source++) << 12;
			g.yychar += reg_hex(*g.source++) << 8;
			g.yychar += reg_hex(*g.source++) << 4;
			g.yychar += reg_hex(*g.source++);
			if (g.yychar == 0) {
				g.yychar = '0';
				return 1;
			}
			return 0;
		}
		if (strchr(REG_ESCAPES, g.yychar))
			return 1;
		if (reg_isunicodeletter(g.yychar) || g.yychar == '_') /* check identity escape */
			reg_die("invalid escape character");
		return 0;
	}
	return 0;
}

static int reg_lexreg_count(void)
{
	g.yychar = *g.source++;

	g.yymin = reg_dec(g.yychar);
	g.yychar = *g.source++;
	while (g.yychar != ',' && g.yychar != '}') {
		g.yymin = g.yymin * 10 + reg_dec(g.yychar);
		g.yychar = *g.source++;
	}
	if (g.yymin >= REG_REPINF)
		reg_die("numeric overflow");

	if (g.yychar == ',') {
		g.yychar = *g.source++;
		if (g.yychar == '}') {
			g.yymax = REG_REPINF;
		} else {
			g.yymax = reg_dec(g.yychar);
			g.yychar = *g.source++;
			while (g.yychar != '}') {
				g.yymax = g.yymax * 10 + reg_dec(g.yychar);
				g.yychar = *g.source++;
			}
			if (g.yymax >= REG_REPINF)
				reg_die("numeric overflow");
		}
	} else {
		g.yymax = g.yymin;
	}

	return REG_L_COUNT;
}

static void reg_newcclass(void)
{
	if (g.ncclass >= reg_nelem(g.prog->cclass))
		reg_die("too many character classes");
	g.yycc = g.prog->cclass + g.ncclass++;
	g.yycc->end = g.yycc->spans;
}

static void reg_addrange(RegRune a, RegRune b)
{
	if (a > b)
		reg_die("invalid character class range");
	if (g.yycc->end + 2 == g.yycc->spans + reg_nelem(g.yycc->spans))
		reg_die("too many character class ranges");
	*g.yycc->end++ = a;
	*g.yycc->end++ = b;
}

static void reg_addranges_d(void)
{
	reg_addrange('0', '9');
}

static void reg_addranges_D(void)
{
	reg_addrange(0, '0'-1);
	reg_addrange('9'+1, 0xFFFF);
}

static void reg_addranges_s(void)
{
	reg_addrange(0x9, 0x9);
	reg_addrange(0xA, 0xD);
	reg_addrange(0x20, 0x20);
	reg_addrange(0xA0, 0xA0);
	reg_addrange(0x2028, 0x2029);
	reg_addrange(0xFEFF, 0xFEFF);
}

static void reg_addranges_S(void)
{
	reg_addrange(0, 0x9-1);
	reg_addrange(0x9+1, 0xA-1);
	reg_addrange(0xD+1, 0x20-1);
	reg_addrange(0x20+1, 0xA0-1);
	reg_addrange(0xA0+1, 0x2028-1);
	reg_addrange(0x2029+1, 0xFEFF-1);
	reg_addrange(0xFEFF+1, 0xFFFF);
}

static void reg_addranges_w(void)
{
	reg_addrange('0', '9');
	reg_addrange('A', 'Z');
	reg_addrange('_', '_');
	reg_addrange('a', 'z');
}

static void reg_addranges_W(void)
{
	reg_addrange(0, '0'-1);
	reg_addrange('9'+1, 'A'-1);
	reg_addrange('Z'+1, '_'-1);
	reg_addrange('_'+1, 'a'-1);
	reg_addrange('z'+1, 0xFFFF);
}

static int reg_lexclass(void)
{
	int type = REG_L_CCLASS;
	int quoted, havesave, havedash;
	RegRune save = 0;

	reg_newcclass();

	quoted = reg_nextrune();
	if (!quoted && g.yychar == '^') {
		type = REG_L_NCCLASS;
		quoted = reg_nextrune();
	}

	havesave = havedash = 0;
	for (;;) {
		if (g.yychar == 0)
			reg_die("unterminated character class");
		if (!quoted && g.yychar == ']')
			break;

		if (!quoted && g.yychar == '-') {
			if (havesave) {
				if (havedash) {
					reg_addrange(save, '-');
					havesave = havedash = 0;
				} else {
					havedash = 1;
				}
			} else {
				save = '-';
				havesave = 1;
			}
		} else if (quoted && strchr("DSWdsw", g.yychar)) {
			if (havesave) {
				reg_addrange(save, save);
				if (havedash)
					reg_addrange('-', '-');
			}
			switch (g.yychar) {
			case 'd': reg_addranges_d(); break;
			case 's': reg_addranges_s(); break;
			case 'w': reg_addranges_w(); break;
			case 'D': reg_addranges_D(); break;
			case 'S': reg_addranges_S(); break;
			case 'W': reg_addranges_W(); break;
			}
			havesave = havedash = 0;
		} else {
			if (quoted) {
				if (g.yychar == 'b')
					g.yychar = '\b';
				else if (g.yychar == '0')
					g.yychar = 0;
				/* else identity escape */
			}
			if (havesave) {
				if (havedash) {
					reg_addrange(save, g.yychar);
					havesave = havedash = 0;
				} else {
					reg_addrange(save, save);
					save = g.yychar;
				}
			} else {
				save = g.yychar;
				havesave = 1;
			}
		}

		quoted = reg_nextrune();
	}

	if (havesave) {
		reg_addrange(save, save);
		if (havedash)
			reg_addrange('-', '-');
	}

	return type;
}

static int reg_lex(void)
{
	int quoted = reg_nextrune();
	if (quoted) {
		switch (g.yychar) {
		case 'b': return REG_L_WORD;
		case 'B': return REG_L_NWORD;
		case 'd': reg_newcclass(); reg_addranges_d(); return REG_L_CCLASS;
		case 's': reg_newcclass(); reg_addranges_s(); return REG_L_CCLASS;
		case 'w': reg_newcclass(); reg_addranges_w(); return REG_L_CCLASS;
		case 'D': reg_newcclass(); reg_addranges_d(); return REG_L_NCCLASS;
		case 'S': reg_newcclass(); reg_addranges_s(); return REG_L_NCCLASS;
		case 'W': reg_newcclass(); reg_addranges_w(); return REG_L_NCCLASS;
		case '0': g.yychar = 0; return REG_L_CHAR;
		}
		if (g.yychar >= '0' && g.yychar <= '9') {
			g.yychar -= '0';
			if (*g.source >= '0' && *g.source <= '9')
				g.yychar = g.yychar * 10 + *g.source++ - '0';
			return REG_L_REF;
		}
		return REG_L_CHAR;
	}

	switch (g.yychar) {
	case 0:
	case '$': case ')': case '*': case '+':
	case '.': case '?': case '^': case '|':
		return g.yychar;
	}

	if (g.yychar == '{')
		return reg_lexreg_count();
	if (g.yychar == '[')
		return reg_lexclass();
	if (g.yychar == '(') {
		if (g.source[0] == '?') {
			if (g.source[1] == ':') {
				g.source += 2;
				return REG_L_NC;
			}
			if (g.source[1] == '=') {
				g.source += 2;
				return REG_L_PLA;
			}
			if (g.source[1] == '!') {
				g.source += 2;
				return REG_L_NLA;
			}
		}
		return '(';
	}

	return REG_L_CHAR;
}

/* Parse */

enum {
	P_CAT, P_ALT, P_REP,
	P_BOL, P_EOL, P_WORD, P_NWORD,
	P_PAR, P_PLA, P_NLA,
	P_ANY, P_CHAR, P_CCLASS, P_NCCLASS,
	P_REF
};

struct RegNode {
	unsigned char type;
	unsigned char ng, m, n;
	RegRune c;
	RegClass *cc;
	RegNode *x;
	RegNode *y;
};

static RegNode *reg_newnode(int type)
{
	RegNode *node = g.pend++;
	node->type = type;
	node->cc = NULL;
	node->c = 0;
	node->ng = 0;
	node->m = 0;
	node->n = 0;
	node->x = node->y = NULL;
	return node;
}

static int reg_empty(RegNode *node)
{
	if (!node) return 1;
	switch (node->type) {
	default: return 1;
	case P_CAT: return reg_empty(node->x) && reg_empty(node->y);
	case P_ALT: return reg_empty(node->x) || reg_empty(node->y);
	case P_REP: return reg_empty(node->x) || node->m == 0;
	case P_PAR: return reg_empty(node->x);
	case P_REF: return reg_empty(node->x);
	case P_ANY: case P_CHAR: case P_CCLASS: case P_NCCLASS: return 0;
	}
}

static RegNode *reg_newrep(RegNode *atom, int ng, int min, int max)
{
	RegNode *rep = reg_newnode(P_REP);
	if (max == REG_REPINF && reg_empty(atom))
		reg_die("infinite loop matching the empty string");
	rep->ng = ng;
	rep->m = min;
	rep->n = max;
	rep->x = atom;
	return rep;
}

static void reg_next(void)
{
	g.lookahead = reg_lex();
}

static int reg_accept(int t)
{
	if (g.lookahead == t) {
		reg_next();
		return 1;
	}
	return 0;
}

static RegNode *reg_parsealt(void);

static RegNode *reg_parseatom(void)
{
	RegNode *atom;
	if (g.lookahead == REG_L_CHAR) {
		atom = reg_newnode(P_CHAR);
		atom->c = g.yychar;
		reg_next();
		return atom;
	}
	if (g.lookahead == REG_L_CCLASS) {
		atom = reg_newnode(P_CCLASS);
		atom->cc = g.yycc;
		reg_next();
		return atom;
	}
	if (g.lookahead == REG_L_NCCLASS) {
		atom = reg_newnode(P_NCCLASS);
		atom->cc = g.yycc;
		reg_next();
		return atom;
	}
	if (g.lookahead == REG_L_REF) {
		atom = reg_newnode(P_REF);
		if (g.yychar == 0 || g.yychar > g.nsub || !g.sub[g.yychar])
			reg_die("invalid back-reference");
		atom->n = g.yychar;
		atom->x = g.sub[g.yychar];
		reg_next();
		return atom;
	}
	if (reg_accept('.'))
		return reg_newnode(P_ANY);
	if (reg_accept('(')) {
		atom = reg_newnode(P_PAR);
		if (g.nsub == REG_MAXSUB)
			reg_die("too many captures");
		atom->n = g.nsub++;
		atom->x = reg_parsealt();
		g.sub[atom->n] = atom;
		if (!reg_accept(')'))
			reg_die("unreg_matched '('");
		return atom;
	}
	if (reg_accept(REG_L_NC)) {
		atom = reg_parsealt();
		if (!reg_accept(')'))
			reg_die("unreg_matched '('");
		return atom;
	}
	if (reg_accept(REG_L_PLA)) {
		atom = reg_newnode(P_PLA);
		atom->x = reg_parsealt();
		if (!reg_accept(')'))
			reg_die("unreg_matched '('");
		return atom;
	}
	if (reg_accept(REG_L_NLA)) {
		atom = reg_newnode(P_NLA);
		atom->x = reg_parsealt();
		if (!reg_accept(')'))
			reg_die("unreg_matched '('");
		return atom;
	}
	reg_die("syntax error");
	return NULL;
}

static RegNode *reg_parserep(void)
{
	RegNode *atom;

	if (reg_accept('^')) return reg_newnode(P_BOL);
	if (reg_accept('$')) return reg_newnode(P_EOL);
	if (reg_accept(REG_L_WORD)) return reg_newnode(P_WORD);
	if (reg_accept(REG_L_NWORD)) return reg_newnode(P_NWORD);

	atom = reg_parseatom();
	if (g.lookahead == REG_L_COUNT) {
		int min = g.yymin, max = g.yymax;
		reg_next();
		if (max < min)
			reg_die("invalid quantifier");
		return reg_newrep(atom, reg_accept('?'), min, max);
	}
	if (reg_accept('*')) return reg_newrep(atom, reg_accept('?'), 0, REG_REPINF);
	if (reg_accept('+')) return reg_newrep(atom, reg_accept('?'), 1, REG_REPINF);
	if (reg_accept('?')) return reg_newrep(atom, reg_accept('?'), 0, 1);
	return atom;
}

static RegNode *reg_parsecat(void)
{
	RegNode *cat, *head, **tail;
	if (g.lookahead && g.lookahead != '|' && g.lookahead != ')') {
		/* Build a right-leaning tree by splicing in new 'cat' at the tail. */
		head = reg_parserep();
		tail = &head;
		while (g.lookahead && g.lookahead != '|' && g.lookahead != ')') {
			cat = reg_newnode(P_CAT);
			cat->x = *tail;
			cat->y = reg_parserep();
			*tail = cat;
			tail = &cat->y;
		}
		return head;
	}
	return NULL;
}

static RegNode *reg_parsealt(void)
{
	RegNode *alt, *x;
	alt = reg_parsecat();
	while (reg_accept('|')) {
		x = alt;
		alt = reg_newnode(P_ALT);
		alt->x = x;
		alt->y = reg_parsecat();
	}
	return alt;
}

/* Compile */

enum {
	I_END, I_JUMP, I_SPLIT, I_PLA, I_NLA,
	I_ANYNL, I_ANY, I_CHAR, I_CCLASS, I_NCCLASS, I_REF,
	I_BOL, I_EOL, I_WORD, I_NWORD,
	I_LPAR, I_RPAR
};

struct RegInst {
	unsigned char opcode;
	unsigned char n;
	RegRune c;
	RegClass *cc;
	RegInst *x;
	RegInst *y;
};

static unsigned int reg_count(RegNode *node)
{
	unsigned int min, max, n;
	if (!node) return 0;
	switch (node->type) {
	default: return 1;
	case P_CAT: return reg_count(node->x) + reg_count(node->y);
	case P_ALT: return reg_count(node->x) + reg_count(node->y) + 2;
	case P_REP:
		min = node->m;
		max = node->n;
		if (min == max) n = reg_count(node->x) * min;
		else if (max < REG_REPINF) n = reg_count(node->x) * max + (max - min);
		else n = reg_count(node->x) * (min + 1) + 2;
		if (n > REG_MAXPROG) reg_die("program too large");
		return n;
	case P_PAR: return reg_count(node->x) + 2;
	case P_PLA: return reg_count(node->x) + 2;
	case P_NLA: return reg_count(node->x) + 2;
	}
}

static RegInst *reg_emit(RegProg *prog, int opcode)
{
	RegInst *inst = prog->end++;
	inst->opcode = opcode;
	inst->n = 0;
	inst->c = 0;
	inst->cc = NULL;
	inst->x = inst->y = NULL;
	return inst;
}

static void reg_compile(RegProg *prog, RegNode *node)
{
	RegInst *inst, *split, *jump;
	unsigned int i;

	if (!node)
		return;

loop:
	switch (node->type) {
	case P_CAT:
		reg_compile(prog, node->x);
		node = node->y;
		goto loop;

	case P_ALT:
		split = reg_emit(prog, I_SPLIT);
		reg_compile(prog, node->x);
		jump = reg_emit(prog, I_JUMP);
		reg_compile(prog, node->y);
		split->x = split + 1;
		split->y = jump + 1;
		jump->x = prog->end;
		break;

	case P_REP:
		for (i = 0; i < node->m; ++i) {
			inst = prog->end;
			reg_compile(prog, node->x);
		}
		if (node->m == node->n)
			break;
		if (node->n < REG_REPINF) {
			for (i = node->m; i < node->n; ++i) {
				split = reg_emit(prog, I_SPLIT);
				reg_compile(prog, node->x);
				if (node->ng) {
					split->y = split + 1;
					split->x = prog->end;
				} else {
					split->x = split + 1;
					split->y = prog->end;
				}
			}
		} else if (node->m == 0) {
			split = reg_emit(prog, I_SPLIT);
			reg_compile(prog, node->x);
			jump = reg_emit(prog, I_JUMP);
			if (node->ng) {
				split->y = split + 1;
				split->x = prog->end;
			} else {
				split->x = split + 1;
				split->y = prog->end;
			}
			jump->x = split;
		} else {
			split = reg_emit(prog, I_SPLIT);
			if (node->ng) {
				split->y = inst;
				split->x = prog->end;
			} else {
				split->x = inst;
				split->y = prog->end;
			}
		}
		break;

	case P_BOL: reg_emit(prog, I_BOL); break;
	case P_EOL: reg_emit(prog, I_EOL); break;
	case P_WORD: reg_emit(prog, I_WORD); break;
	case P_NWORD: reg_emit(prog, I_NWORD); break;

	case P_PAR:
		inst = reg_emit(prog, I_LPAR);
		inst->n = node->n;
		reg_compile(prog, node->x);
		inst = reg_emit(prog, I_RPAR);
		inst->n = node->n;
		break;
	case P_PLA:
		split = reg_emit(prog, I_PLA);
		reg_compile(prog, node->x);
		reg_emit(prog, I_END);
		split->x = split + 1;
		split->y = prog->end;
		break;
	case P_NLA:
		split = reg_emit(prog, I_NLA);
		reg_compile(prog, node->x);
		reg_emit(prog, I_END);
		split->x = split + 1;
		split->y = prog->end;
		break;

	case P_ANY:
		reg_emit(prog, I_ANY);
		break;
	case P_CHAR:
		inst = reg_emit(prog, I_CHAR);
		inst->c = (prog->flags & REG_ICASE) ? reg_canon(node->c) : node->c;
		break;
	case P_CCLASS:
		inst = reg_emit(prog, I_CCLASS);
		inst->cc = node->cc;
		break;
	case P_NCCLASS:
		inst = reg_emit(prog, I_NCCLASS);
		inst->cc = node->cc;
		break;
	case P_REF:
		inst = reg_emit(prog, I_REF);
		inst->n = node->n;
		break;
	}
}

#ifdef TEST
static void reg_dumpnode(RegNode *node)
{
	RegRune *p;
	if (!node) { printf("Empty"); return; }
	switch (node->type) {
	case P_CAT: printf("Cat("); reg_dumpnode(node->x); printf(", "); reg_dumpnode(node->y); printf(")"); break;
	case P_ALT: printf("Alt("); reg_dumpnode(node->x); printf(", "); reg_dumpnode(node->y); printf(")"); break;
	case P_REP:
		printf(node->ng ? "NgRep(%d,%d," : "Rep(%d,%d,", node->m, node->n);
		reg_dumpnode(node->x);
		printf(")");
		break;
	case P_BOL: printf("Bol"); break;
	case P_EOL: printf("Eol"); break;
	case P_WORD: printf("Word"); break;
	case P_NWORD: printf("NotWord"); break;
	case P_PAR: printf("Par(%d,", node->n); reg_dumpnode(node->x); printf(")"); break;
	case P_PLA: printf("PLA("); reg_dumpnode(node->x); printf(")"); break;
	case P_NLA: printf("NLA("); reg_dumpnode(node->x); printf(")"); break;
	case P_ANY: printf("Any"); break;
	case P_CHAR: printf("Char(%c)", node->c); break;
	case P_CCLASS:
		printf("Class(");
		for (p = node->cc->spans; p < node->cc->end; p += 2) printf("%02X-%02X,", p[0], p[1]);
		printf(")");
		break;
	case P_NCCLASS:
		printf("NotClass(");
		for (p = node->cc->spans; p < node->cc->end; p += 2) printf("%02X-%02X,", p[0], p[1]);
		printf(")");
		break;
	case P_REF: printf("Ref(%d)", node->n); break;
	}
}

static void reg_dumpprog(RegProg *prog)
{
	RegInst *inst;
	int i;
	for (i = 0, inst = prog->start; inst < prog->end; ++i, ++inst) {
		printf("% 5d: ", i);
		switch (inst->opcode) {
		case I_END: puts("end"); break;
		case I_JUMP: printf("jump %d\n", (int)(inst->x - prog->start)); break;
		case I_SPLIT: printf("split %d %d\n", (int)(inst->x - prog->start), (int)(inst->y - prog->start)); break;
		case I_PLA: printf("pla %d %d\n", (int)(inst->x - prog->start), (int)(inst->y - prog->start)); break;
		case I_NLA: printf("nla %d %d\n", (int)(inst->x - prog->start), (int)(inst->y - prog->start)); break;
		case I_ANY: puts("any"); break;
		case I_ANYNL: puts("anynl"); break;
		case I_CHAR: printf(inst->c >= 32 && inst->c < 127 ? "char '%c'\n" : "char U+%04X\n", inst->c); break;
		case I_CCLASS: puts("cclass"); break;
		case I_NCCLASS: puts("ncclass"); break;
		case I_REF: printf("ref %d\n", inst->n); break;
		case I_BOL: puts("bol"); break;
		case I_EOL: puts("eol"); break;
		case I_WORD: puts("word"); break;
		case I_NWORD: puts("nword"); break;
		case I_LPAR: printf("lpar %d\n", inst->n); break;
		case I_RPAR: printf("rpar %d\n", inst->n); break;
		}
	}
}
#endif

RegProg *reg_comp(const char *pattern, int cflags, const char **errorp)
{
	RegNode *node;
	RegInst *split, *jump;
	int i, n;

	g.pstart = NULL;
	g.prog = NULL;

	if (setjmp(g.kaboom)) {
		if (errorp) *errorp = g.error;
		REG_FREE(g.pstart);
		REG_FREE(g.prog);
		return NULL;
	}

	g.prog = REG_MALLOC(sizeof (RegProg));
	if (!g.prog)
		reg_die("cannot allocate regular expression");
	n = strlen(pattern) * 2;
	if (n > 0) {
		g.pstart = g.pend = REG_MALLOC(sizeof (RegNode) * n);
		if (!g.pstart)
			reg_die("cannot allocate regular expression parse list");
	}

	g.source = pattern;
	g.ncclass = 0;
	g.nsub = 1;
	for (i = 0; i < REG_MAXSUB; ++i)
		g.sub[i] = 0;

	g.prog->flags = cflags;

	reg_next();
	node = reg_parsealt();
	if (g.lookahead == ')')
		reg_die("unreg_matched ')'");
	if (g.lookahead != 0)
		reg_die("syntax error");

#ifdef TEST
	reg_dumpnode(node);
	putchar('\n');
#endif

	n = 6 + reg_count(node);
	if (n < 0 || n > REG_MAXPROG)
		reg_die("program too large");

	g.prog->nsub = g.nsub;
	g.prog->start = g.prog->end = REG_MALLOC(n * sizeof (RegInst));

	split = reg_emit(g.prog, I_SPLIT);
	split->x = split + 3;
	split->y = split + 1;
	reg_emit(g.prog, I_ANYNL);
	jump = reg_emit(g.prog, I_JUMP);
	jump->x = split;
	reg_emit(g.prog, I_LPAR);
	reg_compile(g.prog, node);
	reg_emit(g.prog, I_RPAR);
	reg_emit(g.prog, I_END);

#ifdef TEST
	reg_dumpprog(g.prog);
#endif

	REG_FREE(g.pstart);

	if (errorp) *errorp = NULL;
	return g.prog;
}

void reg_free(RegProg *prog)
{
	if (prog) {
		REG_FREE(prog->start);
		REG_FREE(prog);
	}
}

/* Match */

static int reg_isnewline(int c)
{
	return c == 0xA || c == 0xD || c == 0x2028 || c == 0x2029;
}

static int reg_iswordchar(int c)
{
	return c == '_' ||
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9');
}

static int reg_incclass(RegClass *cc, RegRune c)
{
	RegRune *p;
	for (p = cc->spans; p < cc->end; p += 2)
		if (p[0] <= c && c <= p[1])
			return 1;
	return 0;
}

static int reg_incclassreg_canon(RegClass *cc, RegRune c)
{
	RegRune *p, r;
	for (p = cc->spans; p < cc->end; p += 2)
		for (r = p[0]; r <= p[1]; ++r)
			if (c == reg_canon(r))
				return 1;
	return 0;
}

static int strncmpreg_canon(const char *a, const char *b, unsigned int n)
{
	RegRune ra, rb;
	int c;
	while (n--) {
		if (!*a) return -1;
		if (!*b) return 1;
		a += reg_chartorune(&ra, a);
		b += reg_chartorune(&rb, b);
		c = reg_canon(ra) - reg_canon(rb);
		if (c)
			return c;
	}
	return 0;
}

static int reg_match(RegInst *pc, const char *sp, const char *bol, int flags, RegSub *out)
{
	RegSub scratch;
	int i;
	RegRune c;

	for (;;) {
		switch (pc->opcode) {
		case I_END:
			return 1;
		case I_JUMP:
			pc = pc->x;
			break;
		case I_SPLIT:
			scratch = *out;
			if (reg_match(pc->x, sp, bol, flags, &scratch)) {
				*out = scratch;
				return 1;
			}
			pc = pc->y;
			break;

		case I_PLA:
			if (!reg_match(pc->x, sp, bol, flags, out))
				return 0;
			pc = pc->y;
			break;
		case I_NLA:
			scratch = *out;
			if (reg_match(pc->x, sp, bol, flags, &scratch))
				return 0;
			pc = pc->y;
			break;

		case I_ANYNL:
			sp += reg_chartorune(&c, sp);
			if (c == 0)
				return 0;
			pc = pc + 1;
			break;
		case I_ANY:
			sp += reg_chartorune(&c, sp);
			if (c == 0)
				return 0;
			if (reg_isnewline(c))
				return 0;
			pc = pc + 1;
			break;
		case I_CHAR:
			sp += reg_chartorune(&c, sp);
			if (c == 0)
				return 0;
			if (flags & REG_ICASE)
				c = reg_canon(c);
			if (c != pc->c)
				return 0;
			pc = pc + 1;
			break;
		case I_CCLASS:
			sp += reg_chartorune(&c, sp);
			if (c == 0)
				return 0;
			if (flags & REG_ICASE) {
				if (!reg_incclassreg_canon(pc->cc, reg_canon(c)))
					return 0;
			} else {
				if (!reg_incclass(pc->cc, c))
					return 0;
			}
			pc = pc + 1;
			break;
		case I_NCCLASS:
			sp += reg_chartorune(&c, sp);
			if (c == 0)
				return 0;
			if (flags & REG_ICASE) {
				if (reg_incclassreg_canon(pc->cc, reg_canon(c)))
					return 0;
			} else {
				if (reg_incclass(pc->cc, c))
					return 0;
			}
			pc = pc + 1;
			break;
		case I_REF:
			i = out->sub[pc->n].ep - out->sub[pc->n].sp;
			if (flags & REG_ICASE) {
				if (strncmpreg_canon(sp, out->sub[pc->n].sp, i))
					return 0;
			} else {
				if (strncmp(sp, out->sub[pc->n].sp, i))
					return 0;
			}
			if (i > 0)
				sp += i;
			pc = pc + 1;
			break;

		case I_BOL:
			if (sp == bol && !(flags & REG_NOTBOL)) {
				pc = pc + 1;
				break;
			}
			if (flags & REG_NEWLINE) {
				if (sp > bol && reg_isnewline(sp[-1])) {
					pc = pc + 1;
					break;
				}
			}
			return 0;
		case I_EOL:
			if (*sp == 0) {
				pc = pc + 1;
				break;
			}
			if (flags & REG_NEWLINE) {
				if (reg_isnewline(*sp)) {
					pc = pc + 1;
					break;
				}
			}
			return 0;
		case I_WORD:
			i = sp > bol && reg_iswordchar(sp[-1]);
			i ^= reg_iswordchar(sp[0]);
			if (!i)
				return 0;
			pc = pc + 1;
			break;
		case I_NWORD:
			i = sp > bol && reg_iswordchar(sp[-1]);
			i ^= reg_iswordchar(sp[0]);
			if (i)
				return 0;
			pc = pc + 1;
			break;

		case I_LPAR:
			out->sub[pc->n].sp = sp;
			pc = pc + 1;
			break;
		case I_RPAR:
			out->sub[pc->n].ep = sp;
			pc = pc + 1;
			break;
		default:
			return 0;
		}
	}
}

int reg_exec(RegProg *prog, const char *sp, RegSub *sub, int eflags)
{
	RegSub scratch;
	int i;

	if (!sub)
		sub = &scratch;

	sub->nsub = prog->nsub;
	for (i = 0; i < REG_MAXSUB; ++i)
		sub->sub[i].sp = sub->sub[i].ep = NULL;

	return !reg_match(prog->start, sp, sp, prog->flags | eflags, sub);
}

#ifdef TEST
int main(int argc, char **argv)
{
	const char *error;
	const char *s;
	RegProg *p;
	RegSub m;
	unsigned int i;

	if (argc > 1) {
		p = reg_comp(argv[1], 0, &error);
		if (!p) {
			fprintf(stderr, "reg_comp: %s\n", error);
			return 1;
		}

		if (argc > 2) {
			s = argv[2];
			printf("nsub = %d\n", p->nsub);
			if (!reg_exec(p, s, &m, 0)) {
				for (i = 0; i < m.nsub; ++i) {
					int n = m.sub[i].ep - m.sub[i].sp;
					if (n > 0)
						printf("reg_match %d: s=%d e=%d n=%d '%.*s'\n", i, (int)(m.sub[i].sp - s), (int)(m.sub[i].ep - s), n, n, m.sub[i].sp);
					else
						printf("reg_match %d: n=0 ''\n", i);
				}
			} else {
				printf("no reg_match\n");
			}
		}
	}

	return 0;
}
#endif

#endif /* REGEXP_IMPLEMENTATION */
