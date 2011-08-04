/* sysCache.c - cache library for 750GX */

/* Copyright 1984-2005 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01d,30mar05,foo  Update for fast warm restart
01c,19jan05,rlp  Removed the INCLUDE_CACHE_SUPPORT directive.
01b,05jul04,kp   modified for 750GX
01a,17feb04,vnk  Written (TeamF1 Inc.)
*/

/*
DESCRIPTION

This library provides L2 internal cache support.

The routines are called in the context of the architecture specific cache
routines.

*/
 
/* includes */

#include "sysCache.h"

#ifdef	INCLUDE_CACHE_L2

/* defines */



/* globals */

void sysL2CacheInit (void);
void sysL2CacheEnable (void);
void sysL2CacheDisable (void);
void sysL2CacheFlush (void);
void sysL2CacheInv (void);


/* externals */

IMPORT UINT sysL2crGet (void);
IMPORT void sysL2crPut (UINT regVal);
IMPORT void sysCacheFlushInternal (void *, int, int);
IMPORT char * cachePpcReadOrigin;


/******************************************************************************
*
* sysL2CacheInit - initialize the L2 cache 
*
* This routine initializes and invalidates the L2 cache and keeps it ready
* to be enabled.
*
* RETURNS: N/A
*
* SEE ALSO: sysL2CacheFlush(), sysL2CacheEnable(), sysL2CacheDisable(),
*	    sysL2CacheInv()
*/ 


void sysL2CacheInit
    (
    void
    )
    {
    UINT 	l2crVal;
    
    WRS_ASM ("sync");  /* wait for all memory transactions to finish */

    /* init only if disabled */
    if (((l2crVal = sysL2crGet()) & _PPC_L2CR_ENABLE) == 0)
        {

        sysL2crPut (sysL2crGet () | _PPC_L2CR_INVALIDATE);
    
        WRS_ASM ("sync");  /* wait for all memory transactions to finish */

       while (sysL2crGet () & _PPC_L2CR_INPROGRESS) /* poll until turns zero */
        ;

        /* Turn off the invalidate bit */
    
        sysL2crPut (sysL2crGet () & ~_PPC_L2CR_INVALIDATE); 
    
        sysL2CacheDisable (); /* leave the cache in a disabled state */
        }
    }



/******************************************************************************
*
* sysL2CacheEnable - enable the L2 cache 
*
* This routine invalidates and enables the L2 cache
*
* RETURNS: N/A
*
* SEE ALSO: sysL2CacheInv(), sysL2CacheFlush(), sysL2CacheDisable()
*/ 


void sysL2CacheEnable
    (
    void
    )
    {
    UINT 	l2crVal;
    int         lockKey;

    lockKey = intLock();

    WRS_ASM ("sync");
   
    /* enable only if disabled */
    
    if (((l2crVal = sysL2crGet()) & _PPC_L2CR_ENABLE) == 0)
        {

        /* Invalidate the L2 */
   
        sysL2crPut (sysL2crGet() | _PPC_L2CR_INVALIDATE);
    
        while (sysL2crGet() & _PPC_L2CR_INPROGRESS)
            ;

        l2crVal = sysL2crGet();

#ifdef USER_L2_CACHE_DATA_ONLY
    
        /*  use the L2 for data only and not for instructions. */
        l2crVal |= _PPC_L2CR_DATA_ONLY;
    
#endif

        sysL2crPut((l2crVal | _PPC_L2CR_ENABLE) & ~_PPC_L2CR_INVALIDATE);
    
        WRS_ASM ("sync");    /* wait for all memory transactions to finish */
    
        }

    intUnlock (lockKey);

    }



/******************************************************************************
*
* sysL2CacheDisable - disable the L2 cache
*
* This routine disables the L2 cache.
* sysL2CacheInit().
*
* RETURNS: N/A
*
* SEE ALSO: sysL2CacheInit()
*/

void sysL2CacheDisable
    (
    void
    )
    {
    UINT 	l2crVal;

    /* disable only if enabled */    
    if ((l2crVal = sysL2crGet()) & _PPC_L2CR_ENABLE)
        {
        WRS_ASM ("sync");

        sysL2crPut (sysL2crGet() & ~(_PPC_L2CR_ENABLE));

        WRS_ASM ("sync");
        }
    }



/******************************************************************************
*
* sysL2CacheFlush - flush the L2 cache(s)
*
* This routine flushes the L2 cache if previously initialized using
* sysL2CacheInit().
*
* RETURNS: N/A
*
* SEE ALSO: sysL2CacheInit(), sysL2CacheEnable(), sysL2CacheDisable()
*/

void sysL2CacheFlush
    (
    void
    )
    {
    int      lockKey;
    int      i;
    char     tmp;
    volatile char * addr;

    lockKey = intLock ();

    WRS_ASM ("sync"); 

    if (vxHid0Get () & _PPC_HID0_DCE)
        {
        /* Flush by reading one word from each cache line within a cache
         * enabled buffer.  Note that this buffer should be twice the size of
         * the L2 cache to override the pseudo-LRU replacement algorithm.
         */

        addr = cachePpcReadOrigin;
        
        i = (IBM750GX_INLINE_CACHE_SIZE * 2) / 32 / 8; /* number of loops */

        while (i--)
            {
            tmp = addr[0];
            tmp = addr[32*1];
            tmp = addr[32*2];
            tmp = addr[32*3];
            tmp = addr[32*4];
            tmp = addr[32*5];
            tmp = addr[32*6];
            tmp = addr[32*7];
            addr += 32*8;
            }
        }

    WRS_ASM ("sync"); 

    intUnlock(lockKey);
    }


/******************************************************************************
*
* sysL2CacheInv - Invalidate the L2 cache 
*
* This routine invalidates the entire L2 cache.
*
* RETURNS: N/A
*
* SEE ALSO: sysL2CacheFlush(), sysL2CacheEnable(), sysL2CacheDisable()
*/ 



void sysL2CacheInv (void)
    {
    int lockKey;
    volatile UINT l2crVal;

    
    lockKey=intLock();

    l2crVal = sysL2crGet();

    WRS_ASM ("sync");

    /* Invalidate the L2 */

    sysL2crPut (l2crVal | _PPC_L2CR_INVALIDATE);

    while ((sysL2crGet()) & (_PPC_L2CR_INPROGRESS)) /* poll until turns zero */
        ;
    
    sysL2crPut (l2crVal);
    
    WRS_ASM ("sync");
    
    intUnlock(lockKey);
    }


#endif /* INCLUDE_CACHE_L2 */
