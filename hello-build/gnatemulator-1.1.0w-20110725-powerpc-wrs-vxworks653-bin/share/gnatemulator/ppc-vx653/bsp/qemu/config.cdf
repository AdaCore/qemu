/* config.cdf - BSP modules configuration file */

/* Copyright 2004 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,28jun04,sfs removed mv643xxEnd.o, moved to sysLib.c
01a,10jan04,ksn created by teamF1.
*/

Component INCLUDE_BSP_VXWORKS {
        NAME		VxWorks Board Support Package
        MODULES		dataSegPad.o \
			version.o \
			sysALib.o \
			sysLib.o \
			usrAppInit.o \
			prjConfig.o
}
