/* sysALib.s - wrSbc750gx system-dependent assembly routines */

/* Copyright (c) 1984-2007 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01k,13feb07,ymz fixed context synchronization WIND00086846.
01j,08jul05,foo Move C function call to after boot stack set up
01i,23mar05,foo move call to sysHwCacheInit to here for fast warm restart
01h,16sep04,kp  removed TBEN bit        
01g,09jul04,jlk Added sysToMonitor2()
01f,02jul04,kp  removed wrSbc7457 cpu specific stuff        
01e,17mar04,kp  teamF1, merged AE653 and Tornado-221 BSP files. 
01c,19feb04,vnk added l3cr access routines        
01b,17feb04,vnk added l2cr & msscr0 access routines
01a,10dec03,kp  written from  wrSbc7410 and sp7457 by teamF1
*/

/*
DESCRIPTION
This module contains the entry code, sysInit(), for VxWorks images that start
running from RAM, such as 'vxWorks'. These images are loaded into memory
by some external program (e.g., a boot ROM) and then started.
The routine sysInit() must come first in the text segment. Its job is
to perform
the minimal setup needed to call the generic C
routine usrInit() with parameter BOOT_COLD.

The routine sysInit() typically masks interrupts in the processor, sets the
initial stack pointer to _sysInit then jumps to usrInit.
Most other hardware and device initialization is performed later by
sysHwInit().
*/

/* includes */

#define _ASMLANGUAGE
#include <vxWorks.h>
#include <sysLib.h>
#include "config.h"
#include <regs.h>
#include <asm.h>

/* defines */

/*
 * Some releases of h/arch/ppc/toolPpc.h had bad definitions of
 * LOADPTR and LOADVAR. So we will define it correctly.
 * [REMOVE THESE FOR NEXT MAJOR RELEASE].
 *
 * LOADPTR initializes a register with a 32 bit constant, presumably the
 * address of something.
 */

#undef LOADPTR
#define	LOADPTR(reg,const32) \
	  addis reg,r0,HIADJ(const32); addi reg,reg,LO(const32)

/*
 * LOADVAR initializes a register with the contents of a specified memory
 * address. The difference being that the value loaded is the contents of
 * the memory location and not just the address of it.
 */

#undef LOADVAR
#define	LOADVAR(reg,addr32) \
	  addis reg,r0,HIADJ(addr32); lwz reg,LO(addr32)(reg)


/* globals */

	FUNC_EXPORT(_sysInit)		/* start of system code */
	FUNC_EXPORT(sysInByte)
	FUNC_EXPORT(sysOutByte)
        FUNC_EXPORT(sysInLong)
        FUNC_EXPORT(sysOutLong)
        FUNC_EXPORT(sysOutWord)
        FUNC_EXPORT(sysInWord)
        FUNC_EXPORT(sysPciRead32)
        FUNC_EXPORT(sysPciWrite32)
        FUNC_EXPORT(sysPciInByte)
        FUNC_EXPORT(sysPciOutByte)
        FUNC_EXPORT(sysPciInWord)
        FUNC_EXPORT(sysPciOutWord)
        FUNC_EXPORT(sysPciInLong)
        FUNC_EXPORT(sysPciOutLong)
        FUNC_EXPORT(sysMemProbeSup)
        FUNC_EXPORT(sysMssCr0Put)
        FUNC_EXPORT(sysMssCr0Get)
        FUNC_EXPORT(sysHID0Read)
        FUNC_EXPORT(sysDcbf)  
        FUNC_EXPORT(sysDcbz)
#ifdef	INCLUDE_CACHE_L2
        FUNC_EXPORT(sysL2crPut)
        FUNC_EXPORT(sysL2crGet)
        FUNC_EXPORT(sysL3crPut)
        FUNC_EXPORT(sysL3crGet)
#endif /* INCLUDE_CACHE_L2 */
        FUNC_EXPORT(sysToMonitor2)

	/* externals */

	FUNC_IMPORT(usrInit)
        FUNC_IMPORT(sysHwCacheInit)
	
	.text

