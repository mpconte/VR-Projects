#include <errno.h>
#include <stropts.h>
#include <poll.h>
#include "scan.h"

/*
 * older pure System V has no call
 * to return time with a resolution of less than a second
 * many `System V' systems have gettimeofday added
 * be pure for now
 */

/*
 * table of actions to take when a file is ready
 */
#define	MAXFDS	20	/* max number, not max value */

typedef struct {
	int fd;
	void (*act)();
	void *arg;
} Ftab;

static Ftab ftab[MAXFDS];
static struct pollfd polltab[MAXFDS];
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
	polltab[i].fd = fd;
	polltab[i].events = POLLIN|POLLHUP|POLLERR;
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

	for (i = 0; i < maxftab; i++)
		if (ftab[i].fd == fd) {
			ftab[i].fd = -1;
			polltab[i].fd = -1;
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
 * main loop of the program:
 * wait for files to be ready,
 * process them
 */

static sc_set *
fdtosc()
{
	static sc_set wsp;

	SC_ZERO(&wsp);
	/* later, fill in data here */
	return (&wsp);
}

scan(tmo)
int tmo;
{
	int n;
	register int i;
	register int ev;
	long now, target;

	if (tmo > 0) {
		time(&target);
		target += (tmo + 999) / 1000;	/* round up */
	}
	for (;;) {
		if ((n = poll(polltab, maxftab, tmo)) <= 0) {
			if (n < 0 && errno != EINTR)
				return (-1);
			if (tmo < 0)
				continue;
			if (tmo == 0)
				return (0);
			time(&now);
			if (now >= target)
				return (0);
			tmo = (target - now) * 1000;
			continue;
		}
		for (i = 0; i < maxftab && n > 0; i++) {
			if ((ev = polltab[i].revents) == 0)
				continue;
			if (ev & POLLNVAL) {	/* shouldn't happen */
				polltab[i].fd = -1;	/* but don't loop */
				continue;
			}
			--n;
			(*ftab[i].act)(ftab[i].fd, ftab[i].arg, fdtosc());
		}
		/* n == 0 here; check that? */
		if (tmo >= 0)
			return (1);
	}
}
