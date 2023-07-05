typedef struct Msg Msg;
typedef struct Trk Trk;

enum{
	Rate = 44100,
	Ninst = 128 + 81-35+1,
	Nchan = 16,
	Percch = 9,
};

struct Msg{
	int type;
	int chan;
	int arg1;
	int arg2;
};
// FIXME: naming
// FIXME: hicucps playing when there are sysex etc events (-s)
struct Trk{
	u8int *s;
	u8int *prev;
	u8int *p;
	u8int *e;
	double Δ;
	double t;
	int ev;
	int ended;
};
extern Trk *tr;

enum{
	Cnoteoff,
	Cnoteon,
	Cbankmsb,
	Cchanvol,
	Cpan,
	Cprogram,
	Cpitchbend,
	Ceot,
	Ctempo,
	Ckeyafter,
	Cchanafter,
	Csysex,
	Cunknown,
};

extern int mfmt, ntrk, div, tempo;
extern int trace, stream;

void*	emalloc(ulong);
int	readmid(char*);
void	dprint(char*, ...);
void	samp(uvlong);
u32int	peekvar(Trk*);
u8int	peekbyte(Trk*);
int	nextevent(Trk*);
void	translate(Trk*, int, Msg*);
int	getvar(Trk*);

#pragma	varargck	argpos	dprint	1