/*******************************************************************************
*
* sysInit - start after boot
*
* This is the system start-up entry point for VxWorks in RAM, the
* first code executed after booting.  It disables interrupts, sets up
* the stack, and jumps to the C routine usrInit() in usrConfig.c.
*
* The initial stack is set to grow down from the address of sysInit().  This
* stack is used only by usrInit() and is never used again.  Memory for the
* stack must be accounted for when determining the system load address.
*
* NOTE: This routine should not be called by the user.
*
* RETURNS: N/A

* sysInit (void)              /@ THIS IS NOT A CALLABLE ROUTINE @/

*/

        FUNC_BEGIN(_sysInit)

	/* disable external interrupts */

	xor	p0, p0, p0
        mtmsr   p0                      /* clear the MSR register  */
        isync

        /* Zero-out registers: r0 & SPRGs */

        xor     r0,r0,r0
        mtspr   SPRG0,r0
        mtspr   SPRG1,r0
        mtspr   SPRG2,r0
        mtspr   SPRG3,r0


        /*
         *      Set MPU/MSR to a known state
         *      Turn on FP
         */

        andi.   r3, r3, 0
        ori     r3, r3, 0x2000
        sync
        mtmsr   r3
        isync
        sync
        
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

        /* Initialize the floating point data registers to a known state */

        bl      ifpdrValue
        .long   0x3f800000      /* 1.0 */
ifpdrValue:
        mfspr   r3,LR
        lfs     f0,0(r3)
        lfs     f1,0(r3)
        lfs     f2,0(r3)
        lfs     f3,0(r3)
        lfs     f4,0(r3)
        lfs     f5,0(r3)
        lfs     f6,0(r3)
        lfs     f7,0(r3)
        lfs     f8,0(r3)
        lfs     f9,0(r3)
        lfs     f10,0(r3)
        lfs     f11,0(r3)
        lfs     f12,0(r3)
        lfs     f13,0(r3)
        lfs     f14,0(r3)
        lfs     f15,0(r3)
        lfs     f16,0(r3)
        lfs     f17,0(r3)
        lfs     f18,0(r3)
        lfs     f19,0(r3)
        lfs     f20,0(r3)
        lfs     f21,0(r3)
        lfs     f22,0(r3)
        lfs     f23,0(r3)
        lfs     f24,0(r3)
        lfs     f25,0(r3)
        lfs     f26,0(r3)
        lfs     f27,0(r3)
        lfs     f28,0(r3)
        lfs     f29,0(r3)
        lfs     f30,0(r3)
        lfs     f31,0(r3)
        sync

        /*
         *      Set MPU/MSR to a known state
         *      Turn off FP
         */

        andi.   r3, r3, 0
        sync
        mtmsr   r3
        isync
        sync

        /* Init the Segment registers */

        andi.   r3, r3, 0
        isync                  /* isync is enough, no sync required now */
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


        /* insert protection from decrementer exceptions */

	xor	p0, p0, p0
	LOADPTR (p1, 0x4c000064)        /* load rfi (0x4c000064) to p1 */
        stw     p1, 0x900(r0)           /* store rfi at 0x00000900 */

	/* initialize the stack pointer */
	
	lis     sp, HI(RAM_LOW_ADRS)
	addi    sp, sp, LO(RAM_LOW_ADRS)
	
        /* Turn off data and instruction cache control bits */

        mfspr   r3, HID0
        isync
        rlwinm  r4, r3, 0, 18, 15       /* r4 has ICE and DCE bits cleared */
        sync                            /* for pre DCE change */
        isync                           /* required for pre ICE change */
        mtspr   HID0, r4                /* HID0 = r4 */
        isync                           /* immediately follow for ICE change */
        sync                            /* DCE change */
       

        /* invalidate the MPU's data/instruction caches */
        
        /* handle All different CPU specific cache etc.. */
        
	lis     r3, 0x0 
	mfspr   r3, HID0
	sync
	oris    r3,r3,0x0010            # Set DPM
        sync
	mtspr   HID0, r3
        sync                            /* L2 cache for DPM mod need sync */
	
