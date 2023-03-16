#include <u.h>
#include <libc.h>
#include <bio.h>

enum{
	Rate = 44100,
	Nchan = 16,//6,
	Percch = 9,//5,
};

typedef struct Trk Trk;
struct Trk{
	u8int *s;
	u8int *p;
	u8int *q;
	u8int *e;
	double Δ;
	double t;
	int ev;
	int ended;
	int chan[16];
};
Trk *tr;
int chan[16];

int trace;
int mfmt, ntrk, div = 1, tempo;
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

u8int
get8(Trk *x)
{
	u8int v;

	if(x == nil)
		Bread(ib, &v, 1);
	else{
		if(x->p >= x->e || x->ended)
			sysfatal("track overflow");
		v = *x->p++;
	}
	dprint("%02ux", v);
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
	return (n * tempo / div);
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

void
samp(double n)
{
	double Δ;
	long t;
	static double ε;

	/* FIXME: using nsec() might help with desyncs? ie. account for drift? */
	Δ = n * 1000 * tempo / div + ε;
	t = floor(Δ / 1000000);
	ε = Δ - t * 1000000;
	sleep(t);
}

int
mapinst(Trk *x, int c, int e)
{
	int i;

	if(e >> 4 == 0xf)
		return e;
	if(e >> 4 != 0x9 || x->chan[c] >= 0)
		return e;
	if(c == 9)
		i = Percch;
	else if(chan[c] >= 0)
		return e;
	else{
		for(i=0; i<Nchan; i++){
			if(i == Percch)
				continue;
			if(chan[i] < 0)
				break;
		}
		/* hope for the best; either ignore it,
		 * or hope the last channel isn't one of the main ones,
		 * which is usually the case; but we no longer have our
		 * settings */
		if(i == Nchan)
			i = Nchan-2;
	}
	x->chan[c] = i;
	chan[i] = Nchan * (x - tr) + c;
	return e & ~(16-1) | i;
}

int
ev(Trk *x, vlong)
{
	int e, n, m;

	x->q = x->p - 1;
	//*x->q = 0;
	dprint(" [%zd] ", x - tr);
	e = get8(x);
	if((e & 0x80) == 0){
		x->p--;
		e = x->ev;
		x->q--;
		//x->q[0] = 0;
		x->q[1] = e;
		if((e & 0x80) == 0)
			sysfatal("invalid event");
	}else
		x->ev = e;
	dprint("(%02ux) ", e);

	e = mapinst(x, e & 15, e);

	n = get8(x);
	switch(e >> 4){
	case 0x8: get8(x); break;
	case 0x9: get8(x); break;
	case 0xb: get8(x); break;
	case 0xc: break;
	case 0xe: get8(x); break;
	case 0xf:
		if((e & 0xf) == 0){
			while(get8(x) != 0xf7)
				;
			break;
		}
		m = get8(x);
		switch(n){
		case 0x2f: dprint(" -- so long!\n"); return -1;
		case 0x51: tempo = get16(x) << 8; tempo |= get8(x); break;
		default: skip(x, m);
		}
		break;
	case 0xa: get8(x); break;
	case 0xd: get8(x); break;
	default: sysfatal("invalid event %#ux\n", e >> 4);
	}
	dprint("\n");
	if(x->p > x->q){
		x->q[1] = e;
		x->q[0] = x->q[1] >> 4 | (x->q[1] & 0xff) << 4;
		write(1, x->q, x->p - x->q);
		x->q = x->p;
	}
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
	if(mfmt < 0 || mfmt > 1)
		sysfatal("unsupported format %d", mfmt);
	div = get16(nil);
	tr = emalloc(ntrk * sizeof *tr);
	for(x=tr, z=-1UL; x<tr+ntrk; x++){
		if(get32(nil) != 0x4d54726b)
			sysfatal("invalid track");
		n = get32(nil);
		s = emalloc(n);
		bread(s, n);
		x->s = s;
		x->p = s;
		x->q = x->p;
		x->e = s + n;
		memset(x->chan, 0x80, sizeof x->chan);
		x->Δ = getvar(x);	/* prearm */
		if(x->Δ < z)
			z = x->Δ;
	}
	for(x=tr; x<tr+ntrk; x++)
		x->Δ -= z;
	Bterm(ib);
}

void
usage(void)
{
	fprint(2, "usage: %s [-D] [mid]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int end, debug;
	Trk *x;

	debug = 0;
	ARGBEGIN{
	case 'D': debug = 1; break;
	default: usage();
	}ARGEND
	readmid(*argv);
	memset(chan, 0x80, sizeof chan);
	tempo = 500000;
	trace = debug;
	for(;;){
		end = 1;
		for(x=tr; x<tr+ntrk; x++){
			if(x->ended)
				continue;
			end = 0;
			x->Δ--;
			while(x->Δ <= 0){
				if(x->ended = ev(x, 0)){
					int c = x - tr;
					for(int i=0; i<Nchan; i++){
						if(chan[i] == c + i)
							chan[i] = -1;
					}
					break;
				}
				x->Δ = getvar(x);
			}
		}
		if(end){
			write(1, tr[0].q, tr[0].p - tr[0].q);
			break;
		}
		samp(1);
	}
	exits(nil);
}
