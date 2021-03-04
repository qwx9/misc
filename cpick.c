#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <keyboard.h>
#include <mouse.h>

/* there should really be a color picker in paint(1) instead */

Keyboardctl *kc;
Mousectl *mc;
Image *pic, *boxc;
Point pan;
Rectangle box;

void
putcol(Point m)
{
	char s[32];
	uchar buf[4];
	u32int v;
	Point p;

	p = addpt(screen->r.min, pan);
	unloadimage(pic, rectsubpt(Rpt(m, Pt(m.x+1, m.y+1)), p), buf, sizeof buf);
	loadimage(boxc, boxc->r, buf, sizeof buf);
	draw(screen, rectaddpt(box, p), boxc, nil, ZP);
	v = buf[0] << 16 | buf[1] << 8 | buf[2];
	snprint(s, sizeof s, "%#08ux", v);
	string(screen, addpt(box.min, p), display->black, ZP, font, s);
	flushimage(display, 1);
}

void
redraw(void)
{
	draw(screen, rectaddpt(pic->r, addpt(screen->r.min, pan)), pic, nil, ZP);
	flushimage(display, 1);
}

Image *
iconv(Image *i)
{
	Image *ni;

	if(i->chan == XBGR32)
		return i;
	if((ni = allocimage(display, i->r, XBGR32, 0, DNofill)) == nil)
		sysfatal("allocimage: %r");
	draw(ni, ni->r, i, nil, ZP);
	freeimage(i);
	return ni;
}

void
threadmain(int, char **)
{
	int fd;
	Mouse m, om;
	Rune r;

	if(initdraw(nil, nil, "cpick") < 0)
		sysfatal("initdraw: %r");
	if((fd = open("/dev/screen", OREAD)) < 0)
		sysfatal("open: %r");
	if((pic = readimage(display, fd, 0)) == nil)
		sysfatal("readimage: %r");
	close(fd);
	pic = iconv(pic);
	box = screen->r;
	if((boxc = allocimage(display, Rect(0,0,1,1), pic->chan, 1, DNofill)) == nil)
		sysfatal("allocimage: %r");
	redraw();
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	if((mc = initmouse(nil, _screen->display->image)) == nil)
		sysfatal("initmouse: %r");
	Alt a[] = {
		{mc->resizec, nil, CHANRCV},
		{mc->c, &m, CHANRCV},
		{kc->c, &r, CHANRCV},
		{nil, nil, CHANEND}
	};
	for(;;){
		switch(alt(a)){
		case 0:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			redraw();
			break;
		case 1:
			if((m.buttons & 4) == 4){
				pan.x += m.xy.x - om.xy.x;
				pan.y += m.xy.y - om.xy.y;
				redraw();
			}else
				putcol(m.xy);
			om = m;
			break;
		case 2:
			switch(r){
			case Kdel: case 'q': threadexitsall(nil);
			}
			break;
		default:
			threadexitsall("alt: %r");
		}
	}
}
