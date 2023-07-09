#include "pd.h"

int
toggle(Btree *b, Entry *e)
{
	static Entry * last;

	if(e && b->ett && e->tt > b->ett)
		return 1;
	if(last && e && last->vt == e->vt && 
	!strcmp(last->key, e->key) && !strcmp(last->val, e->val)) {
		last = e = 0;
		USED(last);
	}else if(last)
		(*b->fn)(last);
	last = e;
	return 1;
}

int
found(Entry *e)
{
	fprint(1, "%ld\t%s\t%s\n", e->vt, e->key, e->val);
	return 1;
}

void
bfn(Btree *b, int (*f)(Entry *))
{
	b->fn = f;
}

int
btree(Btree *b, char *f, int flag) 
{
	int i;
	uchar buf[HEADR];
	uchar *p;

	memset(b, 0, sizeof(Btree));
	b->name = strdup(f);
	b->next = 0;
	b->inversion = 1;
	b->blist = 0;
	b->fn = found;
	if(access(f, 0) < 0) {
		if((flag & ORDWR) && ((b->fd = create(f, ORDWR, 0666)) < 0)) {
			fprint(2, "btree: cannot create file %s: %r\n", f);
			exits("create");
		} else if(flag == OREAD) {
			fprint(2, "%s: No such file\n", f);
			exits("access");
		}
		close(b->fd);
	}	
	if((b->fd = open(f, flag)) < 0) {
		fprint(2, "btree: cannot open %s: %r\n", f);
		exits("open");
	}
	p = buf;
	i = read(b->fd, buf, HEADR);
	if(i == 0)
		return 1;
	if(i < 0) {
		fprint(2, "read header error");
		exits("header");
	} else {
		b->cnt = GBIT32(p);
		p += BIT32SZ;
		for(i = 0; i < 4; i++) {
			b->H[i] = GBIT32(p);
			p += BIT32SZ;
		}
		for (i = 0;  i < 4; i++) {
			b->head[i] = GBIT32(p);
			p += BIT32SZ;
		}
	}
	return 1;
}

void
bflush(Btree *b)
{
	Block *p;
	uchar buf[HEADR], *bp;
	int i;

	bp = buf;
	PBIT32(bp, b->cnt);
	bp += BIT32SZ;
	for(i = 0; i < 4; i++) {
		PBIT32(bp, b->H[i]);
		bp += BIT32SZ;
	}
	for (i = 0;  i < 4; i++) {
		PBIT32(bp, b->head[i]);
		bp += BIT32SZ;
	}
	seek(b->fd, 0, 0);
	write(b->fd, buf, HEADR);
	for(p = b->blist; p != 0; p = p->next)
		putblock(b->fd, p);
}

Block *
lookup(Btree *b, uint n)
{
	Block *p;
	for(p = b->blist; p != 0; p = p->next)
		if(p->seq == n)
			return p;
	return B;
}

int
epack(Block *b, uchar *buf, int nbuf)
{
	uchar *p;
	Entry **e;
	int ts=0;

	if(debug)
		fprint(2, "pack %d, nent %d, size %d\n", b->seq, b->m, b->size);
	p = buf;
	for (e = b->ents; e < b->ents + b->m; e++) {
		ts += (*e)->size;
		if(ts > nbuf) {
			fprint(2, "warning: too many ents\n");
			break;
		}
		convE2M(*e, p, (*e)->size);
		p += (*e)->size;
	}
	return p - buf;
}

int
eunpack(uchar *buf, long ts, Block *b)
{
	char *s;
	long ss, i, n, nn, m;
	Entry *e;

	if(debug)
		fprint(2, "unpack %d\n", b->seq);
	if(ts <= 0)
		return 0;

	ss = 0;
	n = 0;
	for(i = 0; i < ts; i += m){
		m = GBIT16(&buf[i]);
		ss += m;
		n++;
	}
	if(i != ts)
		return -1;

	e = malloc(n * sizeof(Entry));
	if(e == nil)
		return -1;

	s = malloc(ss);
	nn = 0;
	for(i = 0; i < ts; i += m){
		m = GBIT16(&buf[i]);
		if(nn >= n || convM2E(&buf[i], m, e + nn, s) != m){
			free(e);
			return -1;
		}
		addentry(b, e + nn, nn, AZIP);
		nn++;
		s += m;
	}
	if(debug)
		fprint(2, "unpack: %d nent %d size %d\n", b->seq, b->m, b->size);
	return nn;
}

