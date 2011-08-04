/* sysLib.c - system-dependent library for wrSbc750gx */

/* Copyright (c) 1984-2007 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01u,08jun07,mat  Cleaned up a warning
01t,28may07,sfs  removed dependency on INCLUDE_NETWORK
01s,15may07,dan  Modified the code in function "sysUsDelay". Replaced
                 a hard coded constant with "DEC_CLOCK_FREQ" (WIND00082177) 
01r,10apr07,hya  save & restore TBR in sysMsDelay (WIND00069592 fix).
01q,26jul05,rlp  Added stdio.h header file.
01q,06jul06,ymz  removed compiler warnings (CQ #WIND00026807).
01p,18jul05,foo  Mark the eeprom BATs non cachable
01o,23mar05,foo  Updates for fast warm restart, use BATs, modify cache functions
01o,29jun05,f_l  merged ACE support from psc2.0
01n,28apr05,foo  Write protect flash (SPR #108102)
01m,25mar05,vhe  Fix SPR 105934, typo in user message
01l,29jul04,kam  changed references to wrSbc750GX to wrSbc750gx
01k,09jul04,jlk  split sysToMonitor() and added HM event injection
01j,30jun04,sfs  removed calls to abs()
01i,28jun04,sfs  added #include "mv643xxEnd.c"
01h,02jul04,kp   modified for wrsbc750GX
01g,04jun04,kp   added description for the macro INTEL_FLASH_BOOT_LOCK_DEFAULT
01f,26may04,kp   made call to sysFlashInit() routine.
01e,17feb04,vnk  added L2 cache support.
01d,28jan04,vin  teamF1, fixed delay problem in  sysNvWrite().
01c,23jan04,ksn  teamF1, added NVRAM read,write routines.
01b,10dec03,kp   teamF1, added sysMV64360Init() routine.
01a,02dec03,kp   teamF1, adopted from templatePpc/sysLib.c.
*/

/*
DESCRIPTION
This library provides board-specific routines.  The chip drivers included are:

    ppcDecTimer.c           - Decrementer Timer
    
INCLUDE FILES: sysLib.h

SEE ALSO:
.pG "Configuration"
*/

/* includes */

#include <vxWorks.h>
#include <stdio.h>
#include <memLib.h>
#include <cacheLib.h>
#include <sysLib.h>
#include "config.h"
#include <string.h>
#include <intLib.h>
#include <logLib.h>
#include <taskLib.h>
#include <tickLib.h>
#include <stdio.h>
#include <vxLib.h>
#include <tyLib.h>
#include <vmLib.h>
#include <fioLib.h>
#include <arch/ppc/mmu603Lib.h>
#include <arch/ppc/vxPpcLib.h>

#include "wrSbc750gx.h"

/* defines */

#define ZERO	0

#if defined(SBC750GX)
#define SYS_MODEL	"wrSbc750gx IBM Power PC"
#else
#error Incorrect model number, model should be wrSbc750gx IBM Power PC.        
#endif

/* globals */

/*
 * sysBatDesc[] is used to initialize the block address translation (BAT)
 * registers within the PowerPC 603/604 MMU.  BAT hits take precedence
 * over Page Table Entry (PTE) hits and are faster.  Overlap of memory
 * coverage by BATs and PTEs is permitted in cases where either the IBATs
 * or the DBATs do not provide the necessary mapping (PTEs apply to both
 * instruction AND data space, without distinction).
 *
 * The primary means of memory control for VxWorks is the MMU PTE support
 * provided by vmLib and cacheLib.  Use of BAT registers will conflict
 * with vmLib support.  User's may use BAT registers for i/o mapping and
 * other purposes but are cautioned that conflicts with cacheing and mapping
 * through vmLib may arise.  Be aware that memory spaces mapped through a BAT
 * are not mapped by a PTE and any vmLib() or cacheLib() operations on such
 * areas will not be effective, nor will they report any error conditions.
 *
 * Note: BAT registers CANNOT be disabled - they are always active.
 * For example, setting them all to zero will yield four identical data
 * and instruction memory spaces starting at local address zero, each 128KB
 * in size, and each set as write-back and cache-enabled.  Hence, the BAT regs
 * MUST be configured carefully.
 *
 * With this in mind, it is recommended that the BAT registers be used
 * to map LARGE memory areas external to the processor if possible.
 * If not possible, map sections of high RAM and/or PROM space where
 * fine grained control of memory access is not needed.  This has the
 * beneficial effects of reducing PTE table size (8 bytes per 4k page)
 * and increasing the speed of access to the largest possible memory space.
 * Use the PTE table only for memory which needs fine grained (4KB pages)
 * control or which is too small to be mapped by the BAT regs.
 *
 * The BAT configuration for 4xx/6xx-based PPC boards is as follows:
 * All BATs point to PROM/FLASH memory so that end customer may configure
 * them as required.
 *
 * [Ref: chapter 7, PowerPC Microprocessor Family: The Programming Environments]
 */

