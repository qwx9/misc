#include <u.h>
#include <libc.h>
#include <thread.h>
#include <bio.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>

enum{
	Cback,
	Cbord,
	Cvol,
	Ctxt,
	Cmute,
	Ncols,

	Bordsz = 3,
};
Image *cols[Ncols];

typedef struct Vol Vol;
struct Vol{
	char name[128];
	char value[128];
	int text;
	int stereo;
	int left;
	int right;
	int muted;
	Rectangle r;
	Rectangle lr;
	Rectangle rr;
};
Rectangle frame;
Vol *voltab;
int nvol, tabsz;
char *dev = "/dev/volume";
Mousectl *mctl;
Keyboardctl *kctl;

int
writevol(Vol *v)
{
	Biobuf *bf;

	if((bf = Bopen(dev, OWRITE)) == nil)
		return -1;
	if(v->text)
		Bprint(bf, "%s %s\n", v->name, v->value);
	else if(v->stereo)
		Bprint(bf, "%s %d %d\n", v->name, v->left, v->right);
	else
		Bprint(bf, "%s %d\n", v->name, v->left);
	Bterm(bf);
	return 0;
}

int
quickquiet(Vol *v)
{
	Biobuf *bf;

	if((bf = Bopen(dev, OWRITE)) == nil)
		return -1;
	Bprint(bf, "%s 0\n", v->name);
	v->left = v->right = 0;
	Bterm(bf);
	return 0;
}

int
prompt(Vol *v)
{
	int r;
	char buf[sizeof v->value] = {0};

	strncpy(buf, v->value, sizeof buf);
	if((r = enter(v->name, buf, sizeof(buf)-UTFmax, mctl, kctl, nil)) < 0){
		fprint(2, "prompt: %r\n");
		return -1;
	}else if(r > 0)
		strncpy(v->value, buf, sizeof buf);
	return 0;
}

int
readvol(char *path)
{
	int n;
	Biobuf *bf;
	char *s, *fld[4];
	Vol *v;

	if((bf = Bopen(path, OREAD)) == nil)
		return -1;
	nvol = 0;
	for(;;){
		if((s = Brdstr(bf, '\n', 1)) == nil)
			break;
		if((n = getfields(s, fld, nelem(fld), 1, " ")) < 1 || n > 3)
			goto next;
		if(nvol++ >= tabsz){
			tabsz += 16;
			if((voltab = realloc(voltab, tabsz * sizeof *voltab)) == nil)
				sysfatal("realloc: %r");
		}
		v = voltab + nvol - 1;
		memset(v, 0, sizeof *v);
		strncpy(v->name, fld[0], sizeof(v->name)-1);
		if(strcmp(fld[0], "delay") == 0
		|| strcmp(fld[0], "speed") == 0
		|| strcmp(fld[0], "dev") == 0){
			strncpy(v->value, fld[1], sizeof(v->value)-1);
			v->text = 1;
			goto next;
		}
		v->text = 0;
		v->stereo = n == 3;
		v->left = strtol(fld[1], nil, 10);
		if(n > 2){
			v->right = strtol(fld[2], nil, 10);
			if(v->right == 0.0)
				v->stereo = 0;
		}
next:
		free(s);
	}
	Bterm(bf);
	return 0;
}

Vol *
getcur(Point p, int *right)
{
	Vol *v;

	for(v=voltab; v<voltab+nvol; v++){
		if(ptinrect(p, v->r)){
			if(ptinrect(p, v->rr))
				*right = 1;
			else
				*right = 0;
			break;
		}
	}
	return (v == voltab + nvol ? nil : v);
}

void
redraw(void)
{
	int h;
	Vol *v;
	Point p;
	Rectangle r, rr;

	draw(screen, screen->r, cols[Cback], nil, ZP);
	h = (Dy(frame) - nvol) / nvol;
	rr = frame;
	rr.max.y = frame.min.y + h + 1;
	for(v=voltab; v<voltab+nvol; v++){
		border(screen, rr, 1, cols[Cbord], ZP);
		r = v->lr;
		if(v->text){
			p = subpt(r.max, stringsize(font, v->value));
			string(screen, p, cols[Ctxt], ZP, font, v->value);
		}else{
			r.max.x = r.min.x + Dx(r) * v->left / 100;
			draw(screen, r, cols[v->muted ? Cmute : Cvol], nil, ZP);
			if(v->stereo){
				r = v->rr;
				r.max.x = r.min.x + Dx(r) * v->right / 100;
				draw(screen, r, cols[v->muted ? Cmute : Cvol], nil, ZP);
			}
		}
		r = v->lr;
		string(screen, addpt(r.min, Pt(2,2)), cols[Ctxt], ZP, font, v->name);
		rr = rectaddpt(rr, Pt(0, h+1));
	}
	flushimage(display, 1);
}

