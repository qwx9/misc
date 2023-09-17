#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

// volctl.c: simple volume control for /dev/volume, using our shatup /dev/theme

char *progname = "volctl";

char *vf;
int vfd;

Font *font;

// if set, write extra stuff to stderr
int debug;

enum {
	Mono,
	Stereo,
};

typedef struct Volume	Volume;
struct Volume {
	char *name;
	int type;
	int llevel;
	int rlevel;
	Rectangle r;
};

Volume *voltab;
int Nvolume;

#pragma	   varargck    type  "V"   Volume*
static int
volfmt(Fmt *fmt)
{
	Volume *v;
	v = va_arg(fmt->args, Volume*);

	if(v->type == Stereo)
		return fmtprint(fmt, "%s %d %d", v->name, v->llevel, v->rlevel);

	return fmtprint(fmt, "%s %d", v->name, v->llevel);
}

void
getvol(void)
{
	char buf[256];
	char *line[32], *elem[3];
	int i, l, n;
	Volume *v;

	// read volume file
	seek(vfd, 0, 0);
	if((n = read(vfd, buf, sizeof buf)) <= 0) {
		fprint(2, "cannot read volume file: %r\n");
		exits("read");
	}

	if(n >= sizeof buf)
		n = sizeof(buf)-1;
	buf[n] = 0;

	// break volume lines into fields
	n = getfields(buf, line, nelem(line), 1, "\n");

	voltab = calloc(n, sizeof(struct Volume));

	// fill each Volume struct
	for(i=0; i<n; i++) {
		v = voltab+Nvolume;
		l = tokenize(line[i], elem, nelem(elem));

		v->name = strdup(elem[0]);

		switch(l) {
		case 2:
			v->type = Mono;
			v->llevel = v->rlevel = atoi(elem[1]);
			break;
		case 3:
			v->type = Stereo;
			v->llevel = atoi(elem[1]);
			v->rlevel = atoi(elem[2]);
			break;
		default:
			fprint(2, "bad volume line '%s'\n", line[i]);
			exits(0);
		}

		// we dont care about these
		if(strcmp("speed", elem[0]) == 0 || strcmp("delay", elem[0]) == 0) {
			if(debug) fprint(2, "ignored: %V\n", v);
			continue;
		} else {
			Nvolume++;
		}

		if(debug) fprint(2, "parsed:  %V\n", v);
	}
}

void
setvol(Volume *v)
{
	seek(vfd, 0, 0);

	if(debug)
		fprint(2, "write '%V'\n", v);
	if(fprint(vfd, "%V", v) < 0)
		fprint(2, "write '%V' to %s: %r\n", v, vf);
}

int nvrect;
int toprect;
Image *slidercolor;
Image *background;
Image *bordertext;

void
drawvol(Volume *v, int new)
{
	int midlx, midrx, midy;

	midlx = v->r.min.x+(Dx(v->r)*v->llevel)/100;
	midrx = v->r.min.x+(Dx(v->r)*v->rlevel)/100;
	midy = v->r.min.y+Dy(v->r)/2;

	if(new) {
		border(screen, v->r, -1, bordertext, ZP);
		draw(screen, v->r, background, nil, ZP);
		line(screen, Pt(v->r.min.x, midy), Pt(v->r.max.x-1, midy), 0, 0, 0, background, ZP);
	}

	if(v->type == Stereo) {
		draw(screen, Rect(v->r.min.x, v->r.min.y, midlx, midy), slidercolor, nil, ZP);
		draw(screen, Rect(v->r.min.x, midy+1, midrx, v->r.max.y), slidercolor, nil, ZP);
	} else {
		draw(screen, Rect(v->r.min.x, v->r.min.y, midlx, v->r.max.y), slidercolor, nil, ZP);
	}

	if(!new) {
		if(v->type == Stereo) {
			draw(screen, Rect(midlx, v->r.min.y, v->r.max.x, midy), background, nil, ZP);
			draw(screen, Rect(midrx, midy+1, v->r.max.x, v->r.max.y), background, nil, ZP);
		} else {
			draw(screen, Rect(midlx, v->r.min.y, v->r.max.x, v->r.max.y), background, nil, ZP);
		}
	}

	string(screen, Pt(v->r.max.x-stringwidth(font, v->name)-5, v->r.min.y+2),
		bordertext, ZP, font, v->name);
}
	
