/* w83553PciIbc.c - Winbond 83553 PCI-ISA Bridge Controller (IBC) driver */

/* Copyright 1984-1999 Wind River Systems, Inc. */
/* Copyright 1996-1998 Motorola, Inc. */
#include "copyright_wrs.h"

/* Debug interrupt used by ACATS for Ada.Interrupts.  */
/* #define DEBUG_INT50 */

/*
modification history
--------------------
01a,10oct99,mtl   written from SPS/Motorola & yk 750 by teamF1
*/

/*
DESCRIPTION
This module implements the Motorola w83553 ISA Bridge Controller (IBC)
driver.

The w83553 Chip is a highly integrated ASIC providing PCI-ISA interface
chip.  It provides following major capabilities:

  PCI Bus Master capability for ISA DMA.
  PCI Arbiter capability
  PCI Power management control
  64 byte PCI bus FIFO for burst capability
  Standard ISA interrupt controllers (82C59)
  Standard ISA timer/counter (82C54)

This driver is limited to the interrupt controller feature of the chip.
This driver does not interact with any other drivers supporting the
w83553 chip.

The chip implements a standard ISA bus interrupt system consisting of
two 82C59A interrupt controller units.  The two units are cascaded 
together to provide 15 actual interrupt lines.  The first unit implements
interrupt number 0 thru 7.  Interrupt number 2 is the cascaded input
from the second 82C59A controller, so it functionally does not exist
as a user input.  The inputs on the second controller are numbered 8 through
15.  Since they are cascaded into interrupt number 2 on the first unit,
their actual priority is higher than inputs 3 through 7 on the first
unit.  The true priority heirarchy (from high to low) is: 0, 1, 8, 9,
10, 11, 12, 13, 14, 15, 3, 4, 5, 6, 7.  The highest priority input that
is active is the unit that shall receive service first.

The actual vector number corresponding to IRQ0 is determined by the value
in sysVectorIRQ0 at initialization time.

Based on the IBM-PC legacy system, this driver supports 16 levels, each
of which maps to a single vector.  Since PCI interrupt lines are shared, this
driver does provide for overloading of interrupt routines (i.e. there is
a list of interrupt routines for each interrupt vector (level)).  To service
a vector requires that all connected interrupt routines be called in order
of their connection.

.SH INITIALIZATION

This driver is initialized from the BSP, usually as part of sysHwInit().
The first routine to be called is sysPicInit(). The routine verifies the type
of device and then initializes the interrupt pins routine to IRQ numbers.


Typical device initialization looks like this:

.CS
  /@ Initialize the extended portion of the IBC's PCI Header.  @/

  int pciBusNum;
  int pciDevNum;
  int pciFuncNum;

  if (pciFindDevice ((PCI_ID_IBC & 0xFFFF), (PCI_ID_IBC >> 16) & 0xFFFF, 0,
                       &pciBusNum, &pciDevNum, &pciFuncNum) != ERROR)
      {
      sysPicInit ();
      }
.CE

.SH CUSTOMIZING THIS DRIVER

The macros PIC_BYTE_OUT and PIC_BYTE_IN provide the hardware access methods.
By default they call the routines sysOutByte() and sysInByte(), which are
presumed to be defined by the BSP with the appropriate memory map offsets.
The user may redefine these macros as needed.

The macros CPU_INT_LOCK() and CPU_INT_UNLOCK provide the access
to the CPU level interrupt lock/unlock routines.  We presume that there
is a single interrupt line to the CPU.  By default these macros call
intLock() and intUnlock() respectively.
*/

/*  includes  */

#include "vxWorks.h"
#include "config.h"
#include "errno.h"
/*#include "sp.h" */
#include "w83553PciIbc.h"

#ifdef INCLUDE_WINDVIEW
#include "private/eventP.h"
#endif

/*  interrupt handler description  */
    
typedef struct intHandlerDesc     
    {
    VOIDFUNCPTR			vec;	/* interrupt vector */
    int				arg;	/* interrupt handler argument */
    struct  intHandlerDesc *	next;	/* pointer to the next handler */
    } INT_HANDLER_DESC;
