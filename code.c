#include "pd.h"

#define	NSTACK	256

static Datum stack[NSTACK];	/* the stack */
static Datum *stackp;		/* next free spot on stack */

void
initcode(void)
{
	stackp = stack;
}

void
push(Datum d)
{
	if (stackp >= &stack[NSTACK])
		execerror("stack too deep", 0);
	*stackp++ = d;
}

Datum
pop(void)
{
	if (stackp == stack)
		execerror("stack underflow", 0);
	return *--stackp;
}

void
xpop(void)	/* for when no value is wanted */
{
	if (stackp == stack)
		execerror("stack underflow", (char *)0);
	--stackp;
	if(stackp->type == DSTR)
		release(stackp->p);
}

void
constpush(void)
{
	Datum d;
	d.type = DNUM;
	d.val = sym->u.val;
	push(d);
}

void
varpush(void)
{
	Datum d;
	d.type = DSYM;
	d.sym = sym;
	push(d);
}

void
strpush(void)
{
	Datum d;
	d.type = DSTR;
	d.p = (Blk*)sym;
	push(d);
}

void
whilecode(void)
{
}

void
forcode(void)
{
}

void
ifcode(void) 
{
	Datum d1, d2;

	d1 = pop();
	d2 = pop();
	if (d2.val)
		pushio(d1.p);
}

void
xec(void)
{
	Datum d;
	d = pop();
	pushio(d.p);
}

void
bltin(void) 
{

	Datum d;
	d = pop();
	d.val = (*(double (*)(double))sym->u.ptr)(d.val);
	push(d);
}

void
add(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val += d2.val;
	push(d1);
}

void
sub(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val -= d2.val;
	push(d1);
}

void
mul(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val *= d2.val;
	push(d1);
}

void
div(void)
{
	Datum d1, d2;
	d2 = pop();
	if (d2.val == 0.0)
		execerror("division by zero", (char *)0);
	d1 = pop();
	d1.val /= d2.val;
	push(d1);
}

void
mod(void)
{
	Datum d1, d2;
	d2 = pop();
	if (d2.val == 0.0)
		execerror("division by zero", (char *)0);
	d1 = pop();
	/* d1.val %= d2.val; */
	d1.val = fmod(d1.val, d2.val);
	push(d1);
}

void
negate(void)
{
	Datum d;
	d = pop();
	d.val = -d.val;
	push(d);
}

void
verify(Symbol* s)
{
	if (s->type != LVAR && s->type != LUNDEF)
		execerror("attempt to evaluate non-variable", s->name);
	if (s->type == LUNDEF)
		execerror("undefined variable", s->name);
}

void
eval(void)		/* evaluate variable on stack */
{
	Datum d;
	d = pop();
	verify(d.sym);
	d.val = d.sym->u.val;
	push(d);
}

void
gt(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val > d2.val);
	push(d1);
}

void
lt(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val < d2.val);
	push(d1);
}

void
ge(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val >= d2.val);
	push(d1);
}

void
le(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val <= d2.val);
	push(d1);
}

void
eq(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val == d2.val);
	push(d1);
}

void
ne(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val != d2.val);
	push(d1);
}

void
and(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val != 0.0 && d2.val != 0.0);
	push(d1);
}

void
or(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val != 0.0 || d2.val != 0.0);
	push(d1);
}

void
not(void)
{
	Datum d;
	d = pop();
	d.val = (double)(d.val == 0.0);
	push(d);
}

void
power(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = Pow(d1.val, d2.val);
	push(d1);
}

void
assign(void)
{
	Datum d1, d2;
	d1 = pop();
	d2 = pop();
	if (d1.sym->type != LVAR && d1.sym->type != LUNDEF)
		execerror("assignment to non-variable",
			d1.sym->name);
	d1.sym->u.val = d2.val;
	d1.sym->type = LVAR;
	push(d2);
}

void
printcnt(void)
{
	print("(%d) ", stackp - stack);
}

void
printtop(void)	/* pop top value from stack, print it */
{
	Datum d;
	static Symbol *s;	/* last value computed */
	if (s == 0)
		s = install("_", LVAR, 0.0);
	d = pop();
	if(d.type == DNUM) {
		print("%.12g\n", d.val);
		s->u.val = d.val;
	} else if(d.type == DSTR) {
		rewind(d.p);
		while(sfeof(d.p) == 0) {
			print("%c",sgetc(d.p));
		}
		print("\n");
		release(d.p);
		return;
	}
}

