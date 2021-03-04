#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>

typedef struct Tg Tg;
struct Tg{
	Point;
	Point v;
	int r;
	Tg *p;
	Tg *n;
};
Tg troot;
Tg *tg;

vlong tics;
int ntg;
int hits, misses, misclicks;
uint bmask = 1;
int oldb;
enum{
	NSEC = 1000000000
};
/* recommend these are set well outside the confort range, but not so much so
 * that every game lasts less than a minute */
vlong div, div0 = NSEC/20;	/* tic duration (ns) */
int tddd = 220;	/* tic duration decrease delay (tics) */
int Δ0 = 100;	/* tic duration delta factor */
int mtdt = 5;	/* tics/tddd % mtdt: delay to tgmax++ */
int floatdt = 25;	/* floating text delay (tics) */
int newdt = 8;		/* target spawn delay (tics) */
int dmax, rmax = 24;	/* max target radius (px) */
int tgmax = 3;	/* initial max number of targets onscreen */
int missmax = 10;	/* max targets let go */
int nogrow;	/* don't grow and shrink targets */
int done;

enum{
	CBACK,
	CTARG,
	CMISS,
	CTXT,
	CEND
};
Image *col[CEND];
Image *fb;
Rectangle winxy;	/* spawning area */
Mousectl *mctl;
Keyboardctl *kctl;
Channel *ticc;

#define trr()	(rmax+1 - t->r % (rmax+1))
#define tr()	(nogrow ? rmax : t->r <= rmax ? t->r : trr())

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
			if(nbsend(ticc, nil) < 0)
				break;
		}
		δ = div;
		dt = (t + 2*δ - nsec()) / 1000000;
		t += δ;
	}
}

Tg *
push(void)
{
	Tg *t;

	t = mallocz(sizeof *t, 1);
	if(t == nil)
		sysfatal("talloc: %r");
	t->p = tg->p;
	t->n = tg;
	tg->p->n = t;
	tg->p = t;
	return t;
}

void
pop(Tg *t)
{
	t->p->n = t->n;
	t->n->p = t->p;
	free(t);
}

void
nuke(void)
{
	while(tg->n != tg)
		pop(tg->n);
	ntg = 0;
}

void
drw(Tg *t)
{
	int r;

	r = tr();
	fillellipse(fb, *t, r, r, col[CTARG], ZP);
}

void
clr(Tg *t)
{
	draw(fb, Rpt(*t, t->v), col[CBACK], nil, ZP);
}

void
txt(char *s, Tg *t, Image *i)
{
	Point o;

	if(t == nil)
		o = subpt(divpt(winxy.max, 2), divpt(stringsize(font, s), 2));
	else{
		o = divpt(stringsize(font, s), 2);
		t->v = addpt(*t, o);
		t->x -= o.x;
		t->y -= o.y;
		o = *t;
	}
	string(fb, o, i, ZP, font, s);
}

void
misclk(Point o)
{
	Tg *t;

	/* FIXME: often doesn't do anything */
	fillellipse(fb, o, 1, 1, col[CTARG], ZP);
	t = push();
	t->v = addpt(o, Pt(2,2));
	t->x = o.x - 1;
	t->y = o.y - 1;
	t->r = dmax+1;
	misclicks++;
}

void
hit(Tg *t)
{
	int r;

	r = tr();
	fillellipse(fb, *t, r, r, col[CBACK], ZP);
	pop(t);
	ntg--;
	hits++;
}

void
miss(Tg *t)
{
	int r;

	r = nogrow ? rmax : 1;
	fillellipse(fb, *t, r, r, col[CBACK], ZP);
	txt("miss", t, col[CMISS]);
	misses++;
	ntg--;
}

void
grow(Tg *t)
{
	if(nogrow)
		return;
	drw(t);
}

void
shrink(Tg *t)
{
	int r;

	if(nogrow)
		return;
	r = trr();
	fillellipse(fb, *t, r, r, col[CBACK], ZP);
	r--;
	fillellipse(fb, *t, r, r, col[CTARG], ZP);
}

void
spawn(void)
{
	Tg *t;

	t = push();
	ntg++;
	t->x = winxy.min.x + nrand(winxy.max.x - winxy.min.x);
	t->y = winxy.min.y + nrand(winxy.max.y - winxy.min.y);
	if(nogrow){
		t->r = rmax;
		drw(t);
	}
}

void
check(int b, Point m)
{
	Point o;
	Tg *t;

	/* FIXME: implement counters for each button */
	USED(b);
	m = subpt(m, screen->r.min);
	for(t=tg->n; t != tg; t=t->n){
		if(t->r > dmax)
			continue;
		o = subpt(*t, m);
		if(hypot(o.x, o.y) < tr()){
			hit(t);
			return;
		}
	}
	misclk(m);
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
	char s[64];

	sprint(s, "hit %d misses %d misclicks %d", hits, misses, misclicks);
	txt(s, nil, col[CTXT]);
	plaster();
}

