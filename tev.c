#include <u.h>
#include <libc.h>
#include <draw.h>
#include <keyboard.h>
#include <mouse.h>

void
mproc(void)
{
	int fd, n, nerr;
	char buf[1+5*12];
	Mouse m;

	fd = open("/dev/mouse", ORDWR);
	if(fd < 0)
		sysfatal("open /dev/mouse: %r");
	memset(&m, 0, sizeof m);
	nerr = 0;
	for(;;){
		n = read(fd, buf, sizeof buf);
		if(n != 1+4*12){
			if(n < 0 || ++nerr > 10)
				break;
			continue;
		}
		nerr = 0;
		switch(*buf){
		case 'r':
			print("[r]\n");
			/* wet floor */
		case 'm':
			m.xy.x = atoi(buf+1+0*12);
			m.xy.y = atoi(buf+1+1*12);
			m.buttons = atoi(buf+1+2*12);
			m.msec = atoi(buf+1+3*12);
			print("[m] %d,%d %d %lud\n", m.xy.x, m.xy.y, m.buttons, m.msec);
			break;
		}
	}
}

void
main(int, char **)
{
	int n, fd, mpid;
	char buf[256], *s;
	Rune r;

	mpid = fork();
	switch(mpid){
	case -1:
		sysfatal("fork: %r");
	case 0:
		mproc();
		exits(nil);
	}
	fd = open("/dev/kbd", OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	memset(buf, 0, sizeof buf);
	for(;;){
		if(buf[0] != 0){
			n = strlen(buf)+1;
			memmove(buf, buf+n, sizeof(buf)-n);
		}
		if(buf[0] == 0){
			print("\n");
			n = read(fd, buf, sizeof(buf)-1);
			if(n <= 0)
				break;
			buf[n-1] = 0;
			buf[n] = 0;
		}
		s = buf+1;
		switch(buf[0]){
		case 'c':
			chartorune(&r, s);
			print("[c] %C %#ux ", r, r);
			if(utfrune(buf, Kdel))
				goto done;
			break;
		case 'k':
		case 'K':
			print("[%c] ", buf[0]);
			while(*s){
				s += chartorune(&r, s);
				print("%C %#ux ", r, r);
				if(r == Kdel)
					goto done;
			}
			break;
		default:
			print("unknown message %c\n", *buf);
		}
	}
done:
	postnote(PNPROC, mpid, "shutdown");
	exits(nil);
}