UINT32 sysBatDesc [2 * (_MMU_NUM_IBAT + _MMU_NUM_DBAT)] =
    {
    /* I BAT 0 */
    ((ROM_BASE_ADRS & _MMU_UBAT_BEPI_MASK) | _MMU_UBAT_BL_1M |
    _MMU_UBAT_VS),
    ((ROM_BASE_ADRS & _MMU_LBAT_BRPN_MASK) | _MMU_LBAT_PP_RW |
    _MMU_LBAT_CACHE_INHIBIT),

    /* I BAT 1 */

    0, 0,

    /* I BAT 2 */

    0, 0,

    /* I BAT 3 */

    0, 0,

    /* D BAT 0 */
    (FLASH_BASE_ADRS | _MMU_UBAT_BL_16M | _MMU_UBAT_VS),
    (FLASH_BASE_ADRS | _MMU_LBAT_PP_RW | _MMU_LBAT_MEM_COHERENT),

    /* D BAT 1 */
    (PCI_MSTR_ISA_IO_LOCAL | _MMU_UBAT_BL_128K | _MMU_UBAT_VS),
    (PCI_MSTR_ISA_IO_LOCAL | _MMU_LBAT_PP_RW | _MMU_LBAT_CACHE_INHIBIT | 
     _MMU_LBAT_GUARDED | _MMU_LBAT_MEM_COHERENT),

    /* D BAT 2 */
    ((PCI_MSTR_IACK_LOCAL & _MMU_UBAT_BEPI_MASK) | _MMU_UBAT_BL_128K | _MMU_UBAT_VS),
    ((PCI_MSTR_IACK_LOCAL & _MMU_LBAT_BRPN_MASK) | _MMU_LBAT_PP_RW | _MMU_LBAT_CACHE_INHIBIT | 
     _MMU_LBAT_GUARDED | _MMU_LBAT_MEM_COHERENT),

    /* D BAT 3 */
    0, 0
    };


/*
 * sysPhysRgnTbl[] is used to initialize the Page Table Entry (PTE) array
 * used by the MMU to translate addresses with single page (4k) granularity.
 * PTE memory space should not, in general, overlap BAT memory space but
 * may be allowed if only Data or Instruction access is mapped via BAT.
 *
 * Address translations for local RAM, memory mapped PCI bus, memory mapped
 * VME A16 space and local PROM/FLASH are set here.
 *
 * PTEs are held, strangely enough, in a Page Table.  Page Table sizes are
 * integer powers of two based on amount of memory to be mapped and a
 * minimum size of 64 kbytes.  The MINIMUM recommended Page Table sizes
 * for 32-bit PowerPCs are:
 *
 * Total mapped memory		Page Table size
 * -------------------		---------------
 *        8 Meg			     64 K
 *       16 Meg			    128 K
 *       32 Meg			    256 K
 *       64 Meg			    512 K
 *      128 Meg			      1 Meg
 * 	.				.
 * 	.				.
 * 	.				.
 *
 * [Ref: chapter 7, PowerPC Microprocessor Family: The Programming Environments]
 */

/* Default Pagesize, and Valid PageSize List */

UINT    vmPgSizeDefault = MMU_PAGE_SIZE;
UINT    vmPgSizeTbl[] = {MMU_PAGE_SIZE, -1};