#ifdef USER_I_CACHE_ENABLE
        mfspr  r3,HID0
        rlwinm r3,r3,0,19,17    /* clear ILOCK (bit 18) */
        mtspr  HID0, r3         /* change ILOCK in separate step with ICE */
        ori    r3,r3,(_PPC_HID0_ICFI | _PPC_HID0_ICE)
        isync                   /* required before changing ICE */
        mtspr  HID0,r3          /* set ICFI (bit 20) and ICE (bit 16) */
        isync
        sync
#endif
        
        b       cacheEnableDone


cacheEnableDone:
	/* All processor types end up here */
	
        /* disable instruction and data translations in the MMU */
	mfmsr	r3			/* get the value in msr */
					/* clear bits IR and DR */
	
	rlwinm	r4, r3, 0, _PPC_MSR_BIT_DR+1, _PPC_MSR_BIT_IR - 1

	mtmsr	r4			/* set the msr */
	isync
	sync				/* SYNC */

	/* initialize the BAT register */
        
	li	p3,0	 		/* clear p0 */
	
	isync
	mtspr	IBAT0U,p3		/* SPR 528 (IBAT0U) */
	isync

	mtspr	IBAT0L,p3		/* SPR 529 (IBAT0L) */
	isync

	mtspr	IBAT1U,p3		/* SPR 530 (IBAT1U) */
	isync

	mtspr	IBAT1L,p3		/* SPR 531 (IBAT1L) */
	isync

	mtspr	IBAT2U,p3		/* SPR 532 (IBAT2U) */
	isync

	mtspr	IBAT2L,p3		/* SPR 533 (IBAT2L) */
	isync

	mtspr	IBAT3U,p3		/* SPR 534 (IBAT3U) */
	isync

	mtspr	IBAT3L,p3		/* SPR 535 (IBAT3L) */
	isync

	mtspr	DBAT0U,p3		/* SPR 536 (DBAT0U) */
	isync

	mtspr	DBAT0L,p3		/* SPR 537 (DBAT0L) */
	isync

	mtspr	DBAT1U,p3		/* SPR 538 (DBAT1U) */
	isync

	mtspr	DBAT1L,p3		/* SPR 539 (DBAT1L) */
	isync

	mtspr	DBAT2U,p3		/* SPR 540 (DBAT2U) */
	isync

	mtspr	DBAT2L,p3		/* SPR 541 (DBAT2L) */
	isync

	mtspr	DBAT3U,p3		/* SPR 542 (DBAT3U) */
	isync

	mtspr	DBAT3L,p3		/* SPR 543 (DBAT3L) */
	isync

	/* invalidate entries within both TLBs */

CLR_TLBS:
        bl sysInvalidateTLBs

	/* initialize Small Data Area (SDA) start address */

#if	FALSE				/* XXX TPR NO SDA for now */
	lis     r2, HI(_SDA2_BASE_)
	addi    r2, r2, LO(_SDA2_BASE_)

	lis     r13, HI(_SDA_BASE_)
	addi    r13, r13, LO(_SDA_BASE_)
#endif

	addi	sp, sp, -FRAMEBASESZ	/* get frame stack */
	
        /* C code, requires stack frame */
	bl sysHwCacheInit
	
	li      r3, BOOT_WARM_AUTOBOOT
   	b	usrInit			/* never returns - starts up kernel */

       
/*******************************************************************************
* sysInvalidateTLBs - invalidate the TLB's.
*
* This routine will invalidate the TLB's.
*
* SYNOPSIS
* \ss
* void sysInvalidateTLBs
*     (
*     void
*     )
* \se
*
* RETURNS: N/A
*/

FUNC_BEGIN(sysInvalidateTLBs)
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
	bdnz   	tlbloop
        
	sync
        
        isync
        
	blr
FUNC_END(sysInvalidateTLBs)





















        
/*****************************************************************************
*
* sysInByte - reads a byte from an io address.
*
* This function reads a byte from a specified io address.
*
* RETURNS: byte from address.

* UCHAR sysInByte
*     (
*     UCHAR *  pAddr		/@ Virtual I/O addr to read from @/
*     )

*/

