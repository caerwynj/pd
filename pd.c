#include "pd.h"

Blk	*readstk[RDSKSZ];
Blk	**readptr;
Blk	*hfree;
int	lastchar;
long	rel;
long	nbytes;
long	all;
long	headmor;
long	longest, maxsize, active;
int	lall, lrel, lcopy, lmore, lbytes;
Biobuf	*curfile;

char	*progname;
int	lineno = 1;
jmp_buf	begin;
int	indef;
char	*infile;	/* input file name */
Biobuf	bin;		/* input file descriptor */
char	**gargv;	/* global argument list */
int	gargc;

int c = '\n';	/* global for use by warning() */

Symbol *sym;

int
yylex(void)
{
	while ((c=readc()) == ' ' || c == '\t')
		;
	if (c < 0)
		return 0;
	if (c == '\\') {
		c = readc();
		if (c == '\n') {
			lineno++;
			return yylex();
		}
	}
	if (c == '#') {		/* comment */
		while ((c=readc()) != '\n' && c >= 0)
			;
		if (c == '\n')
			lineno++;
		return c;
	}
	if (c == '.' || isdigit(c)) {	/* number */
		double d;
		unreadc();
		d = charstod(readd, nil);
		unreadc();
		sym = install("", LNUMBER, d);
		return LNUMBER;
	}
	if (c == '/') {	/* literal name */
		Blk *p;
		p = salloc(0);
		while((c=readc()) >= 0 && (isalnum(c) || c == '_')){
			sputc(p, c);
		}
		unreadc();
		sputc(p, 0);
		sym = (Symbol *)p;
		return LLITERAL;
	}		
	if (isalpha(c) || c == '_') {
		Symbol *s;
		char sbuf[256], *p = sbuf;
		do {
			if (p >= sbuf + sizeof(sbuf) - 1) {
				*p = '\0';
				execerror("name too long", sbuf);
			}
			*p++ = c;
		} while ((c=readc()) >= 0 && (isalnum(c) || c == '_'));
		unreadc();
		*p = '\0';
		if((p = dlookup(sbuf)) != nil) {
			pushio(str2blk(p));
			return yylex();
		}else if ((s=slookup(sbuf)) == 0)
			s = install(sbuf, LUNDEF, 0.0);
		sym = s;
		return s->type == LUNDEF ? LVAR : s->type;
	}
	if (c == '(' || c == '{') { /* quoted string or code*/
		int n = 0;
		Blk *p;
		int cstart = c, cend;
		p = salloc(0);
		cend = cstart == '(' ? ')' : '}';
		for(;;) {
			if((c = readc()) == cend) {
				if(n == 0)
					break;
				n--;
			}
			sputc(p, c);
			if(c == cstart)
				n++;
		}
		if(cstart == '(')
			sputc(p, 0);
		sym = (Symbol *)p;
		return (cstart == '(' ? LSTRING : LFUNC);
	}
	if(c == '\n')
		lineno++;
	return c;
}


void
execerror(char* s, char* t)	/* recover from run-time error */
{
	warning(s, t);
	Bseek(curfile, 0L, 2);		/* flush rest of file */
	longjmp(begin, 0);
}

void
fpecatch(void)	/* catch floating point exceptions */
{
	execerror("floating point exception", (char *) 0);
}

void
intcatch(void)	/* catch interrupts */
{
	execerror("interrupt", 0);
}

void
run(void)	/* execute until EOF */
{
	int tok;

	while(tok=yylex()) {
		switch(tok) {
		case LOPS:  (*(void(*)(void))sym->u.ptr)(); break;
		case LBLTIN: bltin(); break;
		case LFUNC: strpush(); break;
		case LNUMBER: constpush(); break;
		case LSTOP: return;
		case LSTRING: strpush(); break;
		case LLITERAL: strpush(); break;
		case LUNDEF: break;
		case LVAR: varpush(); break;
		case '\n': printcnt(); break;
		default: execerror("unknown token", "");
		}
	}
}

void
main(int argc, char* argv[])
{
	static int first = 1;

	progname = argv[0];
	init();
	readptr = &readstk[0];
	if (argc == 1) {	/* fake an argument list */
		static char *stdinonly[] = { "-" };

		gargv = stdinonly;
		gargc = 1;
	} else if (first) {	/* for interrupts */
		first = 0;
		gargv = argv+1;
		gargc = argc-1;
	}
	Binit(&bin, 0, OREAD);
	curfile = &bin;
	while (moreinput()) {
		setjmp(begin);
		initcode();	
		run();
	}
	exits(0);
}

moreinput(void)
{
	char *expr;
	static char buf[64];
	int fd;
	static Biobuf b;

	if (gargc-- <= 0)
		return 0;
	if (curfile && curfile != &bin)
		Bterm(curfile);
	infile = *gargv++;
	lineno = 1;
	if (strcmp(infile, "-") == 0) {
		curfile = &bin;
		infile = 0;
		return 1;
	}
	if(strncmp(infile, "-e", 2) == 0) {
		if(infile[2]==0){
			if(gargc == 0){
				fprint(2, "%s: no argument for -e\n", progname);
				return 0;
			}
			gargc--;
			expr = *gargv++;
		}else
			expr = infile+2;
		sprint(buf, "/tmp/hocXXXXXXX");
		infile = mktemp(buf);
		fd = create(infile, ORDWR|ORCLOSE, 0600);
		if(fd < 0){
			fprint(2, "%s: can't create temp. file: %r\n", progname);
			return 0;
		}
		fprint(fd, "%s\n", expr);
		/* leave fd around; file will be removed on exit */
		/* the following looks weird but is required for unix version */
		curfile = &b;
		seek(fd, 0, 0);
		Binit(curfile, fd, OREAD);
	} else {
		curfile=Bopen(infile, OREAD);
		if (curfile) {
			fprint(2, "%s: can't open %s\n", progname, infile);
			return moreinput();
		}
	}
	return 1;
}

