#include "pd.h"

static struct {		/* Keywords */
	char	*name;
	int	kval;
} keywords[] = {
	"stop",	LSTOP,
	0,		0,
};

static struct {
	char *name;
	void (*func)(void);
} ops[] = {
	"add",	add,
	"sub",	sub,
	"mul",	mul,
	"div",	div,
	"mod",	mod,
	"neg",	negate,
	"gt",	gt,
	"lt",	lt,
	"le",	le,
	"ge",	ge,
	"eq",	eq,
	"ne",	ne,
	"and",	and,
	"or",	or,
	"not",	not,
	"now",	now,
	"dict",	dict,
	"exec",	xec,
	"pop",	xpop,
	"if",		ifcode,
	"xeff", 	xeff,
	"load",	load,
	"print",	printtop,
	"get",	get,
	"begin",	dictbegin,
	"end",	dictend,
	"def",	def,
	0, 0
};

static struct {		/* Constants */
	char *name;
	double cval;
} consts[] = {
	"PI",	 3.14159265358979323846,
	"E",	 2.71828182845904523536,
	"GAMMA", 0.57721566490153286060,  /* Euler */
	"DEG",	57.29577951308232087680,  /* deg/radian */
	"PHI",   1.61803398874989484820,  /* golden ratio */
	0,	 0
};

static struct {		/* Built-ins */
	char *name;
	double	(*func)(double);
} builtins[] = {
	"sin",	sin,
	"cos",	cos,
	"tan",	tan,
	"atan",	atan,
	"asin",	Asin,	/* checks range */
	"acos",	Acos,	/* checks range */
	"sinh",	Sinh,	/* checks range */
	"cosh",	Cosh,	/* checks range */
	"tanh",	tanh,
	"log",	Log,	/* checks range */
	"log10",	Log10,	/* checks range */
	"exp",	Exp,	/* checks range */
	"sqrt",	Sqrt,	/* checks range */
	"int",	integer,
	"abs",	fabs,
	0,	0
};

void
init(void)	/* install constants and built-ins in table */
{
	int i;
	Symbol *s;
	for (i = 0; keywords[i].name; i++)
		install(keywords[i].name, keywords[i].kval, 0.0);
	for (i = 0; consts[i].name; i++)
		install(consts[i].name, LVAR, consts[i].cval);
	for (i = 0; builtins[i].name; i++) {
		s = install(builtins[i].name, LBLTIN, 0.0);
		s->u.ptr = (void*) builtins[i].func;
	}
	for (i = 0; ops[i].name; i++) {
		s = install(ops[i].name, LOPS, 0.0);
		s->u.ptr = (void*) ops[i].func;
	}
}