#define  INTERRUPT_TABLESIZE   256

/* globals */

INT_HANDLER_DESC * sysIntTbl [INTERRUPT_TABLESIZE]; /* system interrupt tbl */


/* defines */

/* externs */

IMPORT STATUS 	excIntConnect (VOIDFUNCPTR *, VOIDFUNCPTR);
IMPORT void	sysOutByte (ULONG, UCHAR);
IMPORT UCHAR	sysInByte (ULONG);

IMPORT UINT	sysVectorIRQ0;		/* vector for IRQ0 */

/* forward declarations */

LOCAL int	sysPciIbcIntEnable (int intLevel);
LOCAL int	sysPciIbcIntDisable (int intLevel);
LOCAL UCHAR	sysPicPhantomInt (UCHAR *intNum, int lvl7Int, int lvl15Int);
LOCAL void	sysPicEndOfInt (int intLevel);
void 		sysPicIntLevelSet(int intLevel);
void 		sysPicIntHandler (void);


#if FALSE /* defined(INCLUDE_WINDVIEW) || defined(INCLUDE_INSTRUMENTATION) */
IMPORT	int	evtTimeStamp;  /* Windview 1.0 only */
#endif

/* Mask values are the currently disabled sources */

LOCAL UINT8	sysPicMask1 = 0xfb;	/* all levels disabled */
LOCAL UINT8	sysPicMask2 = 0xff;

/* Level values are the interrupt level masks */

LOCAL UINT8	sysPicLevel1 = 0;
LOCAL UINT8	sysPicLevel2 = 0;
LOCAL UINT8	sysPicLevelCur = 16;	/* curr intNum is 15, all enabled */

/* level values by real priority */

LOCAL const UCHAR sysPicPriMask1[17] =
    {
    0xFB,0xFA,0xF8,0xF8,0xF0,0xE0,0xC0,0x80,
    0xF8,0xF8,0xF8,0xF8,0xF8,0xF8,0xF8,0xF8,0x0
    };

LOCAL const UCHAR sysPicPriMask2 [17] =
    {
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,
    0xFF,0xFE,0xFC,0xF8,0xF0,0xE0,0xC0,0x80,0x0
    };

/*******************************************************************************
*
*  sysPicInit  -  Initialize the PIC  (Winbond ISA bridge Controller
*                 also known as a PIB (PCI to ISA Bridge)
*
* This routine initializes the non PCI header configuration registers
* of the PIC within the W83C553F PIC  Processor Interrupt Controller.
*
* RETURNS:  OK always
*
*/

STATUS  sysPicInit (void)
    {
    UCHAR intNum;

    /*  Initialize the Interrupt Controller #1  */

    PIC_BYTE_OUT (PIC1_ICW1, 0x11);  /* ICW1 */
    /*  bit 4 = 1 -> select ICW1, bit1 SNGL = 0 -> two cascaded, 
     *  bit 0 = 1 -> ICW4 must be intialized, indicates x86 mode,
     *  indicates that no call opcode is sent to the processor. 
     */

    PIC_BYTE_OUT (PIC1_ICW2, sysVectorIRQ0);  /* ICW2 */

    PIC_BYTE_OUT (PIC1_ICW3, 0x04);  /* ICW3 */
    /*  bit 2 = 1 -> cascades this PIC1 is cascaded to PIC2. */

    PIC_BYTE_OUT (PIC1_ICW4, 0x01);  /* ICW4 */
    /*  bit 0 = 1 -> x86 system, again no call opcode is generated */


    /*
     *  Mask interrupts IRQ 0, 1, and 3-7 by writing to OCW1 register
     * IRQ 2 is the cascade input and is not masked
     *                                          ORQs 7654 3210
     */
    PIC_BYTE_OUT (PIC1_OCW1, 0xfb);  	     /* OCW1 */


    /*  Initialize the Interrupt Controller  #2	*/

    PIC_BYTE_OUT (PIC2_ICW1, 0x11);  /* ICW1 */
    /*  bit 4 = 1 -> select ICW1, bit3 SNGL = 0 -> two cascaded, 
     *  bit 0 = 1 -> ICW4 must be intialized, indicates x86 mode,
     *  indicates that no call opcode is sent to the processor. 
     */

    PIC_BYTE_OUT (PIC2_ICW2, sysVectorIRQ0 + 8); /* ICW2 */

    PIC_BYTE_OUT (PIC2_ICW3 , 0x02);  /* ICW3 */
    /*  bit 1 = 0 -> this is the slave identification code
     *  i.e. this PIC2 is the cascaded (slave) to PIC1 
     */

    PIC_BYTE_OUT (PIC2_ICW4 , 0x01);  /* ICW4 */
    /*  bit 0 = 1 -> x86 system, again no call opcode is generated */


    /* Mask interrupts IRQ 15-8 by writing to OCW1 register */

    PIC_BYTE_OUT (PIC2_OCW1, 0xff);

    /* Permanently turn off ISA refresh by never completing init steps */
    PIC_BYTE_OUT (W83553_TRM1_CMOD, 0x74);

    /*
     * Perform PCI Interrupt Ack cycle  
     * Esentially to ensure that we clear all interrupts
     * before we return and start the kernel
     */

    MEM_READ(PCI_MSTR_IACK_LOCAL, &intNum);
 
    /*  perform the end of interrupt procedure  */

    sysPicEndOfInt (15); 

    sysPciIbcIntEnable(2);

    /* For Qemu.  */
    sysPciIbcIntEnable(14);

    sysPicIntLevelSet (16);

    return (OK);
    }   /*  end of 	sysPicInit  */

