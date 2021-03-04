
#line	2	"/usr/qwx/p/misc/calc.y"
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

int yylex(void);
int yyparse(void);
void yyerror(char*);

Biobuf *bf;

#line	14	"/usr/qwx/p/misc/calc.y"
typedef union {
	int i;
} YYSTYPE;
extern	int	yyerrflag;
#ifndef	YYMAXDEPTH
#define	YYMAXDEPTH	150
#endif
YYSTYPE	yylval;
YYSTYPE	yyval;
#define	NUM	57346
#define YYEOFCODE 1
#define YYERRCODE 2

#line	36	"/usr/qwx/p/misc/calc.y"


void
yyerror(char *s)
{
	fprint(2, "%s\n", s);
}

int
getnum(int c)
{
	char s[64], *p;

	for(p=s, *p++=c; isdigit(c=Bgetc(bf));)
		if(p < s+sizeof(s)-1)
			*p++ = c;
	*p = 0;
	Bungetc(bf);
	return strtol(s, nil, 10);
}

int
yylex(void)
{
	int n, c;

	do
		c = Bgetc(bf);
	while(c != '\n' && isspace(c));
	if(isdigit(c)){
		yylval.i = getnum(c);
		return NUM;
	}else if(c == 'd'){
		n = getnum(Bgetc(bf));
		yylval.i = nrand(n != 0 ? n : 1);
		return NUM;
	}else
		return c;
}

void
main(int argc, char **argv)
{
	ARGBEGIN{
	}ARGEND
	if((bf = Bfdopen(0, OREAD)) == nil)
		sysfatal("Bfdopen: %r");
	yyparse();
}
short	yyexca[] =
{-1, 1,
	1, -1,
	-2, 0,
};
#define	YYNPROD	10
#define	YYPRIVATE 57344
#define	YYLAST	31
short	yyact[] =
{
   6,   7,   8,   9,  10,   2,   1,  17,   0,   0,
  11,   0,  12,  13,  14,  15,  16,   6,   7,   8,
   9,  10,   5,   3,   8,   9,  10,   0,   0,   0,
   4
};
short	yypact[] =
{
-1000,  19,  12,-1000,  19,-1000,  19,  19,  19,  19,
  19,  -5,  17,  17,-1000,-1000,-1000,-1000
};
short	yypgo[] =
{
   0,   5,   6
};
short	yyr1[] =
{
   0,   2,   2,   1,   1,   1,   1,   1,   1,   1
};
short	yyr2[] =
{
   0,   0,   3,   1,   3,   3,   3,   3,   3,   3
};
short	yychk[] =
{
-1000,  -2,  -1,   4,  11,  10,   5,   6,   7,   8,
   9,  -1,  -1,  -1,  -1,  -1,  -1,  12
};
short	yydef[] =
{
   1,  -2,   0,   3,   0,   2,   0,   0,   0,   0,
   0,   0,   5,   6,   7,   8,   9,   4
};
short	yytok1[] =
{
   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  10,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   9,   0,   0,
  11,  12,   7,   5,   0,   6,   0,   8
};
short	yytok2[] =
{
   2,   3,   4
};
long	yytok3[] =
{
   0
};

#line	1	"/sys/lib/yaccpar"
#define YYFLAG 		-1000
#define	yyclearin	yychar = -1
#define	yyerrok		yyerrflag = 0

#ifdef	yydebug
#include	"y.debug"
#else
#define	yydebug		0
char*	yytoknames[1];		/* for debugging */
char*	yystates[1];		/* for debugging */
#endif

/*	parser for yacc output	*/

int	yynerrs = 0;		/* number of errors */
int	yyerrflag = 0;		/* error recovery flag */

extern	int	fprint(int, char*, ...);
extern	int	sprint(char*, char*, ...);

char*
yytokname(int yyc)
{
	static char x[16];

	if(yyc > 0 && yyc <= sizeof(yytoknames)/sizeof(yytoknames[0]))
	if(yytoknames[yyc-1])
		return yytoknames[yyc-1];
	sprint(x, "<%d>", yyc);
	return x;
}

char*
yystatname(int yys)
{
	static char x[16];

	if(yys >= 0 && yys < sizeof(yystates)/sizeof(yystates[0]))
	if(yystates[yys])
		return yystates[yys];
	sprint(x, "<%d>\n", yys);
	return x;
}

long
yylex1(void)
{
	long yychar;
	long *t3p;
	int c;

	yychar = yylex();
	if(yychar <= 0) {
		c = yytok1[0];
		goto out;
	}
	if(yychar < sizeof(yytok1)/sizeof(yytok1[0])) {
		c = yytok1[yychar];
		goto out;
	}
	if(yychar >= YYPRIVATE)
		if(yychar < YYPRIVATE+sizeof(yytok2)/sizeof(yytok2[0])) {
			c = yytok2[yychar-YYPRIVATE];
			goto out;
		}
	for(t3p=yytok3;; t3p+=2) {
		c = t3p[0];
		if(c == yychar) {
			c = t3p[1];
			goto out;
		}
		if(c == 0)
			break;
	}
	c = 0;

out:
	if(c == 0)
		c = yytok2[1];	/* unknown char */
	if(yydebug >= 3)
		fprint(2, "lex %.4lux %s\n", yychar, yytokname(c));
	return c;
}

