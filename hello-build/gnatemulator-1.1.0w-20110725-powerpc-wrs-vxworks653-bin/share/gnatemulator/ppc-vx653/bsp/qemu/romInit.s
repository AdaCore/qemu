/* romInit.s - Wind River wrSbc7457 ROM initialization module */

/* Copyright 2003-2006 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01h,06jul06,ymz  removed compiler warning (CQ #WIND00026807).
01g,07jul05,mat  changed WR_SBC_7457_BASE_ADRS to WR_SBC_750GX_BASE_ADRS
01f,24may05,dsb  changed romMV64360Config() to configure boot chip select 
	         to enable the entire 64MB of FLASH.
01e,30sep04,mat  changed romMV64360Config() to configure the DFCDL
01d,16sep04,kp   removed TBEN bit        
01c,17mar04,kp   teamF1, merged AE and Tornado-221 files and Guarded AE 
                 specific stuff under the macros #ifndef TORNADO_AE_653...#endif
01b,18feb03,kp   added Extended ram initialization code from vWare boardinit.s
01a,26nov03,kp   derived from SP7457 & vWare (bordinit.s) by teamF1.
*/

/*

DESCRIPTION
This module contains the entry code for VxWorks images that start
running from ROM, such as 'bootrom' and 'vxWorks_rom'.
The entry point, romInit(), is the first code executed on power-up.
It performs the minimal setup needed to call
the generic C routine romStart() with parameter BOOT_COLD.

RomInit() typically masks interrupts in the processor, sets the initial
stack pointer (to STACK_ADRS which is defined in configAll.h), and
readies system memory by configuring the DRAM controller if necessary.
Other hardware and device initialization is performed later in the
BSP's sysHwInit() routine.

A second entry point in romInit.s is called romInitWarm(). It is called
by sysToMonitor() in sysLib.c to perform a warm boot.
The warm-start entry point must be written to allow a parameter on
the stack to be passed to romStart().

WARNING:
This code must be Position Independent Code (PIC).  This means that it
should not contain any absolute address references.  If an absolute address
must be used, it must be relocated by the macro ROM_ADRS(x).  This macro
will convert the absolute reference to the appropriate address within
ROM space no matter how the boot code was linked. (For PPC, ROM_ADRS does
not work.  You must subtract _romInit and add ROM_TEXT_ADRS to each
absolute address). (NOTE: ROM_ADRS(x) macro does not work for current
PPC compiler).

This code should not call out to subroutines declared in other modules,
specifically sysLib.o, and sysALib.o.  If an outside module is absolutely
necessary, it can be linked into the system by adding the module 
to the makefile variable BOOT_EXTRA.  If the same module is referenced by
other BSP code, then that module must be added to MACH_EXTRA as well.
Note that some C compilers can generate code with absolute addresses.
Such code should not be called from this module.  If absolute addresses
cannot be avoided, then only ROM resident code can be generated from this
module.  Compressed and uncompressed bootroms or VxWorks images will not
work if absolute addresses are not processed by the macro ROM_ADRS.

WARNING:
The most common mistake in BSP development is to attempt to do too much
in romInit.s.  This is not the main hardware initialization routine.
Only do enough device initialization to get memory functioning.  All other
device setup should be done in sysLib.c, as part of sysHwInit().

Unlike other RTOS systems, VxWorks does not use a single linear device
initialization phase.  It is common for inexperienced BSP writers to take
a BSP from another RTOS, extract the assembly language hardware setup
code and try to paste it into this file.  Because VxWorks provides 3
different memory configurations, compressed, uncompressed, and rom-resident,
this strategy will usually not work successfully.

WARNING:
The second most common mistake made by BSP writers is to assume that
hardware or CPU setup functions done by romInit.o do not need to be
repeated in sysALib.s or sysLib.c.  A vxWorks image needs only the following
from a boot program: The startType code, and the boot parameters string
in memory.  Each VxWorks image will completely reset the CPU and all
hardware upon startup.  The image should not rely on the boot program to
initialize any part of the system (it may assume that the memory controller
is initialized).

This means that all initialization done by romInit.s must be repeated in
either sysALib.s or sysLib.c.  The only exception here could be the
memory controller.  However, in most cases even that can be
reinitialized without harm.

Failure to follow this rule may require users to rebuild bootrom's for
minor changes in configuration.  It is WRS policy that bootroms and vxWorks
images should not be linked in this manner.
*/

#define	_ASMLANGUAGE
#include <vxWorks.h>
#include <sysLib.h>
#include <asm.h>
#include "config.h"
#include <regs.h>


        /* globals */
        
#ifdef TORNADO_AE_653
      /* AE specific */             
	.data

#endif /* TORNADO_AE_653 */        
	
        FUNC_EXPORT(_romInit)     /* start of system code */
	FUNC_EXPORT(romInit)      /* start of system code */
	FUNC_EXPORT(_romInitWarm) /* start of system code */
	FUNC_EXPORT(romInitWarm)  /* start of system code */

        /* externals */
	FUNC_IMPORT(romStart)     /* system initialization routine */

