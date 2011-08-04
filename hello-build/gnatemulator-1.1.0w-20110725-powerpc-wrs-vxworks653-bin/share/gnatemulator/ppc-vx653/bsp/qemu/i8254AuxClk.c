/* i8254AuxClk.c - Intel 8254 timer library */

/* Copyright 1984-1999 Wind River Systems, Inc. */

#include "copyright_wrs.h"

/*
modification history
--------------------
01a,10oct99,mtl   written from yk 750 by teamF1
*/

/*
DESCRIPTION
This library contains routines to manipulate the timer functions on the
Intel 8254 compatable chip with a board-independent interface.  This 
library handles the vxWorks auxiliary clock functions.  The macros 
AUX_CLK_RATE_MIN, and AUX_CLK_RATE_MAX must be defined to provide 
parameter checking for the sysAuxClkRateSet() routines.  These are 
defined in <bsp>.h.

This driver runs the 8254 in mode five, "Hardware Triggered Pulse Mode".
In this mode, the i8254 pulls its OUT signal when 0 is reached in its 
internal counter.  The OUT signal fires (drops low for 1 CLK) every 
(COUNT+1) CLKs, where COUNT is the initial counter value.  The count 
is reloaded automatically (actually based on GATE).  

The driver macro PERIOD(x) calculates the proper value to load 
into the counter to obtain the desired auxillary clock rate, which 
is specified in "x".

The BSP macro TIMER_HZ represents the i8254 input CLK rate, as defined 
in <bsp>.h.  x = the desired rate of the auxillary clock, specified in 
Hertz.  The calculation is:  COUNT = ((TIMER_HZ / auxClkRateInHz) + 1)
TIMER_HZ is the value input to the i8254 CLK pin.  On the W83C553, the 
i8254 CLK pin is internally routed from the W83C553 cip OSC (pin#172) 
INPUT, which is divided down by 12.  On the Sandpoint, this OSC 
input is 14.31818 MHz as generated from the CDC9843 clock synth and 
FPX143 XTAL combination.   The macro TIMER_HZ below represents this 
configuration in yk.h.

*/

/* defines */

#ifndef	SYS_BYTE_OUT
#    define	SYS_BYTE_OUT(reg,data)  		\
			    (sysOutByte (reg+PCI_MSTR_ISA_IO_LOCAL,data))
#endif

#ifndef	SYS_BYTE_IN
#    define	SYS_BYTE_IN(reg,pData)  		\
			   (*pData = sysInByte (reg+PCI_MSTR_ISA_IO_LOCAL))
#endif

#ifndef	SYS_INT_DISABLE
#    define	SYS_INT_DISABLE(x)	intDisable (x)
#endif

#ifndef	SYS_INT_ENABLE
#    define	SYS_INT_ENABLE(x)	 intEnable (x)
#endif


/*  
 * When running in 8254 mode five, "Hardware Triggered Pulse Mode", the 
 * i8254 pulls its OUT signal when 0 is reached in its internal counter. 
 * The OUT signal fires (drops low for 1 CLK) every (COUNT+1) CLKs,
 * where COUNT is the initial counter value.  The count is reloaded
 * automatically (actually based on GATE).  The macro "PERIOD (x)" 
 * calculates the proper value to load into the counter to obtain the 
 * desired auxillary clock rate, which is specified in "x".
 *
 * TIMER_HZ represents the i8254 input CLK rate,as defined in <bsp>.h.  
 * x = the desired rate of the auxillary clock, specified in Hertz.
 * The calculation is:  COUNT = ((TIMER_HZ / auxClkRateInHz) + 1)
 */

#define PERIOD(x)	((TIMER_HZ / (x)) + 1)

/* locals */

LOCAL FUNCPTR sysAuxClkRoutine		= NULL;	/* usr clock interrupt rtn */
LOCAL volatile int sysAuxClkArg		= 0;	/* rtn argument */
LOCAL volatile int auxClkTicksPerSecond	= 60;	/* auxiliary clock rate */
LOCAL volatile BOOL auxClkRunning	= FALSE;/* auxiliary clock enabled? */

/*******************************************************************************
*
* sysAuxClkInt - interrupt level processing for timer 0.
*
* This routine handles the interrupts associated with the Intel 8254 chip.
* It is attached to the interrupt vector by sysAuxClkConnect().
*
* RETURNS: N/A
*/

LOCAL void sysAuxClkInt 
    (
    void
    )
    {
    if ((sysAuxClkRoutine != NULL) && (auxClkRunning == TRUE))
	{
        (* sysAuxClkRoutine) (sysAuxClkArg);	/* call aux clock routine */
	}
    }


