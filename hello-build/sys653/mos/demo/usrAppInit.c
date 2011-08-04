/* usrAppInit.c - stub application initialization routine */

/* Copyright 1984-2005 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01c,28nov05,f_l  TRACE B0322. B0323
01b,26jan99,kk   added include prjParams.h to pull in define for USER_APPL_INIT
01a,02jun98,ms   written
*/

/*
DESCRIPTION
Initialize user application code.
*/ 

#include "stdio.h"
#include "prjParams.h"
#include "usrAppInit.h"

#ifdef USER_APPL_ENTRYPOINT
extern void USER_APPL_ENTRYPOINT ();
#endif

/******************************************************************************
*
* usrAppInit - initialize the users application
*/ 

void usrAppInit (void)
    {
#ifdef USER_APPL_ENTRYPOINT
    USER_APPL_ENTRYPOINT ();
#endif

#ifdef	USER_APPL_INIT
	USER_APPL_INIT;		/* for backwards compatibility */
#endif

    /* add application specific code here */
    }