uint
convM2E(uchar *buf, uint nbuf, Entry *e, char *strs)
{
	uchar *p, *ebuf;
	char *sv[2];
	int i, ns;

	p = buf;
	ebuf = buf + nbuf;

	e->size = GBIT16(p);
	p += BIT16SZ;
	e->zip = GBIT8(p);
	p += BIT8SZ;
	e->tt = GBIT32(p);
	p += BIT32SZ;
	e->vt = GBIT32(p);
	p += BIT32SZ;

	for(i = 0; i < 2; i++){
		if(p + BIT16SZ > ebuf)
			return 0;
		ns = GBIT16(p);
		p += BIT16SZ;
		if(p + ns > ebuf)
			return 0;
		if(strs){
			sv[i] = strs;
			memmove(strs, p, ns);
			strs += ns;
			*strs++ = '\0';
		}
		p += ns;
	}

	if(strs){
		e->key = sv[0];
		e->val = sv[1];
	}else{
		e->key = "";
		e->val = "";
	}
	
	if(debug)
		fprint(2, "%ld\t%ld\t%d\t%s\t%s\n", e->tt, e->vt, e->zip, e->key, e->val);
	return p - buf;
}

uint
convE2M(Entry *e, uchar *buf, uint nbuf)
{
	uchar *p;
	int ns;

	p = buf;

	PBIT16(p, e->size);
	p += BIT16SZ;
	PBIT8(p, e->zip);
	p += BIT8SZ;

	PBIT32(p, e->tt);
	p += BIT32SZ;
	PBIT32(p, e->vt);
	p += BIT32SZ;

	if(!e->key || (e->zip & ZKEY))
		ns = 0;
	else
		ns = strlen(e->key);
	PBIT16(p, ns);
	p += BIT16SZ;
	if(ns)
		memmove(p, e->key, ns);
	p += ns;

	if(!e->val || (e->zip & ZVAL))
		ns = 0;
	else
		ns = strlen(e->val);
	PBIT16(p, ns);
	p += BIT16SZ;
	if(ns)
		memmove(p, e->val, ns);
	p += ns;

	if(e->size != p - buf)
		return 0;
	USED(nbuf);
	return p - buf;
}

uchar buf[BLOCK];

Block*
getblock(Btree *b, uint id) 
{
	int i;
	Block *n;
	uchar *p;
	int sz;
	
	i = 0;
	if(debug)
		fprint(2, "getblock %d\n", id);
	if(n = lookup(b, id)) {
		if(debug)
			fprint(2, "found block in lookup\n");
		return n;
	}
	if((n = (Block *) malloc(sizeof(Block))) == B) { 
		fprint(2, "malloc error\n");
		exits("malloc");
	}else if(seek(b->fd, BLOFFSET(id), 0) < 0) {
		fprint(2, "seek error\n");
		exits("seek");
	}else if((i=read(b->fd, buf, BLOCK)) < 0) {
		fprint(2, "read error: %r\n");
		exits("read");
	}
	if(debug)
		fprint(2, "read returned %d\n", i);
	n->seq = id;
	n->size = BIT16SZ;
	n->ents = 0;
	n->m = 0;
	n->esize = 0;
	n->next = B;
	if(i > 0) {
		p = buf;
		sz = GBIT16(p);
		p += BIT16SZ;
		if(eunpack(p, sz - BIT16SZ, n) < 0) {
			fprint(2, "unpack error");
			exits("unpack");
		}
	}
	n->next = b->blist;
	b->blist = n;
	return n;
}

void 
dumpblock(Block *b)
{	
	Entry **e;
	
	fprint(2, "dump seq %d, m %d\n", b->seq, b->m);
	for(e = b->ents; e < b->ents + b->m; e++) {
		fprint(2, "tt %ld, vt %ld, key %s, val %s\n", 
			(*e)->tt, (*e)->vt, (*e)->key, (*e)->val);
	}
}