int   sysBus      = VME_BUS;            /* system bus type */
int   sysCpu      = CPU;                /* system CPU type (MC680x0) */
char *sysBootLine = BOOT_LINE_ADRS;	/* address of boot line */
char *sysExcMsg   = EXC_MSG_ADRS;	/* catastrophic message area */
int   sysProcNum  = 0;			/* processor number of this CPU */
int   sysFlags;				/* boot flags */
char  sysBootHost [BOOT_FIELD_LEN];	/* name of host from which we booted */
char  sysBootFile [BOOT_FIELD_LEN];	/* name of file from which we booted */

/* locals */

LOCAL char sysModelStr[80] = SYS_MODEL;
LOCAL char wrongCpuMsg[] = "WRONG CPU"; 

UINT  sysVectorIRQ0 = INT_VEC_IRQ0;

/* forward declarations */

void	sysSpuriousIntHandler(void);
void	sysCpuCheck (void);
char *  sysPhysMemTop (void);
int	sysGetBusSpd (void);
void    sysHwCacheInit (void);
void    sysHwCacheFuncsInit (void);
void    sysHwRequiringMmuInit (void);

/* externals */

IMPORT void   sysOutByte (ULONG, UCHAR);
IMPORT UCHAR  sysInByte (ULONG);

IMPORT VOIDFUNCPTR _pSysL2CacheInvFunc;
IMPORT VOIDFUNCPTR _pSysL2CacheEnable;
IMPORT VOIDFUNCPTR _pSysL2CacheDisable;
IMPORT VOIDFUNCPTR _pSysL2CacheFlush;

IMPORT VOIDFUNCPTR _pSysL3CacheInvFunc;
IMPORT VOIDFUNCPTR _pSysL3CacheEnable;
IMPORT VOIDFUNCPTR _pSysL3CacheDisable;
IMPORT VOIDFUNCPTR _pSysL3CacheFlush;

IMPORT VOIDFUNCPTR _pSysHwRequiringMmuInit;

IMPORT BOOL snoopEnable;
IMPORT ULONG sysPVRReadSys(void);

IMPORT void sysToMonitor2(int, UINT32);

extern void sysHwCacheFuncsInit (void);

void qDebug (const char *str, ...);

/* BSP DRIVERS */
#ifdef INCLUDE_SERIAL          /* include serial */
#include "sysSerial.c"
#endif /* INCLUDE_SERIAL */

#ifdef	INCLUDE_ATA
#  include "sysAta.c"			/* sysAtaInit() routine */
#endif	/* INCLUDE_ATA */

#include "sysCache.c"
#ifdef INCLUDE_CACHE_LOCK
#include "sysCacheLockLib.c"	/* Cache Lock/unlock library */
#endif /* INCLUDE_CACHE_LOCK */

#include "w83553PciIbc.c"

#ifdef  INCLUDE_END
#include "sysNet.c"

#        ifdef INCLUDE_NE2000END
#define INT_NUM_GET(x) (x)
#            include "sysNe2000End.c"
#        endif /* INCLUDE_NE2000END */

#endif /* INCLUDE_END */

#ifdef INCLUDE_AUXCLK
#include "i8254AuxClk.c"
#endif

#ifdef PRJ_BUILD
#if defined(AE653_BUILD) && defined(BOOT_APP_BUILD)
#   include "prjMemConfig.c"
#endif /*#if defined(AE653_BUILD) && !defined(BOOT_APP_BUILD) */
#endif /* PRJ_BUILD */


static STATUS
qOutBuf (char *buf, int nchars, int outarg)
{
  while (nchars > 0)
    {
      if (*buf == '\n')
	*(volatile unsigned char *)0x80000f01 = 0;
      else
	*(volatile unsigned char *)0x80000f00 = *buf;
      buf++;
      nchars--;
    }
  return 0;
}

void
qDebug (const char *str, ...)
{
  va_list args;
  va_start (args, str);
  fioFormatV (str, args, qOutBuf, 1);
  va_end (args);
  /* *(volatile unsigned char *)0x80000f01 = 0;*/
}

