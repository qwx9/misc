#include <u.h>
#include <libc.h>
#include <bio.h>
#include "midifile.h"

static int magic;

void
samp(uvlong n)
{
	double Δt;
	long s;
	static double t0;

	if(t0 == 0.0)
		t0 = nsec();
	t0 += n * 1000 * tempo / div;
	Δt = t0 - nsec();
	s = floor(Δt / 1000000);
	if(s > 0)
		sleep(s);
}

/* set delay to 0, and translate running status: the receiver
 * only sees one track whereas running status is a property of
 * each track (stream); don't send EOT for the same reason */
static void
eat(Trk *x)
{
	int e, n;
	uchar u[16], *p, *q;
	Msg msg;

	q = u + 1;
	e = nextevent(x);
	*q++ = e;
	p = x->p;
	translate(x, e, &msg);
	if(msg.type == Ceot)
		return;
	u[0] = magic ? e >> 4 | (e & 0xf) << 4 : 0;
	n = x->p - p;
	if(msg.type == Csysex || n > nelem(u) - (q - u)){
		write(1, u, q - u);
		write(1, p, n);
	}else{
		memcpy(q, p, n);
		write(1, u, n + (q - u));
	}
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
	int end;
	uchar eot[] = {0x00, 0xff, 0x2f, 0x00};
	Trk *x;

	ARGBEGIN{
	case 'D': trace = 1; break;
	case 'm': magic = 1; break;	/* FIXME: investigate more */
	default: usage();
	}ARGEND
	if(readmid(*argv) < 0)
		sysfatal("readmid: %r");
	for(;;){
		end = 1;
		for(x=tr; x<tr+ntrk; x++){
			if(x->ended)
				continue;
			end = 0;
			x->Δ--;
			while(x->Δ <= 0){
				eat(x);
				if(x->ended)
					break;
				x->Δ = getvar(x);
			}
		}
		if(end)
			break;
		samp(1);
	}
	write(1, eot, sizeof eot);
	exits(nil);
}
