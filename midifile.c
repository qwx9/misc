#include <u.h>
#include <libc.h>
#include <bio.h>
#include "midifile.h"

Trk *tr;
int mfmt, ntrk, div = 1, tempo = 500000;
int trace, stream;
vlong tic;
Biobuf *ib;

void *
emalloc(ulong n)
{
	void *p;

	p = mallocz(n, 1);
	if(p == nil)
		sysfatal("mallocz: %r");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void
dprint(char *fmt, ...)
{
	char s[256];
	va_list arg;

	if(!trace)
		return;
	va_start(arg, fmt);
	vseprint(s, s+sizeof s, fmt, arg);
	va_end(arg);
	fprint(2, "%s", s);
}

Biobuf *
bfdopen(int fd, int mode)
{
	Biobuf *bf;

	bf = Bfdopen(fd, mode);
	if(bf == nil)
		sysfatal("bfdopen: %r");
	Blethal(bf, nil);
	return bf;
}

Biobuf *
bopen(char *file, int mode)
{
	int fd;

	fd = open(file, mode);
	if(fd < 0)
		sysfatal("bopen: %r");
	return bfdopen(fd, mode);
}

void
bread(void *u, int n)
{
	if(Bread(ib, u, n) != n)
		sysfatal("bread: short read");
}

u8int
get8(Trk *x)
{
	u8int v;

	if(x == nil || x->p == nil || x->e == nil)
		Bread(ib, &v, 1);
	else{
		if(x->p >= x->e || x->ended)
			sysfatal("track overflow");
		v = *x->p++;
	}
	return v;
}

u16int
get16(Trk *x)
{
	u16int v;

	v = get8(x) << 8;
	return v | get8(x);
}

u32int
get32(Trk *x)
{
	u32int v;

	v = get16(x) << 16;
	return v | get16(x);
}

u8int
peekbyte(Trk *x)
{
	return *x->p;
}

u32int
peekvar(Trk *x)
{
	uchar *p;
	uint v;

	p = x->p;
	v = getvar(x);
	x->p = p;
	return v;
}

void
skip(Trk *x, int n)
{
	while(n-- > 0)
		get8(x);
}

int
getvar(Trk *x)
{
	int v, w;

	w = get8(x);
	v = w & 0x7f;
	while(w & 0x80){
		if(v & 0xff000000)
			sysfatal("invalid variable-length number");
		v <<= 7;
		w = get8(x);
		v |= w & 0x7f;
	}
	return v;
}

double
tc(double n)
{
	return (n * tempo * Rate / div) / 1e6;
}

int
nextevent(Trk *x)
{
	int e;

	x->prev = x->p;
	e = get8(x);
	if((e & 0x80) == 0){
		if(x->p != nil){
			x->p--;
			x->prev--;
		}
		e = x->ev;
		if((e & 0x80) == 0)
			sysfatal("invalid event %#ux", e);
	}else
		x->ev = e;
	return e;
}

/* FIXME: use midump's implementation to make this not suck */
static void
setmsg(Msg *m, int chan, int type, int arg1, int arg2)
{
	m->chan = chan;
	m->type = type;
	m->arg1 = arg1;
	m->arg2 = arg2;
}
void
translate(Trk *x, int e, Msg *msg)
{
	int c, n, m, type;
	uchar *p;

	c = e & 0xf;
	dprint("ch%02d ", c);
	n = get8(x);
	m = -1;
	type = Cunknown;
	switch(e >> 4){
	case 0x8:
		m = get8(x);
		dprint("note off\t%02ux\taftertouch\t%02ux", n, m);
		type = Cnoteoff;
		break;
	case 0x9:
		m = get8(x);
		dprint("note on\t%02ux\tvelocity\t%02ux", n, m);
		type = Cnoteon;
		break;
	case 0xb:
		m = get8(x);
		dprint("control change: ");
		switch(n){
		case 0x00:
			dprint("bank select msb\t%02ux", m);
			type = Cbankmsb;
			break;
		case 0x07:
			dprint("channel volume\t%02ux", m);
			type = Cchanvol;
			break;
		case 0x0a:
			dprint("pan\t%02ux", m);
			type = Cpan;
			break;
		default:
			dprint("unknown controller %.4ux", n);
			break;
		}
		break;
	case 0xc:
		dprint("program change\t%02ux", n);
		type = Cprogram;
		break;
	case 0xe:
		n = (get8(x) << 7 | n) - 0x4000 / 2;
		dprint("pitch bend\t%02x", n);
		type = Cpitchbend;
		break;
	case 0xf:
		dprint("sysex:\t");
		if((e & 0xf) == 0){
			m = 0;
			while(get8(x) != 0xf7)
				m++;
			fprint(2, "sysex n %d m %d\n", n, m);
			type = Csysex;
			break;
		}
		m = get8(x);
		switch(n){
		case 0x2f:
			dprint("... so long!");
			x->ended = 1;
			type = Ceot;
			break;
		case 0x51:
			tempo = get16(x) << 8;
			tempo |= get8(x);
			dprint("tempo change\t%d", tempo);
			type = Ctempo;
			break;
		default:
			dprint("skipping unhandled event %02ux", n);
			skip(x, m);
			break;
		}
		break;
	case 0xa:
		m = get8(x);
		dprint("polyphonic key pressure/aftertouch\t%02ux\t%02ux", n, m);
		type = Ckeyafter;
		break;
	case 0xd:
		m = get8(x);
		dprint("channel pressure/aftertouch\t%02ux\t%02ux", n, m);
		type = Cchanafter;
		break;
	default: sysfatal("invalid event %#ux", e >> 4);
	}
	setmsg(msg, c, type, n, m);
	dprint("\t[");
	for(p=x->prev; p<x->p; p++)
		dprint("%02ux", *p);
	dprint("]\n");
}

int
readmid(char *file)
{
	u32int n, z;
	uchar *s;
	Trk *x;

	ib = file != nil ? bopen(file, OREAD) : bfdopen(0, OREAD);
	if(get32(nil) != 0x4d546864 || get32(nil) != 6){
		werrstr("invalid header");
		return -1;
	}
	mfmt = get16(nil);
	ntrk = get16(nil);
	if(ntrk == 1)
		mfmt = 0;
	if(mfmt < 0 || mfmt > 1){
		werrstr("unsupported format %d", mfmt);
		return -1;
	}
	div = get16(nil);
	tr = emalloc(ntrk * sizeof *tr);
	for(x=tr, z=-1UL; x<tr+ntrk; x++){
		if(get32(nil) != 0x4d54726b){
			werrstr("invalid track");
			return -1;
		}
		n = get32(nil);
		x->s = emalloc(n+1);
		bread(x->s, n);
		x->p = x->s;
		x->prev = x->s;
		x->e = x->s + n;
		x->Δ = getvar(x);	/* prearm */
		if(x->Δ < z)
			z = x->Δ;
	}
	for(x=tr; x<tr+ntrk; x++)
		x->Δ -= z;
	Bterm(ib);
	ib = nil;
	return 0;
}
