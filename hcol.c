#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>

char id[16];
Point ps;
Image *col;
Keyboardctl *kc;
Mousectl *mc;

void
plaster(void)
{
	draw(screen, screen->r, col, nil, ZP);
	string(screen, ps, display->black, ZP, font, id);
	flushimage(display, 1);
}

Image *
eallocim(ulong col)
{
	Image *i;

	i = allocimage(display, Rect(0,0,1,1), RGB24, 1, col);
	if(i == nil)
		sysfatal("allocimage: %r");
	return i;
}

void
setid(u32int n)
{
	sprint(id, "%#ux", n);
}

void
reset(void)
{
	ps = addpt(screen->r.min, stringsize(font, "A"));
	plaster();
}

void
threadmain(int, char **)
{
	u32int n;
	char line[16];
	Rune r;

	if(initdraw(nil, nil, "hcol") < 0)
		sysfatal("initdraw: %r");
	kc = initkeyboard(nil);
	if(kc == nil)
		sysfatal("initkeyboard: %r");
	mc = initmouse(nil, screen);
	if(mc == nil)
		sysfatal("initmouse: %r");
	col = eallocim(DBlack);
	setid(0);
	reset();

	Alt a[] = {
		{mc->c, nil, CHANRCV},
		{mc->resizec, nil, CHANRCV},
		{kc->c, &r, CHANRCV},
		{nil, nil, CHANEND}
	};
	for(;;)
		switch(alt(a)){
		case 1:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			reset();
			break;
		case 2:
			if(r == Kdel)
				threadexitsall(nil);
			memset(line, 0, sizeof line);
			runetochar(line, &r);
			if(enter(nil, line, 12, mc, kc, nil) < 0)
				break;
			n = strtoul(line, nil, 0);
			setid(n);
			freeimage(col);
			col = eallocim(n<<8);
			if(col == nil)
				sysfatal("allocimage: %r");
			plaster();
			break;
		}
}
