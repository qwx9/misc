#include <u.h>
#include <libc.h>
#include <bio.h>

enum{
	Rate = 44100,
	Nchan = 16,
	Maxch = 16,	// FIXME: could have more
	Percch = 9,
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
};
Trk *tr;
int m2ich[Maxch], i2mch[Maxch], age[Maxch];
int nch = Nchan, percch = Percch;

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

// FIXME: common midi.c shit (midilib.c? midifile?)
void
samp(double n)
{
	double Δt;
	long s;
	static double t0, t1;

	if(t0 == 0.0)
		t0 = nsec();
	t1 = t0 + n * 1000 * tempo / div;
	t0 = t1;
	Δt = t1 - nsec();
	s = floor(Δt / 1000000);
	if(s > 0)
		sleep(s);
}

int
mapinst(Trk *, int c, int e)
{
	int i, m, a;

	i = m2ich[c];
	if(c == 9)
		i = percch;
	else if(e >> 4 != 0x9){
		if(e >> 4 == 0x8 && i >= 0){
			i2mch[i] = -1;
			m2ich[c] = -1;
		}
		return e;
	}else if(i < 0){
		for(i=0; i<nch; i++){
			if(i == percch)
				continue;
			if(i2mch[i] < 0)
				break;
		}
		if(i == nch){
			for(m=i=a=0; i<nch; i++){
				if(i == percch)
					continue;
				if(age[i] > age[m]){
					m = i;
					a = age[i];
				}
			}
			if(a < 100){
				fprint(2, "could not remap %d\n", c);
				return e;
			}
			i = m;
			fprint(2, "remapped %d → %d\n", c, i);
		}
	}
	age[i] = 0;
	m2ich[c] = i;
	i2mch[i] = c;
	return e & ~(Nchan-1) | i;
}

int
ev(Trk *x, vlong)
{
	int e, n, m;

	x->q = x->p - 1;
	dprint(" [%zd] ", x - tr);
	e = get8(x);
	if((e & 0x80) == 0){
		x->p--;
		e = x->ev;
		x->q--;
		x->q[1] = e;
		if((e & 0x80) == 0)
			sysfatal("invalid event");
	}else
		x->ev = e;
	dprint("(%02ux) ", e);

	e = mapinst(x, e & 15, e);

	n = get8(x);
	if((e & 15) == percch){
		if(n < 36)
			n += 36 - n;
	}
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
	fprint(2, "usage: %s [-D] [-c nch] [-p percch] [mid]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, c, end, debug;
	Trk *x;

	debug = 0;
	ARGBEGIN{
	case 'D': debug = 1; break;
	case 'c':
		nch = atoi(EARGF(usage()));
		break;
	case 'p':
		percch = atoi(EARGF(usage()));
		break;
	default: usage();
	}ARGEND
	if(nch <= 0 || nch > Maxch)
		usage();
	if(percch <= 0 || percch > nch)
		usage();
	readmid(*argv);
	tempo = 500000;
	trace = debug;
	for(i=0; i<nelem(m2ich); i++){
		m2ich[i] = i2mch[i] = -1;
		age[i] = -1UL;
	}
	for(;;){
		end = 1;
		for(x=tr; x<tr+ntrk; x++){
			if(x->ended)
				continue;
			end = 0;
			x->Δ--;
			while(x->Δ <= 0){
				if(x->ended = ev(x, 0)){
					c = x - tr;
					i = m2ich[c];
					if(i >= 0){
						i2mch[i] = -1;
						m2ich[c] = -1;
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
		for(i=0; i<nch; i++){
			if(i2mch[i] < 0)
				continue;
			age[i]++;
			if(age[i] > 10000){
				fprint(2, "reset %d\n", i2mch[i]);
				m2ich[i2mch[i]] = -1;
				i2mch[i] = -1;
			}
		}
	}
	exits(nil);
}