void
warning(char* s, char* t)	/* print warning message */
{
	fprint(2, "%s: %s", progname, s);
	if (t)
		fprint(2, " %s", t);
	if (infile)
		fprint(2, " in %s", infile);
	fprint(2, " near line %d\n", lineno);
	while (c != '\n' && c != Beof)
		if((c = Bgetc(curfile)) == '\n')	/* flush rest of input line */
			lineno++;
}

int
readd(void* vp)
{
	USED(vp);
	return readc();
}

Blk*
str2blk(char *s)
{
	Blk *p;
	char *sp;

	p = salloc(strlen(s));
	for(sp = s; *sp; s++)
		sputc(p, *sp);
	return p;
}

void
pushio(Blk *p)
{
	if((readptr != &readstk[0]) && (*readptr != 0)) {
		if((*readptr)->rd == (*readptr)->wt)
			release(*readptr);
		else {
			if(readptr++ == &readstk[RDSKSZ]) {
				execerror("nesting depth", 0);
			}
		}
	} else
		readptr++;
	*readptr = p;
	if(p != 0)
		rewind(p);
	else {
		if((c = readc()) != '\n')
			unreadc();
	}
}

int
readc(void)
{
loop:
	if((readptr != &readstk[0]) && (*readptr != 0)) {
		if(sfeof(*readptr) == 0)
			return(lastchar = sgetc(*readptr));
		release(*readptr);
		readptr--;
		goto loop;
	}
	lastchar = Bgetc(curfile);
	if(lastchar != -1)
		return(lastchar);
	if(readptr != &readptr[0]) {
		readptr--;
		if(*readptr == 0)
			curfile = &bin;
		goto loop;
	}
	if(curfile != &bin) {
		Bterm(curfile);
		curfile = &bin;
		goto loop;
	}
	exits(0);
	return 0;	/* shut up ken */
}

void
unreadc(void)
{

	if((readptr != &readstk[0]) && (*readptr != 0)) {
		sungetc(*readptr);
	} else
		Bungetc(curfile);
	return;
}


Blk*
salloc(int size)
{
	Blk *hdr;
	char *ptr;

	all++;
	lall++;
	if(all - rel > active)
		active = all - rel;
	nbytes += size;
	lbytes += size;
	if(nbytes >maxsize)
		maxsize = nbytes;
	if(size > longest)
		longest = size;
	ptr = malloc((unsigned)size);
	if(ptr == 0){
		garbage("salloc");
		if((ptr = malloc((unsigned)size)) == 0)
			ospace("salloc");
	}
	if((hdr = hfree) == 0)
		hdr = morehd();
	hfree = (Blk *)hdr->rd;
	hdr->rd = hdr->wt = hdr->beg = ptr;
	hdr->last = ptr+size;
	return(hdr);
}

Blk*
morehd(void)
{
	Blk *h, *kk;

	headmor++;
	nbytes += HEADSZ;
	hfree = h = (Blk *)malloc(HEADSZ);
	if(hfree == 0) {
		garbage("morehd");
		if((hfree = h = (Blk*)malloc(HEADSZ)) == 0)
			ospace("headers");
	}
	kk = h;
	while(h<hfree+(HEADSZ/BLK))
		(h++)->rd = (char*)++kk;
	(h-1)->rd=0;
	return(hfree);
}


void
more(Blk *hptr)
{
	unsigned size;
	char *p;

	if((size=(hptr->last-hptr->beg)*2) == 0)
		size=2;
	nbytes += size/2;
	if(nbytes > maxsize)
		maxsize = nbytes;
	if(size > longest)
		longest = size;
	lbytes += size/2;
	lmore++;
/*	free(hptr->beg);/**/
	p = realloc(hptr->beg, size);

	if(p == 0) {
/*		hptr->beg = realloc(hptr->beg, (hptr->last-hptr->beg));
**		garbage("more");
**		if((p = realloc(hptr->beg,size)) == 0)
*/			ospace("more");
	}
	hptr->rd = p + (hptr->rd - hptr->beg);
	hptr->wt = p + (hptr->wt - hptr->beg);
	hptr->beg = p;
	hptr->last = p+size;
}

void
ospace(char *s)
{
	fprint(2,"out of space: %s\n",s);
	fprint(2,"all %ld rel %ld headmor %ld\n",all,rel,headmor);
	fprint(2,"nbytes %ld\n",nbytes);
	abort();
}

void
garbage(char *s)
{
	USED(s);
}

void
release(Blk *p)
{
	rel++;
	lrel++;
	nbytes -= p->last - p->beg;
	p->rd = (char*)hfree;
	hfree = p;
	free(p->beg);
}

