/* lr - retarded recursive file list */
#include <u.h>
#include <libc.h>
#include <String.h>

extern	void	du(char*, Dir*);
extern	void	err(char*);
extern	int	seen(Dir*);
extern	int	warn(char*);

char	*fmt = "%s\n";
char	*readbuf;
int	dflag;
int	nflag;

void
usage(void)
{
	fprint(2, "usage: %s [-dnQ] [file ...]\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int doqt;
	char *s;

	doqt = 1;
	ARGBEGIN{
	case 'd': dflag++; break;
	case 'n': nflag++; dflag++; break;
	case 'Q': doqt = 0; break;
	default: usage();
	}ARGEND
	if(doqt){
		doquote = needsrcquote;
		quotefmtinstall();
		fmt = "%q\n";
	}
	if(argc == 0)
		du(".", dirstat("."));
	else
		while(*argv != nil){
			s = *argv++;
			du(s, dirstat(s));
		}
	exits(nil);
}

void
dufile(char *name, Dir *d)
{
	String *s;

	s = s_copy(name);
	s_append(s, "/");
	s_append(s, d->name);
	print(fmt, s_to_c(s));
	s_free(s);
}

void
du(char *name, Dir *dir)
{
	int fd, i, n;
	Dir *buf, *d;
	String *s;

	if(dir == nil){
		warn(name);
		return;
	}
	if((dir->qid.type & QTDIR) == 0){
		if(!nflag)
			print(fmt, name);
		return;
	}

	fd = open(name, OREAD);
	if(fd < 0){
		warn(name);
		return;
	}
	n = dirreadall(fd, &buf);
	if(n < 0)
		warn(name);
	close(fd);
	for(i=n, d=buf; i>0; i--, d++){
		if((d->qid.type & QTDIR) == 0){
			if(!nflag)
				dufile(name, d);
			continue;
		}
		if(strcmp(d->name, ".") == 0 || strcmp(d->name, "..") == 0
		|| seen(d))
			continue;	/* don't get stuck */

		s = s_copy(name);
		s_append(s, "/");
		s_append(s, d->name);
		du(s_to_c(s), d);
		s_free(s);
	}
	free(buf);
	if(dflag)
		print(fmt, name);
}

#define	NCACHE	256	/* must be power of two */

typedef struct
{
	Dir*	cache;
	int	n;
	int	max;
} Cache;
Cache cache[NCACHE];

int
seen(Dir *dir)
{
	Dir *dp;
	int i;
	Cache *c;

	c = &cache[dir->qid.path&(NCACHE-1)];
	dp = c->cache;
	for(i=0; i<c->n; i++, dp++)
		if(dir->qid.path == dp->qid.path &&
		   dir->type == dp->type &&
		   dir->dev == dp->dev)
			return 1;
	if(c->n == c->max){
		if (c->max == 0)
			c->max = 8;
		else
			c->max += c->max/2;
		c->cache = realloc(c->cache, c->max*sizeof(Dir));
		if(c->cache == nil)
			err("malloc failure");
	}
	c->cache[c->n++] = *dir;
	return 0;
}

void
err(char *s)
{
	fprint(2, "du: %s: %r\n", s);
	exits(s);
}

int
warn(char *s)
{
	fprint(2, "du: %s: %r\n", s);
	return 0;
}