void
putblock(int fd, Block *b)
{
	int off;
	uchar buf[BLOCK];

	if(debug)
		dumpblock(b);
	if((off=seek(fd, BLOFFSET(b->seq), 0)) < 0){
		fprint(2, "tri: seek error\n");
	}
	if(debug)
		fprint(2,"block %d seek %d, size %d\n", b->seq, off, b->size);
	PBIT16(buf, b->size);
	epack(b, buf + BIT16SZ, BLOCK - BIT16SZ);
	if(write(fd, buf, BLOCK) < 0) {
		fprint(2, "tri: write error: %r\n");
		exits("write");
	}
	USED(off);
}

int
insert(Btree *b, ulong tt, ulong vt, char *key, char *val, uint inv)
{
	Entry *u0, *u1, *e;
	Block *t, *h;

	u0 =  0;
	if(debug)
		fprint(2, "insert: %ld %ld %s %s, %d\n", tt, vt, key, val, inv);
	if(inv > 3) {
		fprint(2, "bad inversion %d\n", inv);
		return 0;
	}
	b->inversion = inv;
	if(b->head[inv] == 0) {
		b->cnt++;
		b->head[inv] = b->cnt;
	}
	h = getblock(b, b->head[inv]);
	e = new(tt, vt, key, val);
	u1 = insertR(b, h, e, b->H[inv]);
	if(u1 == E)
		return 1;
	t = getblock(b, ++(b->cnt));
	switch(inv) {
	case 1:
		u0 = new(h->seq, h->ents[0]->vt, 0, 0);
		break;
	case 2:
		u0 = new(h->seq, h->ents[0]->vt, h->ents[0]->key, 0);
		break;
	case 3:
		u0 = new(h->seq, h->ents[0]->vt, 0, h->ents[0]->val);
		break;
	}
	addentry(t, u0, 0, AZIP);
	addentry(t, u1, 1, AZIP);	
	b->head[inv] = t->seq;
	(b->H[inv])++;
	return 1;
}

/*
 *	acomp(s, t) returns:
 *		-2 if s strictly precedes t
 *		-1 if s is a prefix of t
 *		0 if s is the same as t
 *		1 if t is a prefix of s
 *		2 if t strictly precedes s
 */
int
acomp(char *s, char *t)
{
	int cs, ct;

	for(;;) {
		cs = *s;
		ct = *t;
		if(cs != ct)
			break;
		if(cs == 0)
			return 0;
		s++;
		t++;
	}
	if(cs == 0)
		return -1;
	if(ct == 0)
		return 1;
	if(cs < ct)
		return -2;
	return 2;
}


int
less(Btree *b, Entry  *s, Entry  *t)
{
	int i = 0;

	if(b->inversion == 1) {
		i = (s->vt >= t->vt);
	}else if(b->inversion == 2) {
		i = acomp(s->key, t->key);
		if(i == 0)
			i = (s->vt >= t->vt);
		if(debug)
			fprint(2, "less %s,%s=%d\n", s->key, t->key, i);
	} else if(b->inversion == 3) {
		i = acomp(s->val, t->val);
		if(i == 0)
			i = (s->vt >= t->vt);
		if(debug)
			fprint(2, "less %s,%s=%d\n", s->val, t->val, i);
	}
	return (i <= 0) ? 1 : 0;
}

Entry* 
insertR(Btree *b, Block *h, Entry *e, uint H)
{
	int j = 0;
	Entry  *u, *x;

	x = e;
	if(debug)
		fprint(2, "insert H %d, head %d: %ld %ld %s %s\n", 
			H, h->seq, e->tt, e->vt, e->key, e->val);
	if(H == 0) {
		for(j = 0; j < h->m; j++)
			if(less(b, e, h->ents[j]))
				break;
	}
	if(H != 0) {
		for(j = 0; j < h->m; j++) {
			if((j + 1 == h->m) || less(b, e, h->ents[j+1])) {
				u = insertR(b, getblock(b, h->ents[j++]->tt), e, H-1);
				if(u == E)
					return E;
				x = u;
				break;
			}
		}
	}

	addentry(h, x, j, AZIP);
	if(h->size < BLOCK)
		return E;
	else
		return split(b, h);
}

