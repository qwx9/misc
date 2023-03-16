#include <u.h>
#include <libc.h>
#include <bio.h>

enum{
	Rate = 44100,
	Ninst = 128 + 81-35+1,
};

typedef struct Trk Trk;
struct Trk{
	u8int *s;
	u8int *p;
	u8int *e;
	double Δ;
	double t;
	int ev;
	int ended;
};
Trk *tr;

int desc, quiet;
int mfmt, ntrk, div = 1, tempo;
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

	if(!desc)
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
	if(!quiet)
		print("%02hhux", v);
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

double
tc(double n)
{
	return (n * tempo * Rate / div) / 1e6;
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

int
ev(Trk *x)
{
	int e, n, m;

	dprint("%08lld ", tic);
	print("[%02zd] ", x-tr);
	quiet = 1;
	e = get8(x);
	quiet = 0;
	if((e & 0x80) == 0){
		x->p--;
		e = x->ev;
		print("%02ux*", e);
		if((e & 0x80) == 0)
			sysfatal("invalid event");
	}else{
		print("%02ux", e);
		x->ev = e;
	}
	dprint("C%02d: %1ux ", e & 15, e >> 4);
	n = get8(x);
	switch(e >> 4){
	case 0x8:
		m = get8(x);
		dprint("C%02d note off %02ux, aftertouch %02ux", e&0xf, n, m);
		break;
	case 0x9:
		m = get8(x);
		dprint("C%02d note on %02ux, velocity %02ux", e&0xf, n, m);
		break;
	case 0xb:
		m = get8(x);
		dprint("control change: ");
		switch(n){
		case 0x00: dprint("C%02d bank select msb %02ux", e&0xf, m); break;
		case 0x07: dprint("C%02d channel volume %02ux", e&0xf, m); break;
		case 0x0a: dprint("C%02d pan %02ux", e&0xf, m); break;
		default: dprint("C%02d unknown controller %.4ux = %02ux", e&0xf, n, m);
		}
		break;
	case 0xc: dprint("program change %02ux", e&0xf, n); break;
	case 0xe:
		n = get8(x) << 7 | n;
		dprint("pitch bend change %02x", n - 0x4000 / 2);
		break;
	case 0xf:
		dprint("sysex: ");
		if((e & 0xf) == 0){
			while(get8(x) != 0xf7)
				;
			break;
		}
		m = get8(x);
		dprint("[%d] ", m);
		switch(n){
		case 0x2f: print(" -- so long!\n"); return -1;
		case 0x51:
			tempo = get16(x) << 8;
			tempo |= get8(x);
			dprint("tempo change %d", tempo);
			break;
		default: skip(x, m);
		}
		break;
	case 0xa:
		m = get8(x);
		dprint("polyphonic key pressure/aftertouch %02ux", m);
		break;
	case 0xd:
		m = get8(x);
		dprint("channel pressure/aftertouch %02ux", m);
		break;
	default: sysfatal("invalid event %#ux", e >> 4);
	}
	print("\n");
	return 0;
}

void
readmid(char *file)
{
	u32int n, z;
	uchar *s;
	Trk *x;

	ib = file != nil ? bopen(file, OREAD) : bfdopen(0, OREAD);
	if(get32(nil) != 0x4d546864 || get32(nil) != 6)
		sysfatal("invalid header");
	mfmt = get16(nil);
	ntrk = get16(nil);
	if(ntrk == 1)
		mfmt = 0;

		switch(mfmt){
		case 0: dprint("format 0: single multi-channel track\n"); break;
		case 1: dprint("format 1: multiple concurrent tracks\n"); break;
		case 2: dprint("format 2: multiple independent tracks\n"); break;
		}
		dprint("%d tracks\n", ntrk);

	if(mfmt < 0 || mfmt > 1)
		sysfatal("unsupported format %d", mfmt);
	div = get16(nil);
	if(1)	/* FIXME: properly introduce new members, etc. */
		dprint("div: %d ticks per quarter note\n", div);
	else
		dprint("div: %d smpte format, %d ticks per frame\n", (s16int)div>>8, div & 0xff);
	tr = emalloc(ntrk * sizeof *tr);
	print("\n");
	for(x=tr, z=-1UL; x<tr+ntrk; x++){
		if(get32(nil) != 0x4d54726b)
			sysfatal("invalid track");
		n = get32(nil);
		dprint("track %zd, %d bytes\n", x-tr, n);
		s = emalloc(n);
		bread(s, n);
		x->s = s;
		x->p = s;
		x->e = s + n;
		x->Δ = getvar(x);	/* prearm */
		if(x->Δ < z)
			z = x->Δ;
		print("\n");
	}
	for(x=tr; x<tr+ntrk; x++)
		x->Δ -= z;
	tic = z;
	Bterm(ib);
}

void
usage(void)
{
	fprint(2, "usage: %s [mid]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int end, stream;
	Trk *x, ☺;

	desc = 1;
	stream = 0;
	ARGBEGIN{
	case 'b': desc = 0; break;
	case 's': stream = 1; break;
	default: usage();
	}ARGEND
	tempo = 500000;
	if(stream){
		ib = *argv != nil ? bopen(*argv, OREAD) : bfdopen(0, OREAD);
		tr = &☺;
		ntrk = 1;
		memset(tr, 0, sizeof *tr);
		for(;;){
			getvar(&☺);
			if(ev(&☺) < 0)
				exits(nil);
		}
	}
	readmid(*argv);
	for(end=0; !end;){
		end = 1;
		for(x=tr; x<tr+ntrk; x++){
			if(x->ended)
				continue;
			end = 0;
			x->Δ--;
			x->t += tc(1);
			while(x->Δ <= 0){
				if(x->ended = ev(x)){
					x->p = x->e;
					break;
				}
				x->Δ = getvar(x);
				print(" ");
			}
		}
		tic++;
	}
	exits(nil);
}