/*******************************************************************************
* sysModel - return the model name of the CPU board
*
* This routine returns the model name of the CPU board.  The returned string
* depends on the board model and CPU version being used, for example,
* "wrSbc750gx IBM Power PC".
*
* RETURNS: A pointer to the string.
*/

char * sysModel (void)
    {
    return (sysModelStr);
    }


/*******************************************************************************
*
* sysBspRev - return the BSP version and revision number
*
* This routine returns a pointer to a BSP version and revision number, for
* example, 1.1/0. BSP_REV is concatenated to BSP_VERSION and returned.
*
* RETURNS: A pointer to the BSP version/revision string.
*/

char * sysBspRev (void)
    {
    return (BSP_VERSION BSP_REV);
    }

/****************************************************************************** *
 * sysHwMemInit - initialize and configure system memory.
 *
 * This routine is called before sysHwInit(). It performs memory auto-sizing 
 * and updates the system's physical regions table, `sysPhysRgnTbl'. It may  
 * include the code to do runtime configuration of extra memory controllers.
 *
 * NOTE: This routine should not be called directly by the user application. It
 * cannot be used to initialize interrupt vectors.
 *
 * RETURNS: N/A
 */

void sysHwMemInit (void)
    {
    /* Call sysPhysMemTop() to do memory autosizing if available */
    sysPhysMemTop ();
    }

/*******************************************************************************
*
* sysHwInit - initialize the system hardware
*
* This routine initializes various features of the board.
* It is the first board-specific C code executed, and runs with
* interrupts masked in the processor.
* This routine resets all devices to a quiescent state, typically
* by calling driver initialization routines.
*
* NOTE: Because this routine will be called from sysToMonitor, it must
* shutdown all potential DMA master devices.  If a device is doing DMA
* at reboot time, the DMA could interfere with the reboot. For devices
* on non-local busses, this is easy if the bus reset or sysFail line can
* be asserted.
*
* NOTE: This routine should not be called directly by the user application.
*
* RETURNS: N/A
*/

void sysHwInit (void)
    {
    /* Validate CPU type */

      qDebug("sysHwInit - 0\n");

    sysCpuCheck();
   
    /* interrupt controller initialization */    
    sysPicInit ();

    sysHwCacheFuncsInit ();

      qDebug("sysHwInit - 4\n");
    }

/*******************************************************************************
*
* sysHwCacheInit - initialize the L2, L3 caches and HID0
*
*/

void sysHwCacheInit(void)
    {
#ifdef INCLUDE_CACHE_L2
    sysL2CacheInit ();
#endif
    }


/*******************************************************************************
*
* sysHwCacheFuncsInit - initialize the cache function pointers
*
* This routine initializes cache function pointers
*
*/

void sysHwCacheFuncsInit(void)
{
#ifdef  INCLUDE_CACHE_L2
    _pSysL2CacheInvFunc = sysL2CacheInv;
    _pSysL2CacheDisable = sysL2CacheDisable;
    _pSysL2CacheFlush   = sysL2CacheFlush;
    _pSysL2CacheEnable  = sysL2CacheEnable;
#ifdef SNOOP_ENABLE
    snoopEnable = TRUE;
#else
    snoopEnable = FALSE;
#endif
   
#endif /* defined (INCLUDE_CACHE_L2) */

    _pSysHwRequiringMmuInit = sysHwRequiringMmuInit;

}

/*******************************************************************************
*
* sysHwRequiringMmuInit - initialize the system hardware after MMU enabled
*
* This routine initializes various features of the board.
* that may require the MMU to be initialized first.
*
*/

void sysHwRequiringMmuInit (void)
    {
#ifdef INCLUDE_END
      /*sysNetHwInit ();*/	/* network interface */
#endif

#ifdef INCLUDE_SERIAL
	sysSerialHwInit ();	/* connect serial interrupts */
#endif
    }

/*******************************************************************************
* sysHwInit2 - initialize additional system hardware
*
* This routine connects system interrupt vectors and configures any
* required features not configured by sysHwInit. It is called from usrRoot()
* in usrConfig.c after the multitasking kernel has started.
*
* RETURNS: N/A
*/