Entry*
new(ulong tt, ulong vt, char *key, char *val)
{
	Entry *e;
	long ns;

	e = (Entry*) malloc(sizeof(Entry));
	if(e == E)
		return E;
	e->tt = tt;
	e->vt = vt;
	if(key) {
		ns = strlen(key);
		e->key = strdup(key);
	} else {
		ns = 0;
		e->key = "";
	}
	if(val) {
		ns += strlen(val);
		e->val = strdup(val);
	} else {
		ns += 0;
		e->val = "";
	}
	e->zip = ZDO;
	e->size = ENTFIXLEN + ns;
	return e;
}	

int
addentry(Block *h, Entry *e, int j, uchar flag)
{
	Entry **evp;
	int i;

	if(debug)
		fprint(2,"add j = %d, tot %d blk %d\n", j, h->m, h->seq);
	if(h->ents == 0) {
		h->ents = (Entry**) malloc(EINIT * sizeof(Entry*));
		if(h->ents == 0)
			return -1;
		h->esize = EINIT;
		h->m = 0; 
	}else if(h->m >= h->esize) {	
		evp = (Entry**) realloc(h->ents, (h->esize * EGROW) * sizeof(Entry*));
		if(evp == 0)
			return -1;
		h->esize *= EGROW;
		h->ents = evp;
	}
	if(j > h->m)
		return -1;
	for(i = (h->m)++; (i > j); i--)
		h->ents[i] = h->ents[i-1];
	h->ents[j] = e;
	h->size += e->size;
	if(flag & AZIP)
		zipe(h, j);
	return h->m;
}

void
zipe(Block *h, int j)
{
	int zip, len;
	Entry *e, *l, *n;
	
	e = h->ents[j];
	if (j > 0)
		l = h->ents[j - 1];
	else
		l = 0;
	if ((j + 1) < h->m)
		n = h->ents[j + 1];
	else
		n = 0;
	zip = h->ents[j]->zip;

	if((zip & ZDO) && (j == 0)) {
			e->zip = ZNONE;
	} else if (zip & ZDO) {
		e->zip = ZNONE;
		if(l &&  e->key && l->key && (strcmp(e->key, l->key) == 0)) {
			e->zip |= ZKEY;
			len = strlen(e->key);
			e->size -= len;
			h->size -= len;
		} else if(n && (n->zip & ZKEY)) {
			n->zip ^= ZKEY;
			len = strlen(n->key);
			n->size += len;
			h->size += len;
		} else if(n && !(n->zip & ZKEY) && e->key && n->key && (strcmp(e->key, n->key) == 0)) {
			n->zip |= ZKEY;
			len = strlen(n->key);
			n->size -= len;
			h->size -= len;
		}
		if(debug)
			fprint(2, "zip key  zipflag %d\n", e->zip);

		if(l && e->val && l->val && (strcmp(e->val, l->val) == 0)) {
			e->zip |= ZVAL;
			len = strlen(e->val);
			e->size -= len;
			h->size -= len;
		} else if(n && (n->zip & ZVAL)) {
			n->zip ^= ZVAL;
			len = strlen(n->val);
			n->size += len;
			h->size += len;
		} else if(n && !(n->zip & ZVAL) && e->val && n->val && (strcmp(e->val, n->val) == 0)) {
			n->zip |= ZVAL;
			len = strlen(n->val);
			n->size -= len;
			h->size -= len;
		}
		if(debug)
			fprint(2, "zip val  zipflag %d\n", e->zip);
	} else if((j == 0) && (zip != ZNONE)) {
		if(zip & ZKEY) {
			len = strlen(e->key);
			e->size += len;
			h->size += len;
		}
		if(zip & ZVAL) {
			len = strlen(e->val);
			e->size += len;
			h->size += len;
		}
		e->zip = ZNONE;
	} else {
		if(zip & ZKEY)
			e->key = l->key;
		if(zip & ZVAL)
			e->val = l->val;
	}
}

Entry*
split(Btree *b, Block *h)
{
	int j, k, ts;
	Entry *link;
	Block *t;
	
	if(debug)
		fprint(2, "split block %d\n", h->seq);
	link = 0;
	ts = 0;
	t = getblock(b, ++(b->cnt));
	for(j = 0; j < h->m; j++) {
		ts += h->ents[j]->size;
		if(ts >= BLOCK/2)
			break;
	}
	ts = 0;
	for(k = 0; k + j < h->m; k++) {
		ts += h->ents[k + j]->size;
		addentry(t, h->ents[k+j], k, AZIP);
	}
	h->m -= k;
	h->size -= ts;
	switch(b->inversion) {
	case 1:
		link = new(t->seq, t->ents[0]->vt, 0, 0);
		break;
	case 2:
		link = new(t->seq, t->ents[0]->vt, t->ents[0]->key, 0);
		break;
	case 3:
		link = new(t->seq, t->ents[0]->vt, 0, t->ents[0]->val);
		break;
	}
	if (debug)
		fprint(2, "return link: %ld %ld %s\n", link->tt, link->vt, link->key);
	return link;
}