FUNC_BEGIN(sysInByte)
	eieio			/* Sync I/O operation */
	sync
	lbzx	p0,r0,p0	/* Read byte from I/O space */
	bclr	20,0		/* Return to caller */
FUNC_END(sysInByte)

/******************************************************************************
*
* sysOutByte - writes a byte to an io address.
*
* This function writes a byte to a specified io address.
*
* RETURNS: N/A

* VOID sysOutByte
*     (
*     UCHAR *  pAddr,		/@ Virtual I/O addr to write to @/
*     UCHAR    data		/@ data to be written @/
*     )

*/

FUNC_BEGIN(sysOutByte)
	stbx	p1,r0,p0	/* Write a byte to PCI space */
	eieio			/* Sync I/O operation */
	sync
	bclr	20,0		/* Return to caller */
FUNC_END(sysOutByte)

/*****************************************************************************
*
* sysInWord - reads a word from an address, swapping the bytes.
*
* This function reads a swapped word from a specified 
* address.
*
* RETURNS:
* Returns swapped 16 bit data from the specified address.

* USHORT sysInWord
*     (
*     ULONG  address,		/@ addr to read from @/
*     )

*/

FUNC_BEGIN(sysInWord)
	eieio			/* Sync I/O operation */
	sync
        lhbrx   p0,r0,p0	/* Read and swap */
        bclr    20,0		/* Return to caller */
FUNC_END(sysInWord)

/*****************************************************************************
*
* sysOutWord - writes a word to an address swapping the bytes.
*
* This function writes a swapped word to a specified 
* address.
*
* RETURNS: N/A

* VOID sysOutWord
*     (
*     ULONG address,		/@ Virtual addr to write to @/
*     UINT16  data		/@ Data to be written       @/
*     )

*/

FUNC_BEGIN(sysOutWord)
        sthbrx  p1,r0,p0	/* Write with swap to address */
	eieio			/* Sync I/O operation */
	sync
        bclr    20,0		/* Return to caller */
FUNC_END(sysOutWord)

/*****************************************************************************
*
* sysInLong - reads a long from an address.
*
* This function reads a long from a specified PCI Config Space (little-endian)
* address.
*
* RETURNS:
* Returns 32 bit data from the specified register.  Note that for PCI systems
* if no target responds, the data returned to the CPU will be 0xffffffff.

* ULONG sysInLong
*     (
*     ULONG  address,		/@ Virtual addr to read from @/
*     )

*/

FUNC_BEGIN(sysInLong)
	eieio			/* Sync I/O operation */
	sync
        lwbrx   p0,r0,p0	/* Read and swap from address */
        bclr    20,0		/* Return to caller */
FUNC_END(sysInLong)

/******************************************************************************
*
* sysOutLong - write a swapped long to address.
*
* This routine will store a 32-bit data item (input as big-endian)
* into an address in little-endian mode.
*
* RETURNS: N/A

* VOID sysOutLong
*     (
*     ULONG   address,		/@ Virtual addr to write to @/
*     ULONG   data		/@ Data to be written @/
*     )

*/

FUNC_BEGIN(sysOutLong)
        stwbrx  p1,r0,p0	/* store data as little-endian */
	eieio			/* Sync I/O operation */
	sync
        bclr    20,0
FUNC_END(sysOutLong)

/******************************************************************************
*
* sysPciRead32 - read 32 bit PCI data
*
* This routine will read a 32-bit data item from PCI (I/O or
* memory) space.
*
* RETURNS: N/A

* VOID sysPciRead32
*     (
*     ULONG *  pAddr,		/@ Virtual addr to read from @/
*     ULONG *  pResult		/@ location to receive data @/
*     )

*/

FUNC_BEGIN(sysPciRead32)
	eieio			/* Sync I/O operation */
        lwbrx   p0,r0,p0	/* get the data and swap the bytes */
        stw     p0,0(p1)	/* store into address ptd. to by p1 */
        bclr    20,0