void sysHwInit2 (void)
    {
    static int	initialized;	/* must protect against double call! */

    qDebug("sysHwInit2 - 0\n");

    if (!initialized)
	{
	initialized = TRUE;
	
	/* initialize the PIC interrupts */
        sysPicIntrInit ();

#ifdef INCLUDE_END
	/* sysNetHwInit2 ();	*//* network interface */
#endif

#ifdef INCLUDE_SERIAL
	sysSerialHwInit2 ();	/* connect serial interrupts */
#endif

#ifdef INCLUDE_ATA
	/* Winbond specific setup for the ATA controller */

	sysAtaInit(0);	/* device zero used first time through */

#endif /* INCLUDE_ATA */

	}
    qDebug("sysHwInit2 - 4\n");
    }


/*******************************************************************************
* sysPhysMemTop - get the address of the top of physical memory
*
* This routine returns the address of the first missing byte of memory,
* which indicates the top of memory.
*
* Normally, the user specifies the amount of physical memory with the
* macro LOCAL_MEM_SIZE in config.h.  BSPs that support run-time
* memory sizing do so only if the macro LOCAL_MEM_AUTOSIZE is defined.
* If not defined, then LOCAL_MEM_SIZE is assumed to be, and must be, the
* true size of physical memory.
*
* NOTE: Do no adjust LOCAL_MEM_SIZE to reserve memory for application
* use.  See sysMemTop() for more information on reserving memory.
*
* RETURNS: The address of the top of physical memory.
*
* SEE ALSO: sysMemTop()
*/

char * sysPhysMemTop (void)
    {
    static char * physTop = NULL;

    if (physTop == NULL)
	{
#ifdef LOCAL_MEM_AUTOSIZE

#	error	"Dynamic memory sizing not supported"

#else
	/* Don't do autosizing, if size is given */

	physTop = (char *)(LOCAL_MEM_LOCAL_ADRS + LOCAL_MEM_SIZE);

#endif /* LOCAL_MEM_AUTOSIZE */
	}

    return physTop;
    }


/*******************************************************************************
* sysMemTop - get the address of the top of VxWorks memory
*
* This routine returns a pointer to the first byte of memory not
* controlled or used by VxWorks.
*
* The user can reserve memory space by defining the macro USER_RESERVED_MEM
* in config.h.  This routine returns the address of the reserved memory
* area.  The value of USER_RESERVED_MEM is in bytes.
*
* RETURNS: The address of the top of VxWorks memory.
*/

char * sysMemTop (void)
    {
    static char * memTop = NULL;

    if (memTop == NULL)
	{
	memTop = sysPhysMemTop () - USER_RESERVED_MEM;
	}

    return memTop;
    }

/*******************************************************************************
* sysToMonitor - transfer control to the ROM monitor
*
* This routine transfers control to the ROM monitor.  Normally, it is called
* only by reboot()--which services ^X--and by bus errors at interrupt level.
* However, in some circumstances, the user may wish to introduce a
* <startType> to enable special boot ROM facilities.
*
* The entry point for a warm boot is defined by the macro ROM_WARM_ADRS
* in config.h.  We do an absolute jump to this address to enter the
* ROM code.
*
* RETURNS: Does not return.
*/

STATUS sysToMonitor
    (
    int startType    /* parameter passed to ROM to tell it how to boot */
    )
    {
#ifdef  AE653_BUILD
    char    errorString[128];  /* HM error string */
    int     strLength;

    strLength = sprintf (errorString, "File: %s, Line: %d\n",
             __FILE__, __LINE__);

    VX_RAISE_ERROR (VX_ERROR_KERNEL, VX_ERR_SYSTEM_RESTART, startType,
            strLength + 1, errorString);
#endif

    intLock ();			/* disable interrupts */
    cacheDisable (INSTRUCTION_CACHE); /* Disable the Instruction Cache */
    cacheDisable (DATA_CACHE);        /* Disable the Data Cache */

#if     (CPU == PPC604)
    vxHid0Set (vxHid0Get () & ~_PPC_HID0_SIED);	/* Enable Serial Instr Exec */
#endif  /* (CPU == PPC604) */

    /* sysHwInit ();	*/	/* disable all sub systems to a quiet state */

    /* Reset  */
    sysOutByte (PCI_MSTR_ISA_IO_LOCAL + 0x92, 0x01);

    return (OK);		/* in case we continue from ROM monitor */
    }


