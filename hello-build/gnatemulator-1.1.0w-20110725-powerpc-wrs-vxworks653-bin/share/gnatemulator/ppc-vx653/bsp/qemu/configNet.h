/* configNet.h - network configuration header */

/* Copyright 1984-1999 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,12jun02,kab  SPR 74987: cplusplus protection
01a,10oct99,mtl   written from yk 750 by teamF1

*/
 
#if (defined(INCLUDE_END) && defined(INCLUDE_NETWORK))

#ifndef INCnetConfigh
#define INCnetConfigh

#ifdef __cplusplus
    extern "C" {
#endif

/* includes */

#include "vxWorks.h"
#include "end.h"

/* defines */

/* Primary network device */
#define INCLUDE_PRIMARY_END
#define END_LOAD_FUNC_PRI 	sysNe2000EndLoad
#define END_BUFF_LOAN_PRI 	1

/* Secondary network device */
/* secondary devices are currently unsupported in this release */

/* The END_LOAD_STRING is defined empty and created dynamicaly */

#define END_LOAD_STRING "0:"      /* created in sys<device>End.c */

/* define IP_MAX_UNITS to the actual number in the table. */

#ifndef IP_MAX_UNITS 
#define IP_MAX_UNITS            (NELEMENTS(endDevTbl) - 1)
#endif  /* ifndef IP_MAX_UNITS */

#if defined(INCLUDE_PRIMARY_END)
IMPORT END_OBJ * END_LOAD_FUNC_PRI (char *, void*);
#endif

/* each entry will be processed by muxDevLoad() */

END_TBL_ENTRY endDevTbl [] =
    {
#if defined(INCLUDE_PRIMARY_END)
    {0, END_LOAD_FUNC_PRI, END_LOAD_STRING, END_BUFF_LOAN_PRI, NULL},
#endif
    {0, END_TBL_END, NULL, 0, NULL}, 	/* must be last */
    };
#endif /* INCnetConfigh */

#ifdef __cplusplus
    }
#endif

#endif /* (defined (INCLUDE_END) && defined (INCLUDE_NETWORK)) */