FUNC_END(sysPciRead32)

        
/******************************************************************************
*
* sysPciWrite32 - write a 32 bit data item to PCI space
*
* This routine will store a 32-bit data item (input as big-endian)
* into PCI (I/O or memory) space in little-endian mode.
*
* RETURNS: N/A

* VOID sysPciWrite32
*     (
*     ULONG *  pAddr,		/@ Virtual addr to write to @/
*     ULONG   data		/@ Data to be written @/
*     )

*/

FUNC_BEGIN(sysPciWrite32)
        stwbrx  p1,r0,p0	/* store data as little-endian */
        bclr    20,0
FUNC_END(sysPciWrite32)


/*****************************************************************************
*
* sysPciInByte - reads a byte from PCI Config Space.
*
* This function reads a byte from a specified PCI Config Space address.
*
* RETURNS:
* Returns 8 bit data from the specified register.  Note that for PCI systems
* if no target responds, the data returned to the CPU will be 0xff.

* UINT8 sysPciInByte
*     (
*     UINT8 *  pAddr,		/@ Virtual addr to read from @/
*     )

*/

FUNC_BEGIN(sysPciInByte)
	eieio			/* Sync I/O operation */
        lbzx    p0,r0,p0	/* Read byte from PCI space */
        bclr    20,0		/* Return to caller */
FUNC_END(sysPciInByte)

/*****************************************************************************
*
* sysPciInWord - reads a word (16-bit big-endian) from PCI Config Space.
*
* This function reads a word from a specified PCI Config Space (little-endian)
* address.
*
* RETURNS:
* Returns 16 bit data from the specified register.  Note that for PCI systems
* if no target responds, the data returned to the CPU will be 0xffff.

* USHORT sysPciInWord
*     (
*     USHORT *  pAddr,		/@ Virtual addr to read from @/
*     )

*/

FUNC_BEGIN(sysPciInWord)
	eieio			/* Sync I/O operation */
        lhbrx   p0,r0,p0	/* Read and swap from PCI space */
        bclr    20,0		/* Return to caller */
FUNC_END(sysPciInWord)

/*****************************************************************************
*
* sysPciInLong - reads a long (32-bit big-endian) from PCI Config Space.
*
* This function reads a long from a specified PCI Config Space (little-endian)
* address.
*
* RETURNS:
* Returns 32 bit data from the specified register.  Note that for PCI systems
* if no target responds, the data returned to the CPU will be 0xffffffff.

* ULONG sysPciInLong
*     (
*     ULONG *  pAddr,		/@ Virtual addr to read from @/
*     )

*/

FUNC_BEGIN(sysPciInLong)
	eieio			/* Sync I/O operation */
        lwbrx   p0,r0,p0	/* Read and swap from PCI space */
        bclr    20,0		/* Return to caller */
FUNC_END(sysPciInLong)

/******************************************************************************
*
* sysPciOutByte - writes a byte to PCI Config Space.
*
* This function writes a byte to a specified PCI Config Space address.
*
* RETURNS: N/A

* VOID sysPciOutByte
*     (
*     UINT8 *  pAddr,		/@ Virtual addr to write to @/
*     UINT8  data		/@ Data to be written       @/
*     )

*/

FUNC_BEGIN(sysPciOutByte)
        stbx    p1,r0,p0	/* Write a byte to PCI space */
        bclr    20,0		/* Return to caller */
FUNC_END(sysPciOutByte)

/******************************************************************************
*
* sysPciOutWord - writes a word (16-bit big-endian) to PCI Config Space.
*
* This function writes a word to a specified PCI Config Space (little-endian)
* address.
*
* RETURNS: N/A

* VOID sysPciOutWord
*     (
*     USHORT *  pAddr,		/@ Virtual addr to write to @/
*     USHORT  data		/@ Data to be written       @/
*     )

*/

FUNC_BEGIN(sysPciOutWord)
        sthbrx  p1,r0,p0	/* Write with swap to PCI space */
        bclr    20,0		/* Return to caller */
FUNC_END(sysPciOutWord)

