/* sysCache.h - cache header */

/* Copyright 1984-2004 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,05jul04,kp   modified for 750GX
01a,16feb04,vnk  written by (TeamF1)
*/

#ifndef    INCsysCacheh
#define    INCsysCacheh

#ifdef __cplusplus
    extern "C" {
#endif /* __cplusplus */

#define _PPC_BIT(x)	(1 << (31 - (x)))	/* bits are reverse */


#define _PPC_L2CR_ENABLE     0x80000000   /* L2 enable bit                 */
#define _PPC_L2CR_DATA_ONLY  0x00400000   /* L2 data-only, no instructions */
#define _PPC_L2CR_INVALIDATE 0x00200000   /* L2 Global Invalidate bit      */
#define _PPC_L2CR_TEST_SPRT  0x00040000   /* L2 Test Support               */
#define _PPC_L2CR_INPROGRESS 0x00000001   /* L2 Invalidate in-progress bit */
        
#define IBM750GX_INLINE_CACHE_SIZE (1024 * 1024)
        
#ifdef __cplusplus
    }
#endif /* __cplusplus */
#endif    /* INCsysCacheh */
        