/*******************************************************************************
*
* sysPciIbcIntEnable - enable a PIC interrupt level
*
* This routine enables a specified PIC interrupt level.
*
* NOMANUAL
*
* RETURNS: OK.
*/

LOCAL STATUS  sysPciIbcIntEnable
    (
    int intNum
    )
    {
    if (intNum < WB_MAX_IRQS)
        {
        if (intNum < 8)
            {
            sysPicMask1 &= ~(1 << intNum);
            PIC_BYTE_OUT (PIC1_OCW1, sysPicMask1 | sysPicLevel1);
            }
        else
            {
            sysPicMask2 &= ~(1 << (intNum - 8));
            PIC_BYTE_OUT (PIC2_OCW1, sysPicMask2 | sysPicLevel2);
            }
        return (OK);
        }
    else
        {
        return ERROR;
        }
    }
 
/*******************************************************************************
*
* sysPciIbcIntDisable - disable a Pic interrupt level
*
* This routine disables a specified Pic interrupt level.
*
* NOMANUAL
*
* RETURNS: OK.
*
*/

LOCAL int  sysPciIbcIntDisable
    (
    int intNum	/*  interrupt level to disable  */
    )
    {

    if (intNum < WB_MAX_IRQS)
        {
        if (intNum < 8)
            {
            sysPicMask1 |= (1 << intNum);
            PIC_BYTE_OUT (PIC1_OCW1, sysPicMask1 | sysPicLevel1 );
            }
        else
            {
            sysPicMask2 |= (1 << (intNum - 8));
            PIC_BYTE_OUT (PIC2_OCW1, sysPicMask2 | sysPicLevel2);
            }

        if ((sysPicMask1 == 0xfb) && (sysPicMask2 == 0xff))
            return (OK);
        else
            return (sysPicMask1 | sysPicMask2);
        }
    else
        {
        return ERROR;
        }
    }

 
/*******************************************************************************
*
* pciIbcIntHandler - handle a Winbond interrupt
*
*
* This routine handles an interrupt which may have originated from the
* Winbond chip's 8259 controllers. Since we support sharing, it is not
* known whether the interrupt really came from the Winbond or was
* actually generated by another device which shares the same IRQ level
* as the Winbond chip on the EPIC. So first, the IRR is checked to
* see if an interrupt was pending, and if not the handler is not invoked.
*
* NOMANUAL
*
* RETURNS: N/A
*/