/******************************************************************************
*
* sysPciOutLong - writes a long (32-bit big-endian) to PCI Config Space.
*
* This function writes a long to a specified PCI Config Space (little-endian)
* address.
*
* RETURNS: N/A

* VOID sysPciOutLong
*     (
*     ULONG *  pAddr,		/@ Virtual addr to write to @/
*     ULONG  data		/@ Data to be written       @/
*     )

*/

FUNC_BEGIN(sysPciOutLong)
        stwbrx  p1,r0,p0	/* Write big-endian long to little-endian */
        mr      p0,p1		/* PCI space */
        bclr    20,0		/* Return to caller */
FUNC_END(sysPciOutLong)

/*******************************************************************************
*
* sysMemProbeSup - sysBusProbe support routine
*
* This routine is called to try to read byte, word, or long, as specified
* by length, from the specified source to the specified destination.
*
* RETURNS: OK if successful probe, else ERROR

* STATUS sysMemProbeSup
*     (
*     int         length, /@ length of cell to test (1, 2, 4) @/
*     char *      src,    /@ address to read @/
*     char *      dest    /@ address to write @/
*     )

*/

FUNC_BEGIN(sysMemProbeSup)
        addi    p7, p0, 0       /* save length to p7 */
        xor     p0, p0, p0      /* set return status */
        cmpwi   p7, 1           /* check for byte access */
        bne     sbpShort        /* no, go check for short word access */
        lbz     p6, 0(p1)       /* load byte from source */
        stb     p6, 0(p2)       /* store byte to destination */
        isync                   /* enforce for immediate exception handling */
        blr
sbpShort:
        cmpwi   p7, 2           /* check for short word access */
        bne     sbpWord         /* no, check for word access */
        lhz     p6, 0(p1)       /* load half word from source */
        sth     p6, 0(p2)       /* store half word to destination */
        isync                   /* enforce for immediate exception handling */
        blr
sbpWord:
        cmpwi   p7, 4           /* check for short word access */
        bne     sysProbeExc     /* no, check for double word access */
        lwz     p6, 0(p1)       /* load half word from source */
        stw     p6, 0(p2)       /* store half word to destination */
        isync                   /* enforce for immediate exception handling */
        blr
sysProbeExc:
        li      p0, -1          /* shouldn't ever get here, but... */
        blr

FUNC_END(sysMemProbeSup)

#ifndef TORNADO_AE_653
#include "sysCacheLockALib.s"
#endif /* !TORNADO_AE_653 */

#ifdef	INCLUDE_CACHE_L2

/*******************************************************************************
*
* sysL2crPut - write to L2CR register of PPC74x7 CPU
*
* SYNOPSIS
* \ss
* void sysL2crPut
*     (
*     ULONG    data     /@  value to write @/
*     )
* \se
*
* RETURNS: NA
*/

FUNC_BEGIN(sysL2crPut)
    mtspr   1017,r3
    bclr    20,0                    /* Return to caller */
FUNC_END(sysL2crPut)


/*******************************************************************************
*
* sysL2crGet - read from L2CR register of PPC74x7 CPU
*
* SYNOPSIS
* \ss
* UINT32 sysL2crGet(void)
* \se
*
* RETURNS: UINT32 value of SPR1017 (in r3)
*/

FUNC_BEGIN(sysL2crGet)
    mfspr   r3,1017
    bclr    20,0                    /* Return to caller */
FUNC_END(sysL2crGet)

/*******************************************************************************
*
* sysL3crPut - write to L3CR register of PPC74x7 CPU
*
* SYNOPSIS
* \ss
* void sysL3crPut
*     (
*     ULONG    data     /@  value to write @/
*     )
* \se
*
* RETURNS: NA
*/

FUNC_BEGIN(sysL3crPut)
    mtspr   1018,r3
    bclr    20,0                    /* Return to caller */
FUNC_END(sysL3crPut)


/*******************************************************************************
*
* sysL3crGet - read from L3CR register of PPC74x7 CPU
*
* SYNOPSIS
* \ss
* UINT32 sysL3crGet(void)
* \se
*
* RETURNS: UINT32 value of SPR1018 (in r3)
*/