int
search(Btree *b, char *k, char *r, uint inv)
{
	uint head, H;
	if(inv > 3 || b->head[inv] == 0) {
		fprint(2, "bad inversion %d\n", inv);
		return 0;
	}
	b->inversion = inv;
	H = b->H[inv];
	head = b->head[inv];
	searchR(b, getblock(b, head), k, H, r);
	toggle(b, 0);
	return 1;
}

int
comp(Btree *b, Entry *s, char *t)
{
	uint i;
	if(b->inversion == 1) {
		i = atoi(t);
		if(s->vt < i)
			return -1;
		else if(s->vt == i)
			return 0;
		else if(s->vt > i)
			return 1;
	}else if(b->inversion == 2) {
		return acomp(s->key, t);
	}else if(b->inversion == 3) {
		return acomp(s->val, t);
	}
	return 0;
}

int
searchR(Btree *b, Block *h, char *k, uint H, char *r)
{
	int n;
	Entry **e, **ee;
	int found=0;

	ee = h->ents + h->m;
	if(debug)
		fprint(2, "searchR seq %d, k %s, H %d, r %s.\n", h->seq, k, H, r);
	if(H == 0) {
		/* If r is set entry is in range k <= e <= r
		 * else if r not set k precedes or is a prefix of e
		 */
		for(e=h->ents; e < ee; e++)
			if(((n = comp(b, *e, k)) >= 0) && ((r && comp(b, *e, r) <= 0) || (!r && n < 2)))
				toggle(b, *e);
			else if((r && comp(b, *e, r) > 0) || (!r && n > 1))
				return 0;
		return 1;
	}
	if(H != 0) {
		for(e=h->ents; e < ee; e++) {
			if((e + 1 == ee) || (comp(b, *(e+1), k) >= 0) || found) {
				found=1;
				if(!searchR(b, getblock(b, (*e)->tt), k, H-1, r))
					return 0;
			}
		}
	}
	return 1;
}

void
coinR(Btree *b, Block *h, Entry *e, uint H)
{
	int j = 0;

	if(debug)
		fprint(2, "coin H %d, head %d: %ld %ld %s %s\n", 
			H, h->seq, e->tt, e->vt, e->key, e->val);
	if(H == 0) {
		for(j = 0; j < h->m; j++)
			if(less(b, e, h->ents[j]))
				break;
	}
	if(H != 0) {
		for(j = 0; j < h->m; j++) {
			if((j + 1 == h->m) || less(b, e, h->ents[j+1])) {
				coinR(b, getblock(b, h->ents[j++]->tt), e, H-1);
				break;
			}
		}
	}
	if(H == 0 && j>0)
		print("%ld\t%s\t%s\n", e->vt, e->key, h->ents[j-1]->key);
}

int
coin(Btree *b, ulong vt, char *key, char *val, uint inv)
{
	Entry e;

	if(inv > 3 || b->head[inv] == 0) {
		fprint(2, "bad inversion %d\n", inv);
		return 0;
	}
	b->inversion = inv;
	e.vt = vt;
	e.key=key;
	e.val=val;
	coinR(b, getblock(b, b->head[inv]), &e, b->H[inv]);
	return 1;
}

typedef	struct	Sym	Sym;

#define	ETINIT	1
#define	ETGROW	2
#define	MULT	31
#define	NHASH	126

struct	Etab
{
	int	nval;
	int	max;
	Entry**	e;
} etab;

struct	Sym 
{
	char*	key;
	char*	val;
	int	value;
	Sym*	next;
} *symtab[NHASH];


ulong	evt;
int	tflag;
int	addet(Entry*);
void	clearhash(void);
uint	hash(Entry*);
int	looktog(Entry*, int);
int	sjc(long current, long pair[]);

