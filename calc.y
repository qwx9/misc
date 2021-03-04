%{
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

int yylex(void);
int yyparse(void);
void yyerror(char*);

Biobuf *bf;
%}

%union{
	int i;
}

%token<i> NUM
%type<i> expr

%left '+' '-'
%left '*' '/' '%'

%%

input:	| input expr '\n' { print("%d\n", $2); }

expr:	NUM { $$ = $1; }
	| '(' expr ')' { $$ = $2; }
	| expr '+' expr { $$ = $1 + $3; }
	| expr '-' expr { $$ = $1 - $3; }
	| expr '*' expr { $$ = $1 * $3; }
	| expr '/' expr { $$ = $1 / $3; }
	| expr '%' expr { $$ = $1 % $3; }

%%

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
