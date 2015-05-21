#ifndef SCAN_H
#define SCAN_H
/*
 * fd_set clone for portability
 * why not just use fd_set?
 * (1) because FD_* macros are different on Research UNIX and 4BSD
 * (2) to support systems that just use poll sted select
 */

#define	SCMAXFD	256	/* sufficient unto the day */
#define	SCCHAR	8	/* bits per char */
#define	SCSIZE	((SCMAXFD+SCCHAR-1)/SCCHAR)

typedef struct {
	unsigned char sbits[SCSIZE];
} sc_set;

#define	SC_SET(fd, sc)	((sc)->sbits[(fd)/SCCHAR] |= (1<<((fd)%SCCHAR)))
#define	SC_CLR(fd, sc)	((sc)->sbits[(fd)/SCCHAR] &=~ (1<<((fd)%SCCHAR)))
#define	SC_ISSET(fd, sc)  (((sc)->sbits[(fd)/SCCHAR] & (1<<((fd)%SCCHAR))) != 0)
#define	SC_ZERO(sc)	{register int i; for (i=0; i<SCSIZE; i++) (sc)->sbits[i] = 0;}

void scanwfd();
int addfd();
void remfd();
int scan();
void addwfd(), remwfd();

#endif /* SCAN_H */
