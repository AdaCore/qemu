/* sysAta.c - ATA-2 initialization for sysLib.c */

/* Copyright 1984-1999 Wind River Systems, Inc. */

#include "copyright_wrs.h"

/*
modification history
--------------------
01b,27aug01,dgp  change manual pages to reference entries per SPR 23698
01a,01apr99,jkf  written
*/

/* 
Description

This file contains the sysAtaInit() necessary for
initializing the ATA/EIDE subsystem on the Winbond chip. 
This file will enable the controller and also set the 
PIO mode as detected by ataDrv().
*/

/* includes */

#include "vxWorks.h"
#include "config.h"

#ifdef	INCLUDE_ATA

#include "drv/hdisk/ataDrv.h"

/* defines */

/* external declarations */

/* global declarations */

/* locals */

/* function declarations */

/******************************************************************************
*
* sysAtaInit - initialize the EIDE/ATA interface
*
* Perform the necessary initialization required before starting up the
* ATA/EIDE driver on the W83C553.  The first time it is called with 
* ctrl and drive = 0.   The next time it is called is from ataDrv with
* the appropriate controller and drive to determine PIO mode to use
* on the drive.  See the ataDrv reference entry.
*/

void sysAtaInit
    (
    BOOL ctrl
    )
    {
      sysAtaResources[0].resource.ioStart[0] = PCI_MSTR_ISA_IO_LOCAL + 0x1f0;
      sysAtaResources[0].resource.ioStart[1] = PCI_MSTR_ISA_IO_LOCAL + 0x3f6;

      sysAtaResources[1].resource.ioStart[0] = PCI_MSTR_ISA_IO_LOCAL + 0x170;
      sysAtaResources[1].resource.ioStart[1] = PCI_MSTR_ISA_IO_LOCAL + 0x376;

      /*
       * initialize the remainder of the ataRsources structure
       * First, initialize the Controller 0 data structure
       */
      
      sysAtaResources[0].ctrlType 		= ATA_LOCAL;
      sysAtaResources[0].drives 		= 1;
      sysAtaResources[0].intVector 	= (int)IDE_CNTRLR0_INT_LVL;
      sysAtaResources[0].intLevel 		= (int)IDE_CNTRLR0_INT_LVL;
      sysAtaResources[0].configType 	= (ATA_PIO_AUTO | ATA_GEO_PHYSICAL);
      sysAtaResources[0].semTimeout 	= 0;
      sysAtaResources[0].wdgTimeout 	= 0;

#if 0      
      /* Second, initialize the Controller 1 data structure */
      
      sysAtaResources[1].ctrlType 		= IDE_LOCAL;
      sysAtaResources[1].drives 		= 2;
      sysAtaResources[1].intVector 	= (int)IDE_CNTRLR1_INT_LVL;
      sysAtaResources[1].intLevel	 	= (int)IDE_CNTRLR1_INT_LVL;
      sysAtaResources[1].configType 	= (ATA_PIO_AUTO | ATA_GEO_PHYSICAL);
      sysAtaResources[1].semTimeout 	= 0;
      sysAtaResources[1].wdgTimeout 	= 0;
#endif
      
    }

#endif /* INCLUDE_ATA */
