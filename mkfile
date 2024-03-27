</$objtype/mkfile
TARG=\
	calc\
	cpick\
	hcol\
	lr\
	mev\
	rfx\
	spd\
	tev\
	vol\
	wstat\

HFILES=
YFLAGS=-D
</sys/src/cmd/mkmany
BIN=$home/bin/$objtype

%.tab.h %.tab.c:D:	%.y
	$YACC $YFLAGS -s $stem $prereq

(calc).c:R:	\1.tab.c
	mv $stem1.tab.c $stem1.c
