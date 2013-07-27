/*
 * Copyright (c) 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header$ (LBL)
 */

/* Prototypes missing in Utah bsd */
#ifdef FILE
int	_filbuf(FILE *);
int	_flsbuf(u_char, FILE *);
int	fclose(FILE *);
int	fflush(FILE *);
int	fprintf(FILE *, const char *, ...);
int	fputs(const char *, FILE *);
u_int	fread(void *, u_int, u_int, FILE *);
u_int	fwrite(const void *, u_int, u_int, FILE *);
void	rewind(FILE *);
u_int	vfprintf(FILE *, const char *, char *);	/* arg 3 is really va_list */
#endif

__dead	void abort(void) __attribute__((volatile));
int	abs(int);
#ifdef  __STDC__
struct	sockaddr;
#endif
int	accept(int, struct sockaddr *, int *);
int	atoi(const char *);
int	bcmp(const void *, const void *, u_int);
void	bcopy(const void *, void *, u_int);
void	bzero(void *, u_int);
#ifdef notdef
char	*calloc(u_int, u_int);
#endif
int	chmod(const char *, int);
int	close(int);
int	connect(int, struct sockaddr *, int);
char	*crypt(const char *, const char *);
int	execv(const char *, char * const *);
__dead	void exit(int) __attribute__((volatile));
__dead	void _exit(int) __attribute__((volatile));
int	fchmod(int, int);
int	fchown(int, int, int);
int	fcntl(int, int, int);
int	ffs(int);
#ifdef  __STDC__
struct	stat;
#endif
int	fstat(int, struct stat *);
char	*getenv __P((char *));
int	getopt(int, char * const *, const char *);
char	*getlogin __P((void));
int	getpeername(int, struct sockaddr *, int *);
int	getsockname(int, struct sockaddr *, int *);
int	getsockopt(int, int, int, char *, int *);
#ifdef  __STDC__
struct	timeval;
struct	timezone;
#endif
int	gettimeofday(struct timeval *, struct timezone *);
int	ioctl(int, int, caddr_t);
int	isatty(int);
int	kill(int, int);
int	listen(int, int);
__dead	void longjmp(int *, int) __attribute__((volatile));
off_t	lseek(int, off_t, int);
int	lstat(const char *, struct stat *);
char	*memcpy(char *, const char *, u_int);
int	mkdir(const char *, int);
int	open(char *, int, ...);
void	perror(const char *);
int	printf(const char *, ...);
long	random(void);
int	recv(int, char *, u_int, int);
int	rresvport(int *);
int	select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int	setjmp(int *);
int	setsockopt(int, int, int, char *, int);
int	sigblock(int);
int	sigpause(int);
int	sigsetmask(int);
int	snprintf(char *, size_t, const char *, ...);
int	socket(int, int, int);
void	srandom(int);
int	sscanf(char *, const char *, ...);
#ifdef  __STDC__
struct	stat;
#endif
int	stat(const char *, struct stat *);
int	strcmp(const char *, const char *);
char	*strcpy(char *, const char *);
char	*strdup(const char *);
char	*strerror(int);
int	strlen(const char *);
long	strtol(const char *, char **, int);
long	tell(int);
time_t	time(time_t *);
char	*ttyname __P((int));
int	vfork(void);
char	*vsprintf(char *, const char *, ...);
int	shutdown(int, int);
int	umask(int);
void	unsetenv(const char *);
int	utimes(const char *, struct timeval *);
#ifdef  __STDC__
union	wait;
struct	rusage;
#endif
int	wait(union wait *);
int	wait3(union wait *, int, struct rusage *);

#define SIGRET void
#define SIGRETVAL
#define WAITSTATUS union wait

extern	int opterr, optind, optopt;
extern	char *optarg;

/* Ugly signal hacking */
#ifdef BADSIG
#undef BADSIG
#define BADSIG		(int (*)(int))-1
#undef SIG_DFL
#define SIG_DFL		(int (*)(int))0
#undef SIG_IGN
#define SIG_IGN		(int (*)(int))1

#ifdef KERNEL
#undef SIG_CATCH
#define SIG_CATCH	(int (*)(int))2
#endif
#undef SIG_HOLD
#define SIG_HOLD	(int (*)(int))3
#endif

/* SunOS 3 signal compat */
#define WTERMSIG(status)	((status).w_termsig)
#define WEXITSTATUS(status)	((status).w_retcode)

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#define STDOUT_FILENO 1
#define STDIN_FILENO 0
#endif

#ifndef FD_SET
#define FD_SET(n, p)	((p)->fds_bits[0] |= (1<<(n)))
#define FD_CLR(n, p)	((p)->fds_bits[0] &= ~(1<<(n)))
#define FD_ISSET(n, p)	((p)->fds_bits[0] & (1<<(n)))
#define FD_ZERO(p)	((p)->fds_bits[0] = 0)
#endif

#ifndef S_ISTXT
#define S_ISTXT S_ISVTX
#endif

#ifndef S_IRWXU
#define S_IRWXU 0000700			/* RWX mask for owner */
#define S_IRWXG 0000070			/* RWX mask for group */
#define S_IRWXO 0000007			/* RWX mask for other */
#endif

#ifndef S_ISDIR
#define S_ISDIR(m)	((m & S_IFMT) == S_IFDIR)	/* directory */
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef SIGUSR1
#define SIGUSR1 30
#endif

int setpriority(int, int, int);
int getpriority(int, int);
int	setenv(const char *, const char *, int);
void	syslog(int, const char *, ...);
void	openlog(const char *, int, int);
int	strcasecmp(const char *, const char *);
void	endpwent(void);
#ifdef __STDC__
struct	utmp;
#endif
void	login(struct utmp *);
int	logout(const char *);