/******************************************************************************
*
* sysProcNumGet - get the processor number
*
* This routine returns the processor number for the CPU board, which is
* set with sysProcNumSet().
*
* RETURNS: The processor number for the CPU board.
*
* SEE ALSO: sysProcNumSet()
*/

int sysProcNumGet (void)
    {
    return (sysProcNum);
    }


/******************************************************************************
*
* sysProcNumSet - set the processor number
*
* This routine sets the processor number for the CPU board.  Processor numbers
* should be unique on a single backplane.
*
* For bus systems, it is assumes that processor 0 is the bus master and
* exports its memory to the bus.
*
* RETURNS: N/A
*
* SEE ALSO: sysProcNumGet()
*/

void sysProcNumSet
    (
    int procNum			/* processor number */
    )
    {
    sysProcNum = procNum;
    }


/*******************************************************************************
*
* sysCpuCheck - confirm the CPU type
*
* This routine validates the cpu type.  If the wrong cpu type is discovered
* a message is printed using the serial channel in polled mode.
*
* RETURNS: N/A.
*
* NOMANUAL
*/

void sysCpuCheck (void)
    {
    int 	msgSize;	/* message size */
    int 	msgIx;		/* message index */
    SIO_CHAN * 	pSioChan;       /* serial I/O channel */

    /* Check for a valid CPU type;  If one is found, just return */

    if (CPU_TYPE == CPU_TYPE_750GX)
        {
	return;
	}

    /* Invalid CPU type; print error message and terminate */

    msgSize = strlen (wrongCpuMsg);

    sysSerialHwInit ();

    pSioChan = sysSerialChanGet (0);

    sioIoctl (pSioChan, SIO_MODE_SET, (void *) SIO_MODE_POLL);

    for (msgIx = 0; msgIx < msgSize; msgIx++)
    	{
    	while (sioPollOutput (pSioChan, wrongCpuMsg[msgIx]) == EAGAIN);
    	}

    sysToMonitor (BOOT_NO_AUTOBOOT);
    }



/*******************************************************************************
* sysMsDelay - delay for the specified amount of time (MS)
*
* This routine will delay for the specified amount of time by counting
* decrementer ticks.
*
* This routine is not dependent on a particular rollover value for
* the decrementer, it should work no matter what the rollover
* value is.
*
* A small amount of count may be lost at the rollover point resulting in
* the sysMsDelay() causing a slightly longer delay than requested.
*
* This routine will produce incorrect results if the delay time requested
* requires a count larger than 0xffffffff to hold the decrementer
* elapsed tick count.  For a System Bus Speed of 67 MHZ this amounts to
* about 258 seconds.
*
* RETURNS: N/A
*/

void sysMsDelay
    (
    UINT        delay                   /* length of time in MS to delay */
    )
    {
    register UINT oldval;              /* decrementer value */
    register UINT newval;              /* decrementer value */
    register UINT totalDelta;          /* Dec. delta for entire delay period */
    register UINT decElapsed;          /* cumulative decrementer ticks */

#ifndef CERT
    UINT64   sysTimestamp64 = 0;

    /*
     * TBR must be saved & restored if under intLock. Otherwise tick loss
     * reboots board.
     */
    if (!(vxMsrGet() & _PPC_MSR_EE))
	{
	sysTimestamp64 = sysTimestamp64Get();
	}
#endif /* !CERT */

    /*
     * Calculate delta of decrementer ticks for desired elapsed time.
     * The macro DEC_CLOCK_FREQ MUST REFLECT THE PROPER 6xx BUS SPEED.
     */

    totalDelta = ((DEC_CLOCK_FREQ / 4) / 1000) * delay;

    /*
     * Now keep grabbing decrementer value and incrementing "decElapsed" until
     * we hit the desired delay value.  Compensate for the fact that we may
     * read the decrementer at 0xffffffff before the interrupt service
     * routine has a chance to set in the rollover value.
     */

    decElapsed = 0;

    oldval = vxDecGet ();

    while (decElapsed < totalDelta)
        {
        newval = vxDecGet ();

        if ( DELTA(oldval,newval) < 1000 )
            decElapsed += DELTA(oldval,newval);  /* no rollover */
        else
            if (newval > oldval)
                decElapsed += LOCAL_ABS((int)oldval);  /* rollover */

        oldval = newval;
        }

#ifndef CERT
    /* restore saved TBR. */
    if (!(vxMsrGet() & _PPC_MSR_EE))
	{
	sysTimestamp64Set (sysTimestamp64);
	}
#endif /* !CERT */
    }