LOCAL void pciIbcIntHandler(void)
    {
    UCHAR 	irrVal;
    UCHAR 	intVec;
    UCHAR 	winbIntNum;
    int		oldLevel;
    static int  lvl7Int = 0;
    static int  lvl15Int = 0;
    INT_HANDLER_DESC * pCurrHandler;

    /*qDebug ("pciIbcIntHandler");  */
    /*
     * read the IRR and check if the interrupt came from the Winbond
     * This is beause we allow sharing of interrupt vectors between the
     * devices attached directly to the EPIC and the ones controlled by
     * the Winbond chip.
     */
    
    PIC_BYTE_OUT (PIC1_OCW3, 0x08+0x02);

    PIC_BYTE_IN (PIC1_OCW3, &irrVal);

    if (irrVal == 0) /* check cascaded 8259 */
        {
        PIC_BYTE_OUT (PIC2_OCW3, 0x08+0x02);
        PIC_BYTE_IN (PIC2_OCW3, &irrVal);
        }

    if (irrVal == 0)
        {
        return;
        }

    /*
     * reading ISA_INTR_ACK_REG generates PCI IACK cycles on the PCI bus
     * which will cause the PIC to put out the vector on the bus.
     * This occurs because Bit 5 of the PCI control reg in the PIC is 
     * programmed to 1 by default.
     */

    MEM_READ(PCI_MSTR_IACK_LOCAL, &intVec); 	/* read the vector */

#if 0
    switch (intVec)
      {
      case 4: /* serial */
      case 13: /* ide */
	break;
      default:
	qDebug ("intVec=%d\n", intVec);
      }
#endif

    if ((intVec == 0xff) || (intVec > INT_NUM_IRQ0 +WB_MAX_IRQS))
        {
        /* bogus vector value on bus i.e. interrupt not meant for winbond */
        return;
        }

    winbIntNum = intVec - INT_VEC_IRQ0;

    /* 
     * In both edge and level triggered modes, the IR inputs must
     * remain high until until after the falling edge of the first
     * !INTA. If the IR input goes low before this time a DEFAULT
     * IR7 will occur when the CPU acknowleges the interrupt. This
     * can be a useful safeguard for detecting interrupts caused by
     * spurious noise glitches on the IR inputs.  (sic) 
     */

    if ((winbIntNum & 7) == 7)   /* Special check for phantom interrupts */
        {
        if (sysPicPhantomInt (&winbIntNum, lvl7Int, lvl15Int) == TRUE)
            {
            return;             /* It's phantom so just return. */
            }
        }
        
    /* Keep track of level 7 & 15 nesting for phantom interrupt handling */

    if (winbIntNum == 7)
        {
        lvl7Int++;
        }
    else if (winbIntNum == 15) /* yellowknife code has a bug here */
        {
        lvl15Int++;
        }

    /* mask this interrupt level and lower priority interrupt levels */

    oldLevel = sysPicLevelCur;

    sysPicIntLevelSet (winbIntNum); 

    /* Re-arm (enable) the interrupt chip */

    sysPicEndOfInt (winbIntNum);

    /*  call each respective interrupt handler */	

    /* Kludge for qemu.  */
    if (intVec == 14)
      intVec = 0x50;

    if ((pCurrHandler = sysIntTbl [intVec]) == NULL)
        {
	logMsg ("uninitalized PIC interrupt vector %x\r\n",
                intVec, 0,0,0,0,0);
        }
    else
        {

#ifdef DEBUG_INT50
	  if (intVec == 0x50)
	    qDebug ("handler: pHandler %p, %p\n", pCurrHandler,
		    pCurrHandler ? pCurrHandler->vec : 0);
#endif

	/* call Each respective chained interrupt handler  */
	while (pCurrHandler != NULL)
            {
#ifdef DEBUG_INT50
	      if (intVec == 0x50)
		qDebug ("handler: pHandler %p (%p)\n",
			pCurrHandler->vec, pCurrHandler->arg);
#endif
  	    (*pCurrHandler->vec) (pCurrHandler->arg);
	    pCurrHandler = pCurrHandler->next;
            }

        } /* else */

    if (winbIntNum == 7)
        {
        lvl7Int--;
        }
    else if (winbIntNum == 15)
        {
        lvl15Int--;
        }
        
    /*  set the original interrupt mask back the way it was */
    sysPicIntLevelSet (oldLevel);
    }

