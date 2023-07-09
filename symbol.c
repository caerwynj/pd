#include "pd.h"

static Symbol *symlist = 0;  /* symbol table: linked list */
Btree	*dictstk = 0; /* dictionary: stack */

Symbol*
slookup(char* s)	/* find s in symbol table */
{
	Symbol *sp;

	dprintf(2, "slookup %s\n", s);
	for (sp = symlist; sp != (Symbol *) 0; sp = sp->next)
		if (strcmp(sp->name, s) == 0)
			return sp;
	return 0;	/* 0 ==> not found */	
}

Symbol*
install(char* s, int t, double d)  /* install s in symbol table */
{
	Symbol *sp;

	sp = emalloc(sizeof(Symbol));
	sp->name = emalloc(strlen(s)+1); /* +1 for '\0' */
	strcpy(sp->name, s);
	sp->type = t;
	sp->u.val = d;
	sp->next = symlist; /* put at front of list */
	symlist = sp;
	return sp;
}

void*
emalloc(unsigned n)	/* check return from malloc */
{
	char *p;

	p = malloc(n);
	if (p == 0)
		execerror("out of memory", (char *) 0);
	return p;
}

char*
dlookup(char* s)
{
	Btree *b;
	static Block r = {0, 0, 0, 0, 0};

	dprintf(2, "dblookup %s\n", s);
	r.m = 0;
	for(b = dictstk; b; b = b->next){
		join(b, time(0), s, 0, 2, JSINGLE|JEXACT, &r);
		if(r.m == 1)
			return r.ents[0]->val;
	}
	return nil;
}
