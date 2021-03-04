#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>

enum{
	KMAX = 24 * UTFmax,
	DICTLEN = 32<<10,
	TMAX = 32,
};
char dict[DICTLEN][KMAX];
int ndic;
char *df = "/lib/words";
typedef struct Txt Txt;
struct Txt{
	Point;
	char s[KMAX];
	char *m;
	int d;
	Image *c;
	Txt *p;
	Txt *n;
};
Txt t0, *tl, *cur;

vlong tics;
int nt, words, miss, mistyp;
enum{
	NSEC = 1000000000
};
vlong div, div0 = NSEC/2;	/* tic duration (ns) */
int dage = 16;
int tddd = 64;	/* tic duration decrease delay (tics) */
int newdt = 3;	/* target spawn delay (tics) */
int missmax = 10;	/* max targets let go */
long tm0;
int done;

enum{
	CBACK,
	CTXT,
	CSTALE,
	COLD,
	CCUR,
	CEND
};
Image *fb, *col[CEND];
Point fs, wc;
int dy;
Mousectl *mc;
Keyboardctl *kc;
Channel *tc;

Image *
eallocim(Rectangle r, int repl, ulong col)
{
	Image *i;

	i = allocimage(display, r, screen->chan, repl, col);
	if(i == nil)
		sysfatal("allocimage: %r");
	return i;
}

void
tic(void *)
{
	vlong t, dt, δ;

	t = nsec();
	dt = div/1000000;
	for(;;){
		if(dt >= 0){
			sleep(dt);
			if(nbsend(tc, nil) < 0)
				break;
		}
		δ = div;
		dt = (t + 2*δ - nsec()) / 1000000;
		t += δ;
	}
}

void
pop(Txt *t)
{
	t->p->n = t->n;
	t->n->p = t->p;
	nt--;
	free(t);
	if(cur == t)
		cur = nil;
}

void
nuke(void)
{
	while(tl->n != tl)
		pop(tl->n);
}

void
drw(Txt *t, Image *c)
{
	if(c == nil)
		c = t->c;
	string(fb, *t, c, ZP, font, t->s);
}

void
drwcur(void)
{
	char c;

	c = *cur->m;
	*cur->m = 0;
	drw(cur, col[CCUR]);
	*cur->m = c;
}

void
spawn(void)
{
	Txt *t;

	t = mallocz(sizeof *t, 1);
	if(t == nil)
		sysfatal("talloc: %r");
	t->p = tl->p;
	t->n = tl;
	tl->p->n = t;
	tl->p = t;
	nt++;
	sprint(t->s, dict[nrand(ndic)]);
	t->c = col[CTXT];
	t->y = nrand(dy);
	drw(t, nil);
}

void
plaster(void)
{
	draw(screen, screen->r, fb, nil, ZP);
	flushimage(display, 1);
}

void
score(void)
{
	Point o;
	long t;
	char s[72];

	/* FIXME */
	/* this wpm figure is bullshit */
	t = (time(nil) - tm0) / 60;
	sprint(s, "words %d misses %d mistypes %d in %lds",
		words, miss, mistyp, t);
	o = subpt(wc, divpt(stringsize(font, s), 2));
	string(fb, o, col[CCUR], ZP, font, s);
	plaster();
}

void
adv(void)
{
	Txt *t;

	tics++;
	if(tics % tddd == 0)
		div -= div0 / 16;

	for(t=tl->n; t != tl; t=t->n){
		t->d++;
		drw(t, col[CBACK]);
		t->x += fs.x;
		if(t->d % dage == 0){
			switch(t->d / dage){
			case 1: t->c = col[CSTALE]; break;
			case 2: t->c = col[COLD]; break;
			case 3:
				miss++;
				if(miss >= missmax){
					done++;
					score();
				}
				pop(t);
				continue;
			}
		}
		drw(t, nil);
	}
	if(cur != nil)
		drwcur();
	if(tics % newdt == 0 && nt < TMAX)
		spawn();
	if(tl->n == tl){
		spawn();
		tics += newdt - tics % newdt;
	}
}

int
check(Rune k)
{
	Txt *t;

	if(cur == nil){
		for(t=tl->n; t != tl; t=t->n)
			if(utfrune(t->s, k) == t->s){
				t->m = t->s + runelen(k);
				cur = t;
				drwcur();
				return 1;
			}
	}else if(utfrune(cur->m, k) == cur->m){
		cur->m += runelen(k);
		if(cur->m >= cur->s + strlen(cur->s)){
			words++;
			drw(cur, col[CBACK]);
			pop(cur);
			cur = nil;
		}else
			drwcur();
		return 1;
	}
	mistyp++;
	return 0;
}

