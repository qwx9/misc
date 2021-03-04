#include <u.h>
#include <libc.h>

#define usage() (sysfatal("usage: no."))

void
main(int argc, char **argv)
{
	int fd;
	Dir d;

	nulldir(&d);
	ARGBEGIN{
	case 'g': d.gid = EARGF(usage()); break;
	case 'l': d.length = strtoll(EARGF(usage()), nil, 0); break;
	case 'm': d.mtime = strtoll(EARGF(usage()), nil, 0); break;
	case 'n': d.name = EARGF(usage()); break;
	case 'p': d.mode = strtoll(EARGF(usage()), nil, 0); break;
	case 'u': d.uid = EARGF(usage()); break;
	default: usage();
	}ARGEND
	if(*argv == nil)
		usage();
	fd = open(*argv, OWRITE);
	if(fd < 0)
		sysfatal("open: %r");
	if(dirfwstat(fd, &d) < 0)
		sysfatal("dirfwstat: %r");
	close(fd);
	exits(nil);
}