int
addet(Entry *e)
{
	Entry ** evp;
	if(debug)
		fprint(2, "addet %ld %s %s\n", e->vt, e->key, e->val);
	if(etab.e == 0) {
		etab.e = (Entry **) malloc(ETINIT * sizeof(Entry *));
		if(etab.e == 0)
			return -1;
		etab.max = ETINIT;
		etab.nval = 0;
	}else if(etab.nval >= etab.max) {  /*grow*/
		evp = (Entry **) realloc(etab.e, (ETGROW * etab.max) * sizeof(Entry *));
		if(evp == 0)
			return -1;
		etab.max *= ETGROW;
		etab.e = evp;
	}
	if(tflag && e->vt <= evt)
		looktog(e, 1);
	etab.e[etab.nval] = e;
	return etab.nval++;
}


uint
hash(Entry *e)
{
	uint h;
	uchar *p;
	
	h = 0;
	for(p = (uchar *) e->key; *p != '\0'; p++)
		h = MULT * h + *p;
	for(p = (uchar *) e->val; *p != '\0'; p++)
		h = MULT * h + *p;
	return h % NHASH;
}

int 
looktog(Entry *e, int tog)
{
	int h;
	Sym *sym;
	
	h = hash(e);
	for(sym = symtab[h]; sym; sym = sym->next)
		if((e->key == sym->key && e->val == sym->val) || 
			(strcmp(e->key, sym->key) == 0 && strcmp(e->val, sym->val) == 0))
			break;
	if(!sym) {
		sym = (Sym *) malloc(sizeof(Sym));
		sym->key = e->key;
		sym->val = e->val;
		sym->value = 0;
		sym->next = symtab[h];
		symtab[h] = sym;
	}
	if(tog)
		sym->value ^= 1;
	return sym->value;
}

void
clearhash(void)
{
	Sym *sym, *next;
	int i;
	
	for(i = 0; i < NHASH; i++) {
		for(sym = symtab[i]; sym; sym = next) {
			next = sym->next;
			free(sym);
		}
		symtab[i] = 0;
	}
}

int
join(Btree *b, ulong vt, char *s, char *t, int inv, int flags, Block *r)
{
	int j;
	long pair[2];

	USED(t);
	b->fn = addet;
	tflag = flags & JTOGGLE;
	evt = vt;
	pair[0] = pair[1] = -1;
	if(flags & JRANGE)
		search(b, s, t, inv);
	else if(flags & JEXACT)
		search(b, s, s, inv);
	else if(flags & JPREFIX)
		search(b, s, 0, inv);
	if(debug)
		fprint(2, "nval %d\n", etab.nval);
	for(j = 0; j < etab.nval; j++) {
		if(flags & JMANY) {
			addentry(r, etab.e[j], r->m, 0);
		} else if(flags & JTOGGLE) {
			if(looktog(etab.e[j], 0) && etab.e[j]->vt <= vt) {
				addentry(r, etab.e[j], r->m, 0);
			}
		} else if(flags & JSINGLE) {
			pair[0] = etab.e[j]->vt;
			if(j == (etab.nval - 1))
				pair[1] = -1;
			else if (inv == 2 && (etab.e[j]->key == etab.e[j + 1]->key 
				|| strcmp(etab.e[j]->key, etab.e[j + 1]->key) == 0))
				pair[1] = etab.e[j + 1]->vt;
			else if (inv == 3 && (etab.e[j]->val == etab.e[j + 1]->val 
				|| strcmp(etab.e[j]->val, etab.e[j + 1]->val) == 0))
				pair[1] = etab.e[j + 1]->vt;
			else
				pair[1] = -1;
			if(sjc(vt, pair))
				addentry(r, etab.e[j], r->m, 0);
		}
	}
	etab.nval=0;
	if(flags & JTOGGLE)
		clearhash();
	return r->m;
}

int
sjc(long current, long pair[]) 
{
	if(debug)
		fprint(2, "sjc %ld %ld %ld\n", pair[0], current, pair[1]);
	if(pair[0] == -1)
		return 0;
	else if(pair[1] == -1)
		if(pair[0] <= current)
			return 1;
		else
			return 0;
	else if(pair[0] <= current && current < pair[1])
		return 1;
	else if(pair[0] == current && current == pair[1])
		return 1;
	else
		return 0;
}