/*******************************************************************************
*
* sysPicEndOfInt - send EOI(end of interrupt) signal.
*
* NOTES
*
* This routine is called at the end of the interrupt handler to
* send a non-specific end of interrupt (EOI) signal.
*
* The second PIC is acked only if the interrupt came from that PIC.
* The first PIC is always acked.
*
* RETURNS: N/A
*/

LOCAL void  sysPicEndOfInt
    (
    int intNum
    )
    {
    if (intNum > 7)
	{
        PIC_BYTE_OUT (PIC2_OCW2, 0x20); /* 0010 0000*/
	}
    PIC_BYTE_OUT (PIC1_OCW2, 0x20); /* 0010 0000*/
    }  /* End of sysPicEndOfInt	 */

/*******************************************************************************
*
* sysPicIntLevelSet - set the interrupt priority level
*
* NOTES
*
* This routine masks interrupts with real priority equal to or lower than
* <intNum>.  The special
* value 16 indicates all interrupts are enabled. Individual interrupt
* numbers have to be specifically enabled by sysPicIntEnable() before they
* are ever enabled by setting the interrupt level value.
*
* Note because of the IBM cascade scheme, the actual priority order for
* interrupt numbers is (high to low) 0, 1, 8, 9, 10, 11, 12, 13, 14, 15,
* 3, 4, 5, 6, 7, 16 (all enabled)
*
* INTERNAL: It is possible that we need to determine if we are raising
* or lowering our priority level.  It may be that the order of loading the
* two mask registers is dependent upon raising or lowering the priority.
*
* RETURNS: N/A
*/

void  sysPicIntLevelSet
    (
    int intNum   /*  interrupt level to implement  */
    )
    {
    if (intNum > 16)
	intNum = 16;

    sysPicLevelCur = intNum;

    if (sysPicLevel2 != sysPicPriMask2[intNum])
	{
	sysPicLevel2 = sysPicPriMask2[intNum];
	PIC_BYTE_OUT (PIC2_OCW1, sysPicMask2 | sysPicLevel2);
	}

    if (sysPicLevel1 != sysPicPriMask1[intNum])
	{
	sysPicLevel1 = sysPicPriMask1[intNum];
	PIC_BYTE_OUT (PIC1_OCW1, sysPicMask1 | sysPicLevel1);
	}
    }  /*  end of sysPicIntLevelSet	*/

/*******************************************************************************
*
* sysPicPhantomInt - Determine if IRQ interrupt number 7 or 15 is "phantom".
*
* NOTES
*
* This routine determines if an IRQ number of 7 or 15 is a phantom
* interrupt.  According to Intel 82C59A-2 documentation, the IR (interrupt
* request) inputs must remain high until after the falling edge of the first
* INTA (interrupt acknowledge).  If the IR goes low before this, a DEFAULT
* (phantom) IRQ7 will occur when the CPU acknowledges the interrupt.  Note
* that if an IRQ7 is generated it may really be another interrupt, IRQ4 for
* example.  IRQ 7 is associated  with the master 8259, IRQ 15 is associated 
* with the slave 8259.  This function should only be called if the 
* acknowledged IRQ number is 7 or 15 but does behave sanely if called with
* other IRQ numbers.
*
* RETURNS: TRUE if phantom IRQ, *intNum unaltered.
*          FALSE if not phantom interrupt, *intNum is "real" IRQ number.
*/
        