/******************************************************************************
*
* sysDelay - delay for a moment
*
* This routin delay for a moment
*
* NOMANUAL
*/

void sysDelay 
    (
    void
    )
    {
    sysMsDelay (1);
    }

/******************************************************************************
*
* sysUsDelay - delay for x microseconds.
*
* RETURNS: N/A.
*
* NOMANUAL
*/

void sysUsDelay
    (
    int delay
    )
    {
    register UINT32 oldval;      /* decrementer value */
    register UINT32 newval;      /* decrementer value */
    register UINT32 totalDelta;  /* Dec. delta for entire delay period */
    register UINT32 decElapsed;  /* cumulative decrementer ticks */
#ifndef CERT
    UINT64   sysTimestamp64 = 0;

    /*
     * TBR must be saved & restored if under intLock. Otherwise tick loss
     * reboots board.
     */
    if (!(vxMsrGet() & _PPC_MSR_EE))
	{
	sysTimestamp64 = sysTimestamp64Get();
	}
#endif /* !CERT */

    /* Calculate delta of decrementer ticks for desired elapsed time. */
    totalDelta = ((DEC_CLOCK_FREQ / 4) / 1000000) * (UINT32)delay;

    /*
     * Now keep grabbing decrementer value and incrementing "decElapsed"
     * we hit the desired delay value.  Compensate for the fact that we
     * read the decrementer at 0xffffffff before the interrupt service
     * routine has a chance to set in the rollover value.
     */

    decElapsed = 0;
    oldval = vxDecGet();
    while ( decElapsed < totalDelta )
        {
        newval = vxDecGet();
        if ( DELTA(oldval,newval) < 1000 )
            decElapsed += DELTA(oldval,newval);  /* no rollover */
        else if ( newval > oldval )
            decElapsed += LOCAL_ABS((int)oldval);  /* rollover */
        oldval = newval;
        }

#ifndef CERT
    /* restore saved TBR. */
    if (!(vxMsrGet() & _PPC_MSR_EE))
	{
	sysTimestamp64Set (sysTimestamp64);
	}
#endif /* !CERT */
    }


#if 0
/*******************************************************************************
* sysNvRamRead - read from eeprom.
*
* This routine reads a single byte from a specified offset in NVRAM.
* 
* RETURNS: N/A.
*
* NOMANUAL
*/

unsigned char sysNvRead
    (
    ULONG offset  /* NVRAM offset to read the byte from */
    )
    {
    UCHAR data;
    data = *(unsigned char*)(NVRAM_BASE_ADDR + offset);
    return data;
    }

/*******************************************************************************
* sysNvRamWrite - write to eeprom.
*
* This routine writes a single byte to a specified offset in NVRAM.
*
* RETURNS: N/A.
*
* NOMANUAL
*/

void sysNvWrite
    (
    ULONG	offset,	/* NVRAM offset to write the byte to */
    UCHAR	data	/* datum byte */
    )
    {
    volatile unsigned char *  ptr;
    int     count = 500;
 
    ptr = (unsigned char*)(NVRAM_BASE_ADDR + offset);
 
    *ptr = data;
    
    while (*ptr != data)
        {
        
        sysMsDelay (5); /* 5 milliseconds */
 
        if (count-- == 0) /* abort */
            break;
        }    
    }
#endif