#ifdef TORNADO_AE_653
        
      /* AE specific */
          
        .text
	.align 2
        
#else /* TORNADO_AE_653 */

      /* for tornado 2.x */
          
       _WRS_TEXT_SEG_START
        
#endif

/*******************************************************************************
* romInit - entry point for VxWorks in ROM
*
* SYNOPSIS
* \ss
* romInit
*     (
*     int startType     /@ only used by 2nd entry point @/
*     )
* \se
*/
 
FUNC_LABEL(_romInit)
FUNC_BEGIN(romInit)

	/* This is the cold boot entry (ROM_TEXT_ADRS)
         */

	/*
	 * According to a comment in the DINK32 startup code, a 7400
	 * erratum requires an isync to be the first instruction executed
	 * here.  The isync plus the branch amount to 8 bytes, so omit the
	 * nop which would otherwise follow the branch.
	 */
        
	isync
	bl	cold

	/*
	 * This warm boot entry point is defined as ROM_WARM_ADRS in config.h.
	 * It is defined as an offset from ROM_TEXT_ADRS.  Please make sure
	 * that the offset from _romInit to romInitWarm matches that specified
	 * in config.h.  (WRS is standardizing this offset to 8 bytes.)
	 */

        
_romInitWarm:
romInitWarm:

	/*
	 * See above comment regarding a 7400 erratum.  Since DINK32
	 * uses a common entry point for both cold and warm boot, it's
	 * not clear which one requires the isync.  isync is harmless,
	 * so do it both places to be safe.
	 */
        
	isync
	bl	warm

	/*
         * copyright notice appears at beginning of ROM (in TEXT segment)
	 * This is also useful for debugging images.
         */

	.ascii   "Copyright 1984-2004 Wind River Systems, Inc."
	.align 2
        
cold:
	li	r11, BOOT_COLD
	bl	start		/* skip over next instruction */

warm:
	or	r11, r3, r3	/* startType to r11 */

start:
	/*
	 * DINK32 does a sync after the isync.  It may or may not
	 * be necessary, but should at least be harmless.
	 */
	sync

	/* Zero-out the CPU's registers */
        xor	r0,r0,0		/* DINK32 uses xor instead of addis */
	mtspr   SPRG0,r0
	mtspr   SPRG1,r0
	mtspr   SPRG2,r0
	mtspr   SPRG3,r0

        /* initialize the stack pointer */

	LOADPTR (sp, STACK_ADRS)

	/* Set MPU/MSR to a known state. Turn on FP */

	LOADPTR (r3, _PPC_MSR_FP)
	sync
	mtmsr 	r3
	isync
	sync		/* DINK32 does both isync and sync after mtmsr */

        /* Init the floating point control/status register */
	mtfsfi  7,0x0
	mtfsfi  6,0x0
	mtfsfi  5,0x0
	mtfsfi  4,0x0
	mtfsfi  3,0x0
	mtfsfi  2,0x0
	mtfsfi  1,0x0
	mtfsfi  0,0x0
	isync

        /* Initialize the floating point data regsiters to a known state */

	bl	ifpdr_value
	.long	0x3f800000	/* 1.0 */
        
        ifpdr_value:
	mfspr	r3,LR
	lfs	f0,0(r3)
	lfs	f1,0(r3)
	lfs	f2,0(r3)
	lfs	f3,0(r3)
	lfs	f4,0(r3)
	lfs	f5,0(r3)
	lfs	f6,0(r3)
	lfs	f7,0(r3)
	lfs	f8,0(r3)
	lfs	f9,0(r3)
	lfs	f10,0(r3)
	lfs	f11,0(r3)
	lfs	f12,0(r3)
	lfs	f13,0(r3)
	lfs	f14,0(r3)
	lfs	f15,0(r3)
	lfs	f16,0(r3)
	lfs	f17,0(r3)
	lfs	f18,0(r3)
	lfs	f19,0(r3)
	lfs	f20,0(r3)
	lfs	f21,0(r3)
	lfs	f22,0(r3)
	lfs	f23,0(r3)
	lfs	f24,0(r3)
	lfs	f25,0(r3)
	lfs	f26,0(r3)
	lfs	f27,0(r3)
	lfs	f28,0(r3)
	lfs	f29,0(r3)
	lfs	f30,0(r3)
	lfs	f31,0(r3)
	sync

        
	/* Set MPU/MSR to a known state. Turn off FP */

	andi.	r3, r3, 0
	sync
	mtmsr 	r3
	isync
	sync		/* DINK32 does both isync and sync after mtmsr */

        /* Init the Segment registers */

	andi.	r3, r3, 0
	isync
	mtsr    0,r3
	isync
	mtsr    1,r3
	isync
	mtsr    2,r3
	isync
	mtsr    3,r3
	isync
	mtsr    4,r3
	isync
	mtsr    5,r3
	isync
	mtsr    6,r3
	isync
	mtsr    7,r3
	isync
	mtsr    8,r3
	isync
	mtsr    9,r3
	isync
	mtsr    10,r3
	isync
	mtsr    11,r3
	isync
	mtsr    12,r3
	isync
	mtsr    13,r3
	isync
	mtsr    14,r3
	isync
	mtsr    15,r3
	isync

        /* Turn off data and instruction cache control bits */

	mfspr   r3, HID0
	isync
	rlwinm	r4, r3, 0, 18, 15	/* r4 has ICE and DCE bits cleared */
	sync
	isync
	mtspr	HID0, r4		/* HID0 = r4 */
	isync

        
       /* invalidate the MPU's data/instruction caches */ 
          
	mfspr   r3, HID0
        sync
        oris    r3,r3,0x0010            # Set DPM
        sync
        mtspr   HID0, r3
        sync

        mfspr   r3,HID0

        addis   r4,r0,0x0000    /* Clear r4 */
        ori     r4,r4,0x8800    /* Setup bit pattern for ICE/ICFI */
        or      r3,r4,r3
        isync
        mtspr   HID0,r3         /* set ICE/ICFI */
        isync

        addis   r4,r0,0x0000    /* Clear r4 */
        ori     r4,r4,0x0800    /* Setup bit pattern for ICFI */
        andc    r3,r3,r4
        isync
        mtspr   HID0,r3         /* clear IFCI (bit 16) */
        isync

        addis   r4,r0,0x0000    /* Clear r4 */
        ori     r4,r4,0x2000    /* Setup bit pattern for ILOCK */
        andc    r3,r3,r4
        isync
        mtspr   HID0,r3         /* clear ILOCK (bit 18) */
        isync
        sync
        