LOCAL UCHAR sysPicPhantomInt
    (
    UCHAR *intNum,      /* interrupt number received on acknowledge */
    int   lvl7Int,      /* interrupt 7 nesting level */
    int   lvl15Int      /* interrupt 15 nesting level */
    )
    {
    UINT irqBit;
    UINT irqNum;
    switch (*intNum)
        {
        case 7:
        
            /* Read the in-service register (ISR) */
        
            PIC_BYTE_OUT (PIC1_OCW3, 0x08+0x03);
            PIC_BYTE_IN (PIC1_OCW3, &irqBit);
        
            if (irqBit == 0)
                return (TRUE);  /* No in-service int so it MUST be phantom */
            else
                {
                for (irqNum = 0; ((irqBit & 1) == 0) ; irqNum++, irqBit >>= 1)
                            ;
                if (irqNum == 7)
                    if (lvl7Int > 1)
                        return (TRUE); /* We're nested so it MUST be phantom */
                *intNum = irqNum;
                return (FALSE);
                }
            break;
        
        case 15:
        
            /* Read the in-service register (ISR) */
        
            PIC_BYTE_OUT (PIC2_OCW3, 0x08+0x03);
            PIC_BYTE_IN (PIC2_OCW3, &irqBit);
        
            if (irqBit == 0)
                return (TRUE);  /* No in-service int so it MUST be phantom */
            else
                {
                for (irqNum = 8; ((irqBit & 1) == 0) ; irqNum++, irqBit >>= 1)
                            ;
                if (irqNum == 15)
                    if (lvl15Int > 1)
                        return (TRUE);  /* Were nested so it MUST be phantom */
                *intNum = irqNum;
                return (FALSE);
                }
            break;
        
        default:
            return (FALSE);
        }
    }


/*******************************************************************************
*
* sysEpicIntConnect - connect an interrupt handler to the system vector table
*
* This function connects an interrupt handler to the system vector table.
*
* RETURNS: OK or ERROR.
*/

LOCAL STATUS sysEpicIntConnect
    (
    VOIDFUNCPTR * 	vector,		/* interrupt vector to attach */
    VOIDFUNCPTR		routine,	/* routine to be called */
    int			parameter	/* parameter to be passed to routine */
    )
    {
    INT_HANDLER_DESC *	pNewHandler;
    INT_HANDLER_DESC *	pCurrHandler;
    int			intVal;

#ifdef DEBUG_INT50
    if (vector == 0x50)
      qDebug ("sysEpicIntConnect: vector=%d func=%p, param=%d\n",
	      (int)vector, routine, parameter);
#endif

    if (((int)vector < 0)  || ((int) vector > INTERRUPT_TABLESIZE)) 
	{
        return (ERROR);   /*  out of range  */
	}

    /* create a new interrupt handler */

    pNewHandler = malloc (sizeof (INT_HANDLER_DESC));

    /* check if the memory allocation succeed */
    if (pNewHandler == NULL)
	return (ERROR);

    /*  initialize the new handler  */
    pNewHandler->vec = routine;
    pNewHandler->arg = parameter;
    pNewHandler->next = NULL;

    /* install the handler in the system interrupt table  */

    intVal = intLock (); /* lock interrupts to prevent races */

    if (sysIntTbl [(int) vector] == NULL)
	{
        sysIntTbl [(int) vector] = pNewHandler;  /* single int. handler case */
	}
    else
	{
        pCurrHandler = sysIntTbl[(int) vector];/* multiple int. handler case */

        while (pCurrHandler->next != NULL)
            {
            pCurrHandler = pCurrHandler->next;
            }
        
        pCurrHandler->next = pNewHandler;

	}

    intUnlock (intVal);

    return (OK);
    }

/*******************************************************************************
*
* sysEpicIntrInit - initialize the interrupt table
*
* This function initializes the interrupt mechanism of the board.
*
* RETURNS: OK, always.
*/

STATUS  sysPicIntrInit (void)
    {
    int vector;

    /* initialize the interrupt table */

    for (vector = 0; vector < INTERRUPT_TABLESIZE; vector++)
        {
	sysIntTbl [vector] = NULL;
        }

    /*
     * connect the interrupt demultiplexer to the PowerPC external 
     * interrupt exception vector.
     * i. e.  put the address of this interrupt handler in
     * the PowerPC's only external interrupt exception vector
     * which is  _EXC_OFF_INTR = 0x500
     */

    excIntConnect ((VOIDFUNCPTR *) _EXC_OFF_INTR, pciIbcIntHandler);

    /*  
     * set up the BSP specific interrupt routines
     * Attach the local routines to the VxWorks system calls
     *
     */

    _func_intConnectRtn  =  sysEpicIntConnect;
    _func_intEnableRtn   =  sysPciIbcIntEnable;
    _func_intDisableRtn  =  sysPciIbcIntDisable;

    return (OK);
    }