void
redraw(Image *screen)
{
	enum { PAD=3, MARGIN=5 };
	Point p;
	int i, ht, wid;
	Volume *v;

	p = stringsize(font, "0");
	ht = p.y + 2*2;
	nvrect = (Dy(screen->r)-2*MARGIN)/(ht+PAD)+1;
	wid = Dx(screen->r)-2*MARGIN;
	if(nvrect >= Nvolume) {
		nvrect = Nvolume;
		toprect = 0;
	}

	draw(screen, screen->r, background, nil, ZP);
	p = addpt(screen->r.min, Pt(MARGIN, MARGIN));
	for(i=0; i<nvrect; i++) {
		v = &voltab[(i+toprect)%Nvolume];
		v->r = Rpt(p, addpt(p, Pt(wid, ht)));
		p.y += ht+PAD;
		drawvol(v, 1);
	}
	for(; i<Nvolume; i++){
		v = &voltab[(i+toprect)%Nvolume];
		v->r = Rect(0,0,0,0);
	}
}

void
click(Mouse m)
{
	Volume *v, *ev;
	int mid, δ, lev;
	int what;

	if(m.buttons & 1) {
		if(nvrect < Nvolume) {
			if(toprect-- == 0)
				toprect = Nvolume-1;
			redraw(screen);
		}
		do { 
			m = emouse();
		}	while(m.buttons);
		return;	
	}

	if(m.buttons & 6) {
		what = m.buttons;
		for(v=voltab, ev=v+Nvolume; v<ev; v++) {
			if(ptinrect(m.xy, v->r))
				break;
		}
		if(v >= ev)
			return;
		mid = v->r.min.y+Dy(v->r)/2;
		δ = m.xy.y - mid;

		do {
			lev = ((m.xy.x - v->r.min.x)*100)/Dx(v->r);
			if(lev < 0)
				lev = 0;
			if(lev > 100)
				lev = 100;
		//	if(-5 < δ && δ < 5)
			if(what & 2 || v->type == Mono)
				v->rlevel = v->llevel = lev;
			else if(δ > 0)
				v->rlevel = lev;
			else
				v->llevel = lev;
			setvol(v);
			drawvol(v, 0);
			m = emouse();
		} while(m.buttons);
		return;
	}
	exits(0);
}

void
main(int argc, char **argv)
{
	Event e;

	vf = "/dev/volume";
	ARGBEGIN{
	case 'f':
		vf = ARGF();
		break;
	case 'd':
		debug++;
		break;
	default:
		goto Usage;
	}ARGEND;

	if(argc) {
	Usage:
		fprint(2, "usage: %s [-f volumefile]\n", progname);
		exits("usage");
	}

	if((vfd = open(vf, ORDWR)) < 0) {
		fprint(2, "cannot open '%s': %r\n", vf);
		exits("open");
	}

	fmtinstall('V', volfmt);
	getvol();
	initdraw(0, 0, progname);

	Theme th[] = {
		{ "back",	DPaleyellow },
		{ "border",	DYellowgreen },
		{ "text",	DBlack },
	};
	readtheme(th, nelem(th), nil);
	background = allocimage(display, Rect(0,0,1,1), screen->chan, 1, th[0].c);
	slidercolor = allocimage(display, Rect(0,0,1,1), screen->chan, 1, th[1].c);
	bordertext = allocimage(display, Rect(0,0,1,1), screen->chan, 1, th[2].c);

	font = display->defaultfont;
	redraw(screen);
	einit(Emouse|Ekeyboard);

	for(;;) {
		switch(eread(Emouse|Ekeyboard, &e)) {
		case Ekeyboard:
			if(e.kbdc == 0x7F || e.kbdc == 'q')
				exits(0);
			break;
		case Emouse:
			if(e.mouse.buttons)
				click(e.mouse);
			break;
		}
	}
}

void
eresized(int new)
{
	if(new && getwindow(display, Refmesg) < 0)
		fprint(2,"can't reattach to window");
	redraw(screen);
}