/*******************************************************************************
*
* sysAuxClkConnect - connect a routine to the auxiliary clock interrupt
*
* This routine specifies the interrupt service routine to be called at each
* auxiliary clock interrupt.  It does not enable auxiliary clock
* interrupts.
*
* RETURNS: OK, or ERROR if the routine cannot be connected to the interrupt.
*
* SEE ALSO: intConnect(), sysClkEnable()
*/

STATUS sysAuxClkConnect
    (
    FUNCPTR routine,	/* routine called at each aux. clock interrupt */
    int     arg		/* argument with which to call routine         */
    )
    {
    static BOOL connected = FALSE;
    sysAuxClkRoutine   = routine;
    sysAuxClkArg       = arg;

    if (connected == FALSE)
	{
	(void) intConnect (INUM_TO_IVEC((int)INT_VEC_TIMER0),
			   sysAuxClkInt,0);

	}
    else
	{
	logMsg ("Already connected an aux clock ISR.\n",1,2,3,4,5,6);
	return (ERROR);
	}

    connected = TRUE;
 
    return (OK);
    }

/********************************************************************************
* sysAuxClkDisable - turn off auxiliary clock interrupts
*
* This routine disables auxiliary clock interrupts.
*
* RETURNS: N/A
*
* SEE ALSO: sysAuxClkEnable()
*/

void sysAuxClkDisable 
    (
    void
    )
    {
    SYS_INT_DISABLE (INT_LVL_TIMER0);

    if (auxClkRunning == TRUE)
	{
	/* stop timer 0, actually we get one more interrupt after this */

	SYS_BYTE_OUT (W83553_TRM1_CMOD, 
	    (CW_SELECT (0) | CW_BOTHBYTE | CW_MODE (MD_SWTRIGSB)));

	SYS_BYTE_OUT (W83553_TRM1_CNT0, LSB (0));

	SYS_BYTE_OUT (W83553_TRM1_CNT0, MSB (0));

	auxClkRunning = FALSE;

	PPC_EIEIO_SYNC;
	}
    }

/********************************************************************************
* sysAuxClkEnable - turn on auxiliary clock interrupts
*
* This routine enables auxiliary clock interrupts.
*
* RETURNS: N/A
*
* SEE ALSO: sysAuxClkConnect(), sysAuxClkDisable(), susAuxClkRateSet()
*/

void sysAuxClkEnable 
    (
    void
    )
    {
    if (auxClkRunning == FALSE)
	{

        /* Initialize timer 0, aux clock */

	SYS_BYTE_OUT (W83553_TRM1_CMOD, 
	    (CW_SELECT (0) | CW_BOTHBYTE | CW_MODE (MD_RATEGEN)));

	SYS_BYTE_OUT (W83553_TRM1_CNT0,(LSB(PERIOD(auxClkTicksPerSecond))));

	SYS_BYTE_OUT (W83553_TRM1_CNT0,(MSB(PERIOD(auxClkTicksPerSecond))));

	SYS_INT_ENABLE (INT_LVL_TIMER0);

	auxClkRunning = TRUE;

	PPC_EIEIO_SYNC;
	}
    }

/*******************************************************************************
*
* sysAuxClkRateGet - get the auxiliary clock rate
*
* This routine returns the interrupt rate of the auxiliary clock.
*
* RETURNS: The number of ticks per second of the auxiliary clock.
*
* SEE ALSO: sysAuxClkEnable(), sysAuxClkRateSet()
*/

int sysAuxClkRateGet 
    (
    void
    )
    {
    return (auxClkTicksPerSecond);
    }

/*******************************************************************************
*
* sysAuxClkRateSet - set the auxiliary clock rate
*
* This routine sets the interrupt rate of the auxiliary clock.
* It does not enable auxiliary clock interrupts.
*
* RETURNS: OK, or ERROR if the tick rate is invalid or the timer cannot be set.
*
* SEE ALSO: sysAuxClkEnable(), sysAuxClkRateGet()
*/

STATUS sysAuxClkRateSet
    (
    FAST int ticksPerSecond	    /* number of clock interrupts per second */
    )
    {
    if (ticksPerSecond < AUX_CLK_RATE_MIN || ticksPerSecond > AUX_CLK_RATE_MAX)
	{
	return (ERROR);
	}

    auxClkTicksPerSecond = ticksPerSecond;
     
    if (auxClkRunning == TRUE)
	{
	sysAuxClkDisable ();
	sysAuxClkEnable ();
	}

    return (OK);
    }
