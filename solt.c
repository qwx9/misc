#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>

/* FIXME: shitty model, shitty implementation */

enum{
	NCARDS	= 52,
	NFLAVOR	= 4,
	NSTACKS	= 8,
	NHAND	= NCARDS - ((NSTACKS+1)*(NSTACKS+2))/2 + 1
};

int c[NCARDS];

typedef struct Card Card;
struct Card{
	int	v;
	char	l[5];
	Card*	n;
};
Card *d[NCARDS];
Card *s[NSTACKS];
Card *h[NHAND];

Image *ncol;	/* string color */
Image *selcol;	/* selection color */
Point spt;	/* string pos */

Mouse m;
int resize;
int kfd, mfd;


void *
emalloc(ulong n)
{
	void *b;

	if((b = malloc(n)) == nil)
		sysfatal("%s: %r", argv0);
	return b;
}

void
shuffle(void)
{
	int i, j, n;
	int t[NCARDS];

	srand(time(nil));
	for(i = 0; i < NCARDS; i++)
		t[i] = rand();
	for(i = 0; i < NCARDS; i++){
		n = 0;
		for(j = 0; j < NCARDS; j++)
			if(t[n] > t[j])
				n = j;
		c[i] = n;
		t[n] = 0x10000;
	}
}

char
mkflavor(int n)
{
	switch(n % NFLAVOR){
	case 0: return 'd';
	case 1: return 'p';
	case 2: return 'u';
	case 3: return 'n';
	}
	return ' ';
}

void
mkdeck(void)
{
	int i, v;

	for(i = 0; i < NCARDS; i++){
		d[i] = emalloc(sizeof **d);
		v = c[i];
		d[i]->v = v;
		sprint(d[i]->l, "%d%c", 1 + v/NFLAVOR, mkflavor(v));
		d[i]->n = nil;
	}
}

void
arrange(void)
{
	int i, j, n = 0;
	Card *p;

	for(i = 0; i < NSTACKS; i++)
		s[i] = d[n++];
	for(i = 0; i < NSTACKS; i++){
		p = s[i];
		for(j = NSTACKS-i-1; j < NSTACKS; j++)
			p = p->n = d[n++];
	}
	for(i = 0; i < NHAND; i++)
		h[i] = d[n++];
}

void
drw(void)
{
	int i, n;
	Card *p;
	char b[10];
	Point sxy = spt;

	/* TODO: clear line */

	for(i = 0; i < NSTACKS; i++){
		p = s[i];
		n = 0;
		if(p == nil)
			sprint(b, "  ()    ");
		else{
			while(p->n != nil){
				n++;
				p = p->n;
			}
			snprint(b, 10, "(%d)%s ", n, p->l);
		}
		string(screen, sxy, ncol, ZP, font, b);
		sxy.x += 8 * font->width;
	}
	flushimage(display, 1);
}

void
select(Point *p)
{
	/* TODO: selecting a card */
	USED(p);
}

void
kth(void *)
{
	int k;
	char buf[256];
	char *s;
	Rune r;

	if((kfd = open("/dev/kbd", OREAD)) < 0)
		sysfatal("%s: open /dev/kbd: %r", argv0);
	for(;;){
		if(read(kfd, buf, sizeof(buf)-1) <= 0)
			sysfatal("%s: read /dev/kbd: %r", argv0);
		if(buf[0] == 'c' && utfrune(buf, Kdel))
			threadexitsall(nil);
		if(buf[0] != 'k' && buf[0] != 'K')
			continue;

		s = buf + 1;
		k = 0;
		while(*s != 0){
			s += chartorune(&r, s);
			switch(r){
			case Kdel:
				threadexitsall(nil);
			}
		}
	}
	/* TODO: make other threads exit cleanly? send to threadmain? */
}

void
mth(void *)
{
	int n, nerr;
	Mouse m;
	char buf[1+5*12];

	if((mfd = open("/dev/mouse", OREAD)) < 0)
		sysfatal("open: %r");
	memset(&m, 0, sizeof m);
	nerr = 0;

	for(;;){
		n = read(mfd, buf, sizeof buf);
		if(n != 1+4*12){
			fprint(2, "mproc: bad count %d not 49: %r\n", n);
			if(n < 0 || ++nerr > 10)
				break;
			continue;
		}
		nerr = 0;

		switch(buf[0]){
		case 'r':
			resize = 1;
			/* TODO: something */
			break;
		case 'm':
			m.xy.x = atoi(buf+1+0*12);
			m.xy.y = atoi(buf+1+1*12);
			m.buttons = atoi(buf+1+2*12);
			//m.msec = atoi(buf+1+3*12);
			break;
		}
	}
}

void
croak(void)
{
	Card **p = d;

	close(kfd);
	close(mfd);
	while(p != d + NCARDS)
		free(*p++);
	freeimage(ncol);
	freeimage(selcol);
}

void
reset(void)
{
	draw(screen, screen->r, display->black, nil, ZP);
	shuffle();
	mkdeck();
	arrange();
	drw();
}

void
init(void)
{
	/* TODO: why the difference between proc and thread? */

	if(proccreate(kth, nil, mainstacksize) < 0)
		sysfatal("%s threadcreate kproc: %r", argv0);
	if(proccreate(mth, nil, mainstacksize) < 0)
		sysfatal("%s threadcreate mproc: %r", argv0);

	if(initdraw(nil, nil, "solt") < 0)
		sysfatal("%s initdraw: %r", argv0);
	ncol = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x551100ff);
	if(ncol == nil)
		sysfatal("%s allocimage: %r", argv0);
	selcol = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x882200ff);
	if(selcol == nil)
		sysfatal("%s allocimage: %r", argv0);
	print("selcol %p %p %d\n", selcol->display->bufp, selcol->display->buf, selcol->display->bufsize);
	/* problem: font->width == 0 at this point? */
	spt = Pt(screen->r.min.x + Dx(screen->r)/2 - 8*NSTACKS*4,
		screen->r.min.y + Dy(screen->r)/2);

	reset();
	atexit(croak);
}

void
usage(void)
{
	print("usage: %s\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	ARGBEGIN{
	default:
		usage();
	}ARGEND

	/* TODO: playing the game */
	/* TODO: hand, 4 finish spots, 4 off spots */
	/* TODO: mouse menu (exit, save card, stack card, restart, new, ) */
	/* TODO: moving shit around */
	/* TODO: resize handling */
	/* TODO: exit by communicating by channel */
	/* TODO: calculate distance between card slots programatically */

	init();
	/* FIXME: do shit (setup channels) */
	for(;;){
		/*if(recv(kchan, nil) < 0)
			threadexitsall("%s recv: %r");*/
		sleep(100);
	}
}
