typedef	struct	Btree	Btree;
typedef	struct 	Block 	Block;
typedef	struct 	Entry 	Entry;

enum
{
	JXXX 	= 0,
	JMANY	= 1<<0,
	JTOGGLE	= 1<<1,
	JSINGLE	= 1<<2,
	
	JRANGE	= 1<<3,
	JEXACT	= 1<<4,
	JPREFIX	= 1<<5,
};

enum
{
	ZNONE 	= 0,
	ZKEY	= 1,
	ZVAL	= 2,
	ZDO	= 4,
};

#define	GBIT8(p)	((p)[0])
#define	GBIT16(p)	((p)[0]|((p)[1]<<8))
#define	GBIT32(p)	((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24))

#define	PBIT8(p,v)	(p)[0]=(v)
#define	PBIT16(p,v)	(p)[0]=(v);(p)[1]=(v)>>8
#define	PBIT32(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24

#define	BIT8SZ		1
#define	BIT16SZ		2
#define	BIT32SZ		4
#define	BIT64SZ		8
#define	BLOCK		(1024*8)
#define	EINIT		1
#define	EGROW		2
#define	ENTFIXLEN	15	/* size[2] zip[1] tt[4] vt[4] n[2] (key[n]) n[2] (val[n]) */
#define	HEADR		36
#define	BLOFFSET(x)	(HEADR + ((x - 1) * BLOCK))
#define	AZIP		1
#define	ATOG		2

struct	Btree
{
	Block*	blist;
	uint 	cnt;		/* # of blocks */
	uint 	H[4];		/* Height per inversion */
	uint 	head[4];		/* block id of root of inversion*/
	int	fd;
	int	inversion;		/* of current query */
	ulong	ett;		/* effective transaction time */
	int	(*fn)(Entry*);
	char	*name;
	Btree*	next;
};

struct	Block 
{
	short	size;		/* size of all ents + 2 */
	uint	seq;		/* block id */
	uint	m;		/* number of entries */
	uint	esize;		/* max elements allocated */
	Entry**	ents;
	Block*	next;
};
#define	B	((Block*)0)

struct	Entry 
{
	short	size;		/* ENTFIXLEN + len key + len val */
	uchar	zip;		/* compression flag */
	ulong	tt;		/* transaction time */
	ulong	vt;		/* valid time */
	char*	key;		/* element */
	char*	val;		/* attribute */
};
#define	E	((Entry*)0)

int	acomp(char*, char*);
int	addentry(Block*, Entry*, int, uchar);
void	bflush(Btree*);
void	bfn(Btree*, int(*)(Entry*));
int	btree(Btree*, char*, int);
int	coin(Btree*, ulong, char*, char*, uint);
int	comp(Btree*, Entry*, char*);
uint	convE2M(Entry*, uchar*, uint);
uint	convM2E(uchar*, uint, Entry*, char*);
void	dumpblock(Block*);
int	epack(Block*, uchar*, int);
int	eunpack(uchar*, long, Block*);
int	found(Entry*);
Block*	getblock(Btree*, uint);
int	insert(Btree*, ulong, ulong, char*, char*, uint);
Entry*	insertR(Btree*, Block*, Entry*, uint);
int	less(Btree*, Entry*, Entry*);
Block*	lookup(Btree*, uint);
Entry*	new(ulong tt, ulong vt, char *key, char *val);
int	join(Btree*, ulong, char*, char*, int, int, Block*);
void	putblock(int, Block*);
int	search(Btree*, char*, char*, uint);
int	searchR(Btree*, Block*, char*, uint, char*);
Entry*	split(Btree*, Block*);
int	toggle(Btree *, Entry*);
void	zipe(Block*, int);