#if defined(INCLUDE_ATA) || defined(INCLUDE_NE2000END)
/*******************************************************************************
*
* sysInByteString - reads a string of bytes from an io address.
*
* This function reads a byte string from a specified o address.
*
* RETURNS: N/A
*/

void sysInByteString
    (
    ULONG 	ioAddr,
    char * 	bufPtr,
    int    	nBytes
    )
    {
    int loopCtr;

    for (loopCtr = 0; loopCtr < nBytes; loopCtr++)
        *bufPtr++ = *(char *)ioAddr;
    PPC_EIEIO_SYNC;
    }

/*******************************************************************************
*
* sysOutByteString - writes a string of bytes to an io address.
*
* This function writes a byte string to a specified io address.
*
* RETURNS: N/A
*/

void sysOutByteString
    (
    ULONG 	ioAddr,
    char * 	bufPtr,
    int   	nBytes
    )
    {
    int loopCtr;

    for (loopCtr = 0; loopCtr < nBytes; loopCtr++)
        *(char *)ioAddr = *bufPtr++;
    PPC_EIEIO_SYNC;
    }

/*******************************************************************************
*
* sysInWordString - reads a string of words from an io address.
*
* This function reads a word string from a specified io address.
*
* RETURNS: N/A
*/

void sysInWordString
    (
    ULONG	ioAddr,
    UINT16 *	bufPtr,
    int		nWords
    )
    {
    int loopCtr;

    for (loopCtr = 0; loopCtr < nWords; loopCtr++)
        *bufPtr++ = *(short *)ioAddr;
    PPC_EIEIO_SYNC;
    }

/*******************************************************************************
*
* sysInWordStringRev - reads a string of words that are byte reversed
*
* This function reads a string of words that are byte reversed from a
* specified io address.
*
* RETURNS: N/A
*/

void sysInWordStringRev
    (
    ULONG      ioAddr,
    UINT16 *   bufPtr,
    int        nWords
    )
    {
    int loopCtr;

    for (loopCtr = 0; loopCtr < nWords; loopCtr++)
        *bufPtr++ = sysInWord (ioAddr);
    }

/*******************************************************************************
*
* sysOutWordString - writes a string of words to an io address.
*
* This function writes a word string from a specified io address.
*
* RETURNS: N/A
*/

void sysOutWordString
    (
    ULONG	ioAddr,
    UINT16 *	bufPtr,
    int		nWords
    )
    {
    int loopCtr;

    for (loopCtr = 0; loopCtr < nWords; loopCtr++)
        *(short *)ioAddr = *bufPtr++;
    PPC_EIEIO_SYNC;
    }

/*******************************************************************************
*
* sysInLongString - reads a string of longwords from an io address.
*
* This function reads a longword string from a specified io address.
*
* RETURNS: N/A
*/

void sysInLongString
    (
    ULONG    ioAddr,
    ULONG *  bufPtr,
    int      nLongs
    )
    {
    int loopCtr;

    for (loopCtr = 0; loopCtr < nLongs; loopCtr++)
        *bufPtr++ = *(int *)ioAddr;
    PPC_EIEIO_SYNC;
    }

/*******************************************************************************
*
* sysOutLongString - writes a string of longwords to an io address.
*
* This function writes a longword string from a specified io address.
*
* RETURNS: N/A
*/

void sysOutLongString
    (
    ULONG	ioAddr,
    ULONG *	bufPtr,
    int		nLongs 
    )
    {
    int loopCtr;

    for (loopCtr = 0; loopCtr < nLongs; loopCtr++)
        *(int *)ioAddr = *bufPtr++;
    PPC_EIEIO_SYNC;
    }


/*******************************************************************************
*
* sysIntEnablePIC - enable a bus interrupt level
*
* This routine enables a specified bus interrupt level.
*
* RETURNS: OK, or ERROR if failed.
*
* ERRNO
*
*/

STATUS sysIntEnablePIC
    (
    int irqNo     /* IRQ(PIC) or INTIN(APIC) number to enable */
    )
    {
    return (intEnable (irqNo));
    }


#endif	/* INCLUDE_ATA */