void
snarfgeom(void)
{
	int hh, h;
	Rectangle r, rr;
	Vol *v;

	frame = insetrect(screen->r, Bordsz+1);
	hh = (Dy(frame) - nvol) / nvol;
	rr = frame;
	rr.max.y = frame.min.y + hh + 1;
	for(v=voltab; v<voltab+nvol; v++){
		r = insetrect(rr, 1);
		v->r = r;
		h = Dy(r);
		v->lr = r;
		if(v->stereo){
			r.max.y -= Dy(r) / 2 + 1;
			v->lr.max.y = r.max.y;
			r = rectaddpt(r, Pt(0, h/2+1));
			v->rr = r;
		}
		else
			v->rr = ZR;
		rr = rectaddpt(rr, Pt(0, hh + 1));
	}
}

void
load(void)
{
	if(readvol(dev) < 0)
		sysfatal("readvol: %r");
	snarfgeom();
}

void
reset(void)
{
	load();
	redraw();
}

void
usage(void)
{
	fprint(2, "usage: %s [-f dev]\n", argv0);
	threadexits("usage");
}

void
threadmain(int argc, char **argv)
{
	int i, right;
	double Δ;
	Rune r;
	Mouse mold;
	Vol *cur;

	ARGBEGIN{
	case 'f': dev = EARGF(usage()); break;
	}ARGEND
	if(initdraw(0, 0, "vol") < 0)
		sysfatal("initdraw: %r");
	if((mctl = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kctl = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	fmtinstall('P', Pfmt);
	fmtinstall('R', Rfmt);
	Theme th[nelem(cols)] = {
		[Cback] { "back",	DBlack },
		[Cbord] { "border",	DWhite },
		[Cvol] { "paletext",	0x777777FF },
		[Cmute] { "holdtext", DMedblue },
		[Ctxt] { "text",	0x777777FF },
	};
	readtheme(th, nelem(th), nil);
	for(i=0; i<nelem(cols); i++)
 		cols[i] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, th[i].c); 
	if(readvol(dev) < 0)
		sysfatal("readvol: %r");
	reset();
	enum{AMOUSE, ARESIZE, AKEY, AEND};
	Alt a[AEND+1] = {
		[AMOUSE] {mctl->c, &mctl->Mouse, CHANRCV},
		[ARESIZE] {mctl->resizec, nil, CHANRCV},
		[AKEY] {kctl->c, &r, CHANRCV},
		[AEND] {nil, nil, CHANEND}
	};
	for(cur=nil;;){
		switch(alt(a)){
		case AMOUSE:
			/* FIXME: maybe fsm should be flipped to work based on position
			 * instead of button */
			/* FIXME: device: choose from list of valid devices */
			if(mctl->buttons != 0 && mctl->buttons != mold.buttons)
				cur = nil;
			if((mctl->buttons & 7) == 1){
				if(cur == nil && (cur = getcur(mctl->xy, &right)) == nil)
					break;
				if(cur->text)
					break;
				Δ = 100.0 * (mctl->xy.x - cur->r.min.x) / Dx(cur->r);
				cur->left = cur->right = Δ;
				if(writevol(cur) < 0){
					fprint(2, "writevol: %r\n");
					load();
				}
				redraw();
			}else if((mctl->buttons & 7) == 2){
				if(cur == nil && (cur = getcur(mctl->xy, &right)) == nil)
					break;
				if(cur->text)
					break;
				else{
					Δ = 100.0 * (mctl->xy.x - cur->r.min.x) / Dx(cur->r);
					if(right)
						cur->right = Δ;
					else
						cur->left = Δ;
					if(writevol(cur) < 0){
						fprint(2, "writevol: %r\n");
						load();
					}
				}
				redraw();
			}else if((mctl->buttons & 7) == 4){
				if(cur == nil && (cur = getcur(mctl->xy, &right)) == nil)
					break;
				if(cur->text)
					prompt(cur);
				/* FIXME */
				//quickquiet(cur);
				redraw();
				cur = nil;
			}else if(cur != nil){
				if(writevol(cur) < 0)
					sysfatal("writevol: %r");
				reset();
				cur = nil;
			}else
				cur = nil;
			mold = mctl->Mouse;
			break;
		case ARESIZE:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			reset();
			break;
		case AKEY:
			switch(r){
			case 'q':
			case Kdel: threadexitsall(nil);
			}
			break;
		}
	}
}
