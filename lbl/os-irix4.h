/*
 * Copyright (c) 1994, 1995, 1996
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

#define SIGRET int
#define SIGRETVAL (0)
#define WAITSTATUS union wait

#ifndef ETHERTYPE_REVARP
#define ETHERTYPE_REVARP 0x8035
#endif

#ifndef	IPPROTO_ND
/* From <netinet/in.h> on a Sun somewhere. */
#define	IPPROTO_ND	77
#endif

#ifndef REVARP_REQUEST
#define REVARP_REQUEST 3
#endif
#ifndef REVARP_REPLY
#define REVARP_REPLY 4
#endif

/* newish RIP commands */
#ifndef	RIPCMD_POLL
#define	RIPCMD_POLL 5
#endif
#ifndef	RIPCMD_POLLENTRY
#define	RIPCMD_POLLENTRY 6
#endif
