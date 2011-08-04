/* config.h - wrSbc750gx configuration header file */

/* Copyright 2004 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,29jul04,kam  moved reference to wrSbc750GX.h to wrSbc750gx.h
01b,17feb04,kp   added l2 cache support
01a 27dec03,ksn  teamF1, Created. 
*/


/*
DESCRIPTION

This file contains the configuration parameters for the
wrSbc7457/wrSb7447/wrSbc750gx BSP.
*/


#ifndef	INCconfigh
#define	INCconfigh

#ifdef __cplusplus
extern "C" {
#endif

#include "configAll.h"

    
#if defined(PRJ_BUILD)
#include "prjParams.h"
#endif

#include "wrSbc750gx.h"

#ifdef __cplusplus
}
#endif

#endif	/* INCconfigh */