void
prexpr(void)	/* print numeric value */
{
	Datum d;
	d = pop();
	print("%.12g ", d.val);
}

void
now(void)
{
	Datum d;
	d.type=DNUM;
	d.val = time(0);
	push(d);
}

void
dict(void)
{
	Datum d;
	Btree *b;

	d = pop();
	if((b = malloc(sizeof(Btree))) == 0)
		execerror("btree", "");
	btree(b, d.p->beg, OREAD);
	release(d.p);
	d.type = DDICT;
	d.b = b;
	push(d);
}

void
xeff(void)
{
	Datum d1, d2, d3, d4;
	char *p;
	Block r = {0, 0, 0, 0, 0, 0};

	d4 = pop();
	d3 = pop();
	d2 = pop();
	d1 = pop();
	sputc(d2.p, 0);
	sputc(d3.p, 0);
	join((Btree*)d4.sym->u.ptr, (long)d1.val, d2.p->beg, d3.p->beg, 2, JSINGLE|JEXACT, &r);
	if(debug)
		fprint(2, "xeff ret %d\n", r.m);
	if(r.m == 0)
		return;
	d1.val = r.ents[0]->vt;
	push(d1);
	screate(d2.p);
	for(p = r.ents[0]->key; *p; p++)
		sputc(d2.p, *p);
	push(d2);
	screate(d3.p);
	for(p = r.ents[0]->val; *p; p++)
		sputc(d3.p, *p);
	push(d3);
}

void
load(void)
{
	Datum d1, d2, d3, d4, d5;
	Block *r;

	d5 = pop();
	d4 = pop();
	d3 = pop();
	d2 = pop();
	d1 = pop();
	sputc(d2.p, 0);
	sputc(d3.p, 0);
	r = emalloc(sizeof(Block));
	memset(r, 0, sizeof(Block));
	join((Btree*)d4.sym->u.ptr, (long)d1.val, d2.p->beg, d3.p->beg, 2, (int)d5.val, r);
	d1.type = DDICT;
	/* Convert Block to Btree here!! */
/*	d1.b = r;
	if(debug)
		fprint(2, "result set: %d\n", d1.b->m);
	release(d3.p);
	release(d2.p);
	push(d1);
*/
}

void
get(void)
{
}

void
forall(void)
{
/*
	Datum d1, d2, d3, d4, d5;
	Block *r;
	int i;
	char *p, *s = " stop";
	d2 = pop();
	for(p = s; *p; p++)
		sputc(d2.p, *p);
	d1 = pop();
	r = d1.b;
	for(i = 0; i < r->m; i++) {
		d3.val = r->ents[i]->vt;
		push(d3);
		d4.p = salloc(0);
		for(p = r->ents[i]->key; *p; p++)
			sputc(d4.p, *p);
		push(d4);
		d5.p = salloc(0);
		for(p = r->ents[i]->val; *p; p++)
			sputc(d5.p, *p);
		push(d5);
		pushio(d2.p);
		run();
	}
*/
}

char*
datum2str(Datum d, char *s)
{
	switch(d.type){
	default:
		execerror("invalid type", 0);
	case DNUM:
		sprint(s, "%g", d.val);
		break;
	case DSTR:
	case DCODE:
		s = d.p->beg;
		break;
	case DDICT:
		s = d.b->name;
		break;
	case DSYM:
		s = d.sym->name;
		break;
	case DLIST:
		break;
	case DMARK:
		strcpy(s, "mark");
		break;
	}
	return s;
}

void
def(void)
{
	Datum d1, d2, d3;
	char s[256];

	d3 = pop();	/*val(any)*/
	d2 = pop();	/*key(sym)*/
	d1 = pop();	/*time(num)*/
	datum2str(d3, s);
	insert(dictstk, time(0), d1.val, d2.p->beg, s, 2);
}

void
dictbegin(void)
{
	Datum d;

	d = pop();
	d.b->next = dictstk;
	dictstk = d.b;
}

void
dictend(void)
{
	Btree *b;

	b = dictstk->next;
	/* Free the Btree here! */
	dictstk = b;
}
