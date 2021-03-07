#include <u.h>
#include <libc.h>
#include <draw.h>
#include <keyboard.h>
#include <mouse.h>

int grx = 200 / 4, gry = 200 / 4;
Image *gc, *uc, *nc;
int mson, fix;
Point p0, mp;
Rectangle grabr, rt;

void
reset(void)
{
	Point p;

	p0 = divpt(addpt(screen->r.min, screen->r.max), 2);
	p = Pt(grx, gry);
	grabr = Rpt(subpt(p0, p), addpt(p0, p));
	rt = Rpt(screen->r.min, addpt(screen->r.min, Pt(8*12, font->height)));
	draw(screen, screen->r, display->black, nil, ZP);
	flushimage(display, 1);
}

void
mproc(void)
{
	int fd, n, nerr;
	char buf[1+5*12], s[16];
	Mouse m;
	Point p;
	Rectangle r;

	fd = open("/dev/mouse", ORDWR);
	if(fd < 0)
		sysfatal("open /dev/mouse: %r");
	memset(&m, 0, sizeof m);
	nerr = 0;
	r = Rect(0,0,1,1);
	mp = p0;
	for(;;){
		n = read(fd, buf, sizeof buf);
		if(n != 1+4*12){
			if(n < 0 || ++nerr > 10)
				break;
			continue;
		}
		nerr = 0;
		switch(*buf){
		case 'r':
			if(getwindow(display, Refnone) < 0)
				sysfatal("resize failed: %r");
			reset();
		case 'm':
			p.x = strtol(buf+1+0*12, nil, 10);
			p.y = strtol(buf+1+1*12, nil, 10);
			if(!mson){
				draw(screen, rectaddpt(r, p), nc, nil, ZP);
				flushimage(display, 1);
				break;
			}
			if(fix){
				fix = 0;
				goto res;
			}
			draw(screen, rt, display->black, nil, ZP);
			sprint(s, "%d,%d", p.x - mp.x, p.y - mp.y);
			string(screen, screen->r.min, uc, ZP, font, s);
			draw(screen, rectaddpt(r, mp), uc, nil, ZP);
			draw(screen, rectaddpt(r, p), gc, nil, ZP);
			flushimage(display, 1);
			if(!ptinrect(p, grabr)){
		res:
				fprint(fd, "m%d %d", p0.x, p0.y);
				p = p0;
			}
			mp = p;
			break;
		}
	}
}

void
grab(void)
{
	static char nocurs[2*4+2*2*16];
	static int fd = -1;

	mson ^= 1;
	if(mson){
		fd = open("/dev/cursor", ORDWR|OCEXEC);
		if(fd < 0){
			fprint(2, "grab: %r\n");
			return;
		}
		write(fd, nocurs, sizeof nocurs);
		fix++;
	}else if(fd >= 0){
		close(fd);
		fd = -1;
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [-x dx] [-y dy]\n", argv0);
	sysfatal("usage");
}

void
main(int argc, char **argv)
{
	int n, fd, mpid;
	char buf[256];
	Rune r;

	ARGBEGIN{
	case 'x': grx = strtol(EARGF(usage()), nil, 0); break;
	case 'y': gry = strtol(EARGF(usage()), nil, 0); break;
	default: usage();
	}ARGEND
	if(initdraw(nil, nil, "mev") < 0)
		sysfatal("initdraw: %r");
	gc = allocimage(display, Rect(0,0,1,1), RGB24, 1, DGreen);
	uc = allocimage(display, Rect(0,0,1,1), RGB24, 1, DRed);
	nc = allocimage(display, Rect(0,0,1,1), RGB24, 1, DBlue);
	reset();
	mpid = rfork(RFPROC|RFMEM);
	switch(mpid){
	case -1:
		sysfatal("fork: %r");
	case 0:
		mproc();
		exits(nil);
	}
	fd = open("/dev/kbd", OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	memset(buf, 0, sizeof buf);
	for(;;){
		if(buf[0] != 0){
			n = strlen(buf)+1;
			memmove(buf, buf+n, sizeof(buf)-n);
		}
		if(buf[0] == 0){
			n = read(fd, buf, sizeof(buf)-1);
			if(n <= 0)
				break;
			buf[n-1] = 0;
			buf[n] = 0;
		}
		if(buf[0] == 'c'){
			chartorune(&r, buf+1);
			if(utfrune(buf, Kdel))
				break;
			else if(utfrune(buf, ' '))
				grab();
			else if(utfrune(buf, 'b')){
				draw(screen, screen->r, display->black, nil, ZP);
				flushimage(display, 1);
			}
		}
	}
	postnote(PNPROC, mpid, "shutdown");
	exits(nil);
}
