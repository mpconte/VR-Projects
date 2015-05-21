#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>
#ifndef FD_ISSET
#include "fd_set.h"
#endif
#include "scan.h"

/*
 * table of actions to take when a file is ready
 */
#define	MAXFDS	1024	/* max number, not max value */

typedef struct {
	int fd;
	void (*act)();
	void *arg;
} Ftab;

static Ftab ftab[MAXFDS];

static fd_set selfds, selwfds;
static int maxfd;
static int maxftab;
static int wantwrite;

/*
 * need to know which fds may be written?
 */
void
scanwfd(wanted)
int wanted;
{
	wantwrite = wanted;
}

/*
 * arrange to call act(fd)
 * when fd is ready to read
 */
addfd(fd, act, arg)
int fd;
void (*act)();
void *arg;
{
	register int i;

	for (i = 0; i < maxftab; i++)
		if (ftab[i].fd < 0)
			break;
	if (i >= maxftab) {
		if (maxftab >= MAXFDS)
			return (-1);
		i = maxftab++;
	}
	ftab[i].fd = fd;
	ftab[i].act = act;
	ftab[i].arg = arg;
	if (fd >= maxfd)
		maxfd = fd + 1;
	FD_SET(fd, &selfds);
	FD_SET(fd, &selwfds);
	return (0);
}

/*
 * ignore fd hereafter
 */
void
remfd(fd)
int fd;
{
	register int i;

	if (fd < maxfd && fd >= 0) {
		FD_CLR(fd, &selfds);
		FD_CLR(fd, &selwfds);
	}
	for (i = 0; i < maxftab; i++)
		if (ftab[i].fd == fd) {
			ftab[i].fd = -1;
			if (i == maxftab - 1) {	/* compress table */
				while (--i >= 0 && ftab[i].fd < 0)
					;
				maxftab = i + 1;
			}
			return;
		}
	/* not found; perhaps blocked proc died */
}

/*
 * add a file descriptor to test for writing but not reading
 */
void
addwfd(fd)
int fd;
{
	FD_SET(fd, &selwfds);
	if (fd >= maxfd)
		maxfd = fd + 1;
}

/*
 * remove a write-only fd
 */
void
remwfd(fd)
int fd;
{
	if (fd < maxfd && fd >= 0)
		FD_CLR(fd, &selwfds);
}

static sc_set *
fdtosc(fsp)
register fd_set *fsp;
{
	static sc_set wsp;
	register int i;
	register int mfd;

	SC_ZERO(&wsp);
	if ((mfd = maxfd) > SCMAXFD)
		mfd = SCMAXFD;
	for (i = 0; i < mfd; i++)
		if (FD_ISSET(i, fsp))
			SC_SET(i, &wsp);
	return (&wsp);
}

/*
 * petty details:
 * why a separate select to fill in wr?
 * because files are usually available for writing,
 * so the select for reading would usually return early
 * with no information about reads
 *
 * some care taken not to call ftime if tmo == 0,
 * because that means a poll, for which speed may matter
 */

scan(tmo)
int tmo;
{
	fd_set rd, ex, wr;
	int n;
	register int i;
	register int fd;
	struct timeval now, target;
	struct timeval tv0, tvm;

	FD_ZERO(&wr);
	tv0.tv_sec = tv0.tv_usec = 0;
	if (tmo > 0) {
		gettimeofday(&target, (void *)0);
		target.tv_sec += tmo/1000;
		target.tv_usec += (tmo%1000)*1000;
		if (target.tv_usec > 1000000) {
			target.tv_usec -= 1000000;
			target.tv_sec++;
		}
	}
	for (;;) {
		rd = selfds;
		ex = selfds;
		tvm.tv_sec = tmo/1000;
		tvm.tv_usec = (tmo%1000)*1000;
		n = select(maxfd, &rd, (fd_set *)0, &ex,
			tmo < 0 ? (struct timeval *)0 : &tvm);
		if (n <= 0) {
			if (n < 0 && errno != EINTR)
				return (-1);
			if (tmo < 0)
				continue;
			if (tmo == 0)
				return (0);
			gettimeofday(&now, (void *)0);
			tmo = (target.tv_sec - now.tv_sec)*1000
				+ (target.tv_usec - now.tv_usec)/1000;
			if (tmo <= 0)
				return (0);
			continue;
		}
		if (wantwrite) {
			wr = selwfds;
			(void) select(maxfd, (fd_set *)0, &wr, (fd_set *)0, &tv0);
		}
		for (i = 0; i < maxftab && n > 0; i++) {
			fd = ftab[i].fd;
			if (fd < 0)
				continue;
			if (FD_ISSET(fd, &rd) == 0
			&&  FD_ISSET(fd, &ex) == 0)
				continue;
			--n;
			(*ftab[i].act)(fd, ftab[i].arg, fdtosc(&wr));
		}
		/* n == 0 here; check that? */
		if (tmo >= 0)
			return (1);
	}
}