int
yyparse(void)
{
	struct
	{
		YYSTYPE	yyv;
		int	yys;
	} yys[YYMAXDEPTH], *yyp, *yypt;
	short *yyxi;
	int yyj, yym, yystate, yyn, yyg;
	long yychar;
	YYSTYPE save1, save2;
	int save3, save4;

	save1 = yylval;
	save2 = yyval;
	save3 = yynerrs;
	save4 = yyerrflag;

	yystate = 0;
	yychar = -1;
	yynerrs = 0;
	yyerrflag = 0;
	yyp = &yys[-1];
	goto yystack;

ret0:
	yyn = 0;
	goto ret;

ret1:
	yyn = 1;
	goto ret;

ret:
	yylval = save1;
	yyval = save2;
	yynerrs = save3;
	yyerrflag = save4;
	return yyn;

yystack:
	/* put a state and value onto the stack */
	if(yydebug >= 4)
		fprint(2, "char %s in %s", yytokname(yychar), yystatname(yystate));

	yyp++;
	if(yyp >= &yys[YYMAXDEPTH]) {
		yyerror("yacc stack overflow");
		goto ret1;
	}
	yyp->yys = yystate;
	yyp->yyv = yyval;

yynewstate:
	yyn = yypact[yystate];
	if(yyn <= YYFLAG)
		goto yydefault; /* simple state */
	if(yychar < 0)
		yychar = yylex1();
	yyn += yychar;
	if(yyn < 0 || yyn >= YYLAST)
		goto yydefault;
	yyn = yyact[yyn];
	if(yychk[yyn] == yychar) { /* valid shift */
		yychar = -1;
		yyval = yylval;
		yystate = yyn;
		if(yyerrflag > 0)
			yyerrflag--;
		goto yystack;
	}

yydefault:
	/* default state action */
	yyn = yydef[yystate];
	if(yyn == -2) {
		if(yychar < 0)
			yychar = yylex1();

		/* look through exception table */
		for(yyxi=yyexca;; yyxi+=2)
			if(yyxi[0] == -1 && yyxi[1] == yystate)
				break;
		for(yyxi += 2;; yyxi += 2) {
			yyn = yyxi[0];
			if(yyn < 0 || yyn == yychar)
				break;
		}
		yyn = yyxi[1];
		if(yyn < 0)
			goto ret0;
	}
	if(yyn == 0) {
		/* error ... attempt to resume parsing */
		switch(yyerrflag) {
		case 0:   /* brand new error */
			yyerror("syntax error");
			yynerrs++;
			if(yydebug >= 1) {
				fprint(2, "%s", yystatname(yystate));
				fprint(2, "saw %s\n", yytokname(yychar));
			}

		case 1:
		case 2: /* incompletely recovered error ... try again */
			yyerrflag = 3;

			/* find a state where "error" is a legal shift action */
			while(yyp >= yys) {
				yyn = yypact[yyp->yys] + YYERRCODE;
				if(yyn >= 0 && yyn < YYLAST) {
					yystate = yyact[yyn];  /* simulate a shift of "error" */
					if(yychk[yystate] == YYERRCODE)
						goto yystack;
				}

				/* the current yyp has no shift onn "error", pop stack */
				if(yydebug >= 2)
					fprint(2, "error recovery pops state %d, uncovers %d\n",
						yyp->yys, (yyp-1)->yys );
				yyp--;
			}
			/* there is no state on the stack with an error shift ... abort */
			goto ret1;

		case 3:  /* no shift yet; clobber input char */
			if(yydebug >= 2)
				fprint(2, "error recovery discards %s\n", yytokname(yychar));
			if(yychar == YYEOFCODE)
				goto ret1;
			yychar = -1;
			goto yynewstate;   /* try again in the same state */
		}
	}

	/* reduction by production yyn */
	if(yydebug >= 2)
		fprint(2, "reduce %d in:\n\t%s", yyn, yystatname(yystate));

	yypt = yyp;
	yyp -= yyr2[yyn];
	yyval = (yyp+1)->yyv;
	yym = yyn;

	/* consult goto table to find next state */
	yyn = yyr1[yyn];
	yyg = yypgo[yyn];
	yyj = yyg + yyp->yys + 1;

	if(yyj >= YYLAST || yychk[yystate=yyact[yyj]] != -yyn)
		yystate = yyact[yyg];
	switch(yym) {
		
case 2:
#line	26	"/usr/qwx/p/misc/calc.y"
{ print("%d\n", yypt[-1].yyv.i); } break;
case 3:
#line	28	"/usr/qwx/p/misc/calc.y"
{ yyval.i = yypt[-0].yyv.i; } break;
case 4:
#line	29	"/usr/qwx/p/misc/calc.y"
{ yyval.i = yypt[-1].yyv.i; } break;
case 5:
#line	30	"/usr/qwx/p/misc/calc.y"
{ yyval.i = yypt[-2].yyv.i + yypt[-0].yyv.i; } break;
case 6:
#line	31	"/usr/qwx/p/misc/calc.y"
{ yyval.i = yypt[-2].yyv.i - yypt[-0].yyv.i; } break;
case 7:
#line	32	"/usr/qwx/p/misc/calc.y"
{ yyval.i = yypt[-2].yyv.i * yypt[-0].yyv.i; } break;
case 8:
#line	33	"/usr/qwx/p/misc/calc.y"
{ yyval.i = yypt[-2].yyv.i / yypt[-0].yyv.i; } break;
case 9:
#line	34	"/usr/qwx/p/misc/calc.y"
{ yyval.i = yypt[-2].yyv.i % yypt[-0].yyv.i; } break;
	}
	goto yystack;  /* stack new state and value */
}