FUNC_BEGIN(sysL3crGet)
    mfspr   r3,1018
    bclr    20,0                    /* Return to caller */
FUNC_END(sysL3crGet)
#endif /* INCLUDE_CACHE_L2 */

/*******************************************************************************
*
* sysMssCr0Put - write to  MSSCR0 register of PPC74x7 CPU
*
* SYNOPSIS
* \ss
* void sysMssCr0Put
*     (
*     ULONG    data     /@  value to write @/
*     )
* \se
*
* RETURNS: NA
*/

FUNC_BEGIN(sysMssCr0Put)
    sync
    mtspr   1014,r3
    sync
    isync
    bclr    20,0                    /* Return to caller */
FUNC_END(sysMssCr0Put)


/*******************************************************************************
*
* sysMssCr0Get - read from MSSCR0 register of PPC74x7 CPU
*
* SYNOPSIS
* \ss
* UINT32 sysMssCr0Get(void)
* \se
*
* RETURNS: UINT32 value of SPR1014 (in r3)
*/

FUNC_BEGIN(sysMssCr0Get)
    mfspr   r3,1014
    bclr    20,0                    /* Return to caller */
FUNC_END(sysMssCr0Get)

/*******************************************************************************
*
* sysHID0Read - read from HID0 register of PPC74x7 CPU
*
* SYNOPSIS
* \ss
* UINT32 sysHID0Read(void)
* \se
*
* RETURNS: UINT32 value of SPR1008 (in r3)
*/

FUNC_BEGIN(sysHID0Read)
    sync
    mfspr   r3,1008
    sync    
    bclr    20,0                    /* Return to caller */
FUNC_END(sysHID0Read)
                


/***************************************************************************
* sysDcbz - establishes one cache line of zeros using the dcbz instruction
*
* This routine uses the dcbz instruction to establish one cache line of zeros
* at the specified address.
*
* SYNOPSIS
* \ss
* void sysDcbz
*     (
*     void * address
*     )
* \se
*
* RETURNS: N/A
*/

FUNC_BEGIN(sysDcbz)
        dcbz    r0,r3
        blr
FUNC_END(sysDcbz)

/***************************************************************************
* sysDcbf - flushes one cache line of using the dcbf instruction
*
* This routine uses the dcbf instruction to flush one cache line at the
* specified address.
*
* SYNOPSIS
* \ss
* void sysDcbf
*     (
*     void * address
*     )
* \se
*
* RETURNS: N/A
*/

FUNC_BEGIN(sysDcbf)
        dcbf    r0,r3
        blr
FUNC_END(sysDcbf)

/*****************************************************************************
*
* sysToMonitor2 - completes the transfer to the system monitor
*
* This routine finishes the transfer of control to the ROM monitor.  It is called
* only by sysToMonitor(). It is necessary that this function never access the stack
* as the MMU is turned off. The stack pointer contains a virtual address so if there
* isn't a one-to-one mapping the memory it points to will become invalid once the MMU
* is off.
* <startType> to enable special boot ROM facilities.
*
* The entry point for a warm boot is defined by the macro ROM_WARM_ADRS
* in config.h.  We do an absolute jump to this address to enter the
* ROM code.
*
* RETURNS: Does not return.

* VOID sysToMonitor2
*     (
*     int     startType, /@ parameter passed to ROM to tell it how to boot (p0)  @/
*     UINT32  pRom       /@ Address in ROM to transfer control to          (p1)  @/
*     )

*/

FUNC_BEGIN(sysToMonitor2)
    /* Load LR with address to jump to */
    mtlr    p1

    /* Clear MSR - This will turn off both data and instruction translations */
    xor     p2,p2,p2
    sync
    mtmsr   p2

    /* 
     * The following two instructions are necessary (according to the PPC manual)
     * when turning off the MMU
     */
    sync
    isync

    /* Jump to ROM */
    blr

    /* Should never get here */
loopForever:
    b       loopForever
FUNC_END(sysToMonitor2)        