CLR_BATS:
        bl romClearBats
                 
CLR_TLBS:
        bl romInvalidateTLBs
        
        /* END of processor initialization */
        
START_C_SETUP:         

        /* go to C entry point */

	or	r3, r11, r11		/* put startType in r3 (p0) */
	addi	sp, sp, -FRAMEBASESZ	/* save one frame stack */

	LOADPTR (r6, romStart)
	LOADPTR (r7, romInit)
	LOADPTR (r8, ROM_TEXT_ADRS)

	sub	r6, r6, r7
	add	r6, r6, r8

	mtlr	r6		/* romStart - romInit + ROM_TEXT_ADRS */
	blr
        
FUNC_END(romInit)
        
        
/*******************************************************************************
* romClearBats - clears BAT registers.
*
* This routine clears data/instruction BAT registers.
*
* SYNOPSIS
* \ss
* void romClearBats
*     (
*     void
*     )
* \se
*
* RETURNS: N/A
*/

FUNC_BEGIN(romClearBats)

        li	r4,0
	mtibatu	0,r4
	mtibatl	0,r4
	mtibatu	1,r4
	mtibatl	1,r4
	mtibatu	2,r4
	mtibatl	2,r4
	mtibatu	3,r4
	mtibatl	3,r4

	/* Data BAT */
	mtdbatu	0,r4
	mtdbatl	0,r4
	mtdbatu	1,r4
	mtdbatl	1,r4
	mtdbatu	2,r4
	mtdbatl	2,r4
	mtdbatu	3,r4
	mtdbatl	3,r4
	isync

	mtspr	560,r4		# clear IBAT4U
	mtspr	561,r4		# clear IBAT4L
	mtspr	562,r4		# clear IBAT5U
	mtspr	563,r4		# clear IBAT5L
	mtspr	564,r4		# clear IBAT6U
	mtspr	565,r4		# clear IBAT6L
	mtspr	566,r4		# clear IBAT7U
	mtspr	567,r4		# clear IBAT7L

	mtspr	568,r4		# clear DBAT4U
	mtspr	569,r4		# clear DBAT4L
	mtspr	570,r4		# clear DBAT5U
	mtspr	571,r4		# clear DBAT5L
	mtspr	572,r4		# clear DBAT6U
	mtspr	573,r4		# clear DBAT6L
	mtspr	574,r4		# clear DBAT7U
	mtspr	575,r4		# clear DBAT7L
        
        isync

        blr
FUNC_END(romClearBats)
        
        
/*******************************************************************************
* romInvalidateTLBs - invalidate the TLB's.
*
* This routine will invalidate the TLB's.
*
* SYNOPSIS
* \ss
* void romInvalidateTLBs
*     (
*     void
*     )
* \se
*
* SEE ALSO: romClearBATs(), romMinimumBATsInit()
*
* RETURNS: N/A
*/

FUNC_BEGIN(romInvalidateTLBs)
        /* invalidate entries within both TLBs */
	
inval_tlbs:			/* invalidate entries within both TLBs */
	li     	r3,128
	xor    	r4,r4,r4
	mtctr	r3
	isync			/* context sync req'd before tlbie */
tlbloop:
	tlbie	r4
	sync			/* sync instr req'd after tlbie      */
	addi   	r4,r4,0x1000	/* increment bits 15-19 */
	bdnz   	tlbloop         /* decrement CTR, branch if CTR != 0 */

        sync
        
        isync
        
	blr
FUNC_END(romInvalidateTLBs)      
