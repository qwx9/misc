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
	wstat\
	mid2s\
	midump\

HFILES=
YFLAGS=-D
</sys/src/cmd/mkmany
BIN=$home/bin/$objtype

%.tab.h %.tab.c:D:	%.y
	$YACC $YFLAGS -s $stem $prereq

(calc).c:R:	\1.tab.c
	mv $stem1.tab.c $stem1.c

$O.mid2s: mid2s.$O midifile.$O
	$LD $LDFLAGS -o $target $prereq

$O.s2mid: s2mid.$O midifile.$O
	$LD $LDFLAGS -o $target $prereq

$O.midump: midump.$O midifile.$O
	$LD $LDFLAGS -o $target $prereq