void
adv(void)
{
	Tg *t;

	tics++;
	if(tics % tddd == 0){
		div -= div0 / Δ0;
		if(tics / tddd % mtdt == 0)
			tgmax++;
	}

	for(t=tg->n; t != tg; t=t->n){
		t->r++;
		if(t->r <= rmax)
			grow(t);
		else if(t->r > rmax && t->r <= dmax)
			shrink(t);
		else if(t->r == dmax+1){
			miss(t);
			if(misses >= missmax){
				done++;
				score();
			}
		}else if(t->r > dmax+floatdt){
			clr(t);
			pop(t);
		}
	}
	if(tics % newdt == 0 && ntg < tgmax)
		spawn();
}

void
dreset(void)
{
	Tg *t;

	winxy = rectsubpt(insetrect(screen->r, rmax), screen->r.min);
	if(Dx(winxy) < 3*dmax || Dy(winxy) < 3*dmax)
		sysfatal("window too small");
	freeimage(fb);
	fb = eallocim(Rect(0,0,Dx(screen->r),Dy(screen->r)), 0, DNofill);
	draw(fb, fb->r, col[CBACK], nil, ZP);
	for(t=tg->n; t != tg; t=t->n){
		if(t->r > dmax)
			continue;
		drw(t);
	}
	if(done)
		score();
	plaster();
}

void
init(void)
{
	div = div0;
	tics = 0;
	hits = misses = misclicks = 0;
	done = 0;
}

void
usage(void)
{
	print("%s [-g] [-d newdt] [-h div0] [-i Δ0] [-m nmiss] [-o rmax] [-r tddd] [-t 1-5]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	Rectangle d;
	Rune r;
	Mouse m;

	ARGBEGIN{
	case 'd':
		newdt = strtol(EARGF(usage()), nil, 0);
		break;
	case 'g':
		nogrow++;
		break;
	case 'h':
		div0 = strtoll(EARGF(usage()), nil, 0);
		if(div0 <= 0)
			sysfatal("invalid divisor %lld", div0);
		div0 = NSEC / div0;
		break;
	case 'i':
		Δ0 = strtol(EARGF(usage()), nil, 0);
		if(Δ0 <= 0)
			sysfatal("invalid divisor divisor %d", Δ0);
		break;
	case 'm':
		missmax = strtol(EARGF(usage()), nil, 0);
		break;
	case 'o':
		rmax = strtol(EARGF(usage()), nil, 0);
		break;
	case 'r':
		tddd = strtol(EARGF(usage()), nil, 0);
		break;
	case 't':
		bmask = 31 >> 5 - strtol(EARGF(usage()), nil, 0);
		break;
	default:
		usage();
	}ARGEND
	dmax = rmax*2;

	tg = &troot;
	tg->n = tg->p = tg;
	if(initdraw(nil, nil, "rfx") < 0)
		sysfatal("initdraw: %r");
	d = Rect(0,0,1,1);
	col[CBACK] = display->black;
	col[CTARG] = eallocim(d, 1, 0x990000ff);
	col[CMISS] = eallocim(d, 1, 0x440000ff);
	col[CTXT] = eallocim(d, 1, 0xbb4400ff);
	dreset();

	mctl = initmouse(nil, screen);
	if(mctl == nil)
		sysfatal("initmouse: %r");
	kctl = initkeyboard(nil);
	if(kctl == nil)
		sysfatal("initkeyboard: %r");
	ticc = chancreate(sizeof(int), 0);
	if(ticc == nil)
		sysfatal("chancreate: %r");
	srand(time(nil));
	init();
	if(proccreate(tic, nil, 8192) < 0)
		sysfatal("proccreate tproc: %r");

	enum{ATIC, AMOUSE, ARESIZE, AKEY, AEND};
	Alt a[AEND+1] = {
		[ATIC] {ticc, nil, CHANRCV},
		[AMOUSE] {mctl->c, &m, CHANRCV},
		[ARESIZE] {mctl->resizec, nil, CHANRCV},
		[AKEY] {kctl->c, &r, CHANRCV},
		[AEND] {nil, nil, CHANEND}
	};
	for(;;){
		switch(alt(a)){
		case ATIC:
			if(done)
				break;
			adv();
			plaster();
			break;
		case AMOUSE:
			if(!done && m.buttons & bmask && m.buttons & ~oldb)
				check(m.buttons, m.xy);
			oldb = m.buttons;
			break;
		case ARESIZE:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			dreset();
			break;
		case AKEY:
			switch(r){
			case 'n':
				nuke();
				init();
				draw(fb, fb->r, col[CBACK], nil, ZP);
				plaster();
				break;
			case 'q':
			case Kdel:
				nuke();
				threadexitsall(nil);
			}
			break;
		}
	}
}