void
dreset(void)
{
	Txt *t;

	dy = Dy(screen->r) - fs.y;
	if(dy < 4 * fs.y || Dx(screen->r) < KMAX * fs.x / 8)
		sysfatal("window too small");
	wc = Pt(Dx(screen->r)/2, Dy(screen->r)/2);
	freeimage(fb);
	fb = eallocim(Rect(0,0,Dx(screen->r),Dy(screen->r)), 0, DNofill);
	draw(fb, fb->r, col[CBACK], nil, ZP);
	for(t=tl->n; t != tl; t=t->n)
		drw(t, nil);
	if(done)
		score();
	plaster();
}

void
init(void)
{
	div = div0;
	tics = 0;
	words = miss = mistyp = 0;
	tm0 = time(nil);
	done = 0;
	spawn();
	plaster();
}

void
menu(void)
{
	enum{NEW, QUIT};
	static char *item[] = {
		[NEW] "new",
		[QUIT] "quit",
		nil
	};
	static Menu m = {
		item, nil, 0
	};

	switch(menuhit(3, mc, &m, nil)){
	case NEW:
		nuke();
		init();
		draw(fb, fb->r, col[CBACK], nil, ZP);
		plaster();
		break;
	case QUIT:
		nuke();
		threadexitsall(nil);
	}
}

void
load(char *f)
{
	int n;
	char *s, (*p)[KMAX];
	Biobuf *bf;

	bf = Bopen(f, OREAD);
	if(bf == nil)
		sysfatal("load: %r");
	p = dict;
	while(s = Brdline(bf, '\n'), s != nil && (char*)p < dict[nelem(dict)]){
		n = Blinelen(bf);
		if(n > sizeof *p)
			n = sizeof *p;
		memcpy(*p, s, n-1);
		ndic++;
		p++;
	}
	Bterm(bf);
}

void
usage(void)
{
	print("%s [-d n] [-h hz] [-m n] [-o tics] [-r tics] [dict]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	double d;
	Rectangle r;
	Rune k;
	Mouse m;

	ARGBEGIN{
	case 'd':
		newdt = strtol(EARGF(usage()), nil, 0);
		break;
	case 'h':
		d = strtod(EARGF(usage()), nil);
		if(d <= 0.0)
			sysfatal("invalid divisor %f", d);
		div0 = NSEC / d;
		break;
	case 'm':
		missmax = strtol(EARGF(usage()), nil, 0);
		break;
	case 'o':
		dage = strtol(EARGF(usage()), nil, 0);
		break;
	case 'r':
		tddd = strtol(EARGF(usage()), nil, 0);
		break;
	default:
		usage();
	}ARGEND
	if(argc > 0)
		df = *argv;
	load(df);

	tl = &t0;
	tl->n = tl->p = tl;

	if(initdraw(nil, nil, "spd") < 0)
		sysfatal("initdraw: %r");
	fs = stringsize(font, "A");
	r = Rect(0,0,1,1);
	col[CBACK] = display->black;
	col[CTXT] = eallocim(r, 1, 0xbb4400ff);
	col[CSTALE] = eallocim(r, 1, 0x990000ff);
	col[COLD] = eallocim(r, 1, 0x440000ff);
	col[CCUR] = eallocim(r, 1, 0xdd9300ff);
	dreset();

	mc = initmouse(nil, screen);
	if(mc == nil)
		sysfatal("initmouse: %r");
	kc = initkeyboard(nil);
	if(kc == nil)
		sysfatal("initkeyboard: %r");
	tc = chancreate(sizeof(int), 0);
	if(tc == nil)
		sysfatal("chancreate: %r");
	srand(time(nil));
	init();
	if(proccreate(tic, nil, 8192) < 0)
		sysfatal("proccreate tproc: %r");

	enum{ATIC, AMOUSE, ARESIZE, AKEY, AEND};
	Alt a[AEND+1] = {
		[ATIC] {tc, nil, CHANRCV},
		[AMOUSE] {mc->c, &m, CHANRCV},
		[ARESIZE] {mc->resizec, nil, CHANRCV},
		[AKEY] {kc->c, &k, CHANRCV},
		[AEND] {nil, nil, CHANEND}
	};
	for(;;)
		switch(alt(a)){
		case ATIC:
			if(done)
				break;
			adv();
			plaster();
			break;
		case AMOUSE:
			if(m.buttons & 4)
				menu();
			break;
		case ARESIZE:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			dreset();
			break;
		case AKEY:
			if(check(k))
				plaster();
			break;
		}
}
