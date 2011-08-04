/* i8250Sio.c - I8250 serial driver */

/* Copyright 1984-2006 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01o,20sep06,pmr  fix WIND00065553: possibility of losing interrupt.
01n,23apr04,to   stop THR empty interrupt when there are no bytes to send.
01m,03jun04,pch  SPR 74889: fix hardware flow control
01l,03dec03,rcs  fixed warnings for base6
01k,05dec00,dat  merge from sustaining branch to tor2_0_x
01j,20nov00,pai  enabled receiver buffer full interrupt from UART in
		 i8250Startup() (SPR 32443).
01i,06oct99,pai  account for PC87307 hardware anomaly in i8250Int() (SPR 26117).
01h,26oct98,dbt  call i8250InitChannel() in i8250HrdInit() rather than in
		 i8250ModeSet(). (SPR #22349).
01g,23may97,db   added hardware options and modem control(SPR #7542).
		 fixed bugs reported in SPRs 9404, 9223.
01f,12oct95,dgf  added bauds > 38k
01e,03aug95,myz  fixed the warning messages
01d,12jul95,myz  fixed the baud rate problem.
01c,20jun95,ms   fixed comments for mangen.
01b,15jun95,ms   updated for new driver structure
01a,15mar95,myz  written (using i8250Serial.c + the VxMon polled driver).
*/

/*
DESCRIPTION
This is the driver for the Intel 8250 UART Chip used on the PC 386.
It uses the SCCs in asynchronous mode only.


USAGE
An I8250_CHAN structure is used to describe the chip.
The BSP's sysHwInit() routine typically calls sysSerialHwInit()
which initializes all the register values in the I8250_CHAN structure
(except the SIO_DRV_FUNCS) before calling i8250HrdInit().
The BSP's sysHwInit2() routine typically calls sysSerialHwInit2() which
connects the chips interrupt handler (i8250Int) via intConnect().


IOCTL FUNCTIONS
This driver responds to all the same ioctl() codes as a normal serial driver;
for more information, see the comments in sioLib.h.  As initialized, the
available baud rates are 110, 300, 600, 1200, 2400, 4800, 9600, 19200, and
38400.


This driver handles setting of hardware options such as parity(odd, even) and
number of data bits(5, 6, 7, 8). Hardware flow control is provided with the
handshakes RTS/CTS. The function HUPCL(hang up on last close) is available.


INCLUDE FILES: drv/sio/i8250Sio.h
*/

#include "vxWorks.h"
#include "iv.h"
#include "intLib.h"
#include "errnoLib.h"
#include "drv/sio/i8250Sio.h"

/* defines */

/* This value is used to specify the maximum number of times i8250Int()
 * routine will read the 8250 Interrupt Identification Register before
 * returning.  This value should optimally be derived from the BRDR.
 */
#define I8250_IIR_READ_MAX          40

#ifndef I8250_DEFAULT_BAUD
#   define  I8250_DEFAULT_BAUD      9600
#endif

#ifndef SIO_HUP
#   define SIO_OPEN     0x100A
#   define SIO_HUP      0x100B
#endif

/* locals */

/* baudTable is a table of the available baud rates, and the values to write
 * to the UART's baud rate divisor {high, low} register. the formula is
 * 1843200(source) / (16 * baudrate)
 */

static BAUD baudTable [] =
    {
    {50, 2304}, {75, 1536}, {110, 1047}, {134, 857}, {150, 768},
    {300, 384}, {600, 192}, {1200, 96}, {2000, 58}, {2400, 48},
    {3600,32}, {4800, 24}, {7200,16}, {9600, 12}, {19200, 6}, {38400, 3},
    {57600, 2}, {115200, 1}
    };

static SIO_DRV_FUNCS i8250SioDrvFuncs;

/* forward declarations */

LOCAL   void    i8250InitChannel (I8250_CHAN *);
LOCAL   int     i8250Ioctl(I8250_CHAN *, int, int);
LOCAL   int     i8250Startup(I8250_CHAN *pChan);
LOCAL   int     i8250PRxChar (I8250_CHAN *pChan, char *ch);
LOCAL   int     i8250PTxChar(I8250_CHAN *pChan, char ch);
LOCAL   STATUS  i8250OptsSet (I8250_CHAN *, UINT);
LOCAL   STATUS  i8250ModeSet (I8250_CHAN *, UINT);
LOCAL   STATUS  i8250BaudSet (I8250_CHAN *, UINT);

/******************************************************************************
*
* i8250CallbackInstall - install ISR callbacks to get put chars.
*
* This routine installs the callback functions for the driver
*
* RETURNS: OK on success or ENOSYS on unsupported callback type.
*/ 
static int i8250CallbackInstall
    (
    SIO_CHAN *  pSioChan,
    int         callbackType,
    STATUS      (*callback)(),
    void *      callbackArg
    )
    {
    I8250_CHAN * pChan = (I8250_CHAN *)pSioChan;


    switch (callbackType)
	{
	case SIO_CALLBACK_GET_TX_CHAR:
	    pChan->getTxChar    = callback;
	    pChan->getTxArg     = callbackArg;
	    return (OK);
	case SIO_CALLBACK_PUT_RCV_CHAR:
	    pChan->putRcvChar   = callback;
	    pChan->putRcvArg    = callbackArg;
	    return (OK);
	default:
	    return (ENOSYS);
	}
    }

/******************************************************************************
*
* i8250HrdInit - initialize the chip
*
* This routine is called to reset the chip in a quiescent state.
* RETURNS: N/A
*/
void i8250HrdInit
    (
    I8250_CHAN *  pChan         /* pointer to device */
    )
    {
    if (i8250SioDrvFuncs.ioctl == NULL)
	{
	i8250SioDrvFuncs.ioctl          = (int (*)())i8250Ioctl;
	i8250SioDrvFuncs.txStartup      = (int (*)())i8250Startup;
	i8250SioDrvFuncs.callbackInstall = (FUNCPTR)i8250CallbackInstall;
	i8250SioDrvFuncs.pollInput      = (int (*)())i8250PRxChar;
	i8250SioDrvFuncs.pollOutput     = (int (*)(SIO_CHAN *,char))i8250PTxChar;
	}


    pChan->pDrvFuncs = &i8250SioDrvFuncs;


    i8250InitChannel(pChan);    /* reset the channel */
    }

/*******************************************************************************
*
* i8250InitChannel  - initialize a single channel
*/
static void i8250InitChannel
    (
    I8250_CHAN *  pChan         /* pointer to device */
    )
    {
    int oldLevel;

    oldLevel = intLock ();

    /* set channel baud rate */

    (void) i8250BaudSet(pChan, I8250_DEFAULT_BAUD);

    /* 8 data bits, 1 stop bit, no parity */

    (*pChan->outByte) (pChan->lcr, (I8250_LCR_CS8 | I8250_LCR_1_STB));
    (*pChan->outByte) (pChan->mdc, (I8250_MCR_RTS | I8250_MCR_DTR | \
					I8250_MCR_OUT2));

    (*pChan->inByte) (pChan->data);     /* clear the port */

    /* disable interrupts  */

    (*pChan->outByte) (pChan->ier, 0x0);

    pChan->options = (CLOCAL | CREAD | CS8);

    intUnlock (oldLevel);
    }

/*******************************************************************************
*
* i8250Hup - hang up the modem control lines 
*
* Resets the RTS and DTR signals.
*
* RETURNS: OK
*/
LOCAL STATUS i8250Hup
    (
    I8250_CHAN * pChan          /* pointer to channel */
    )
    {
    FAST int     oldlevel;      /* current interrupt level mask */

    oldlevel = intLock ();

    (*pChan->outByte) (pChan->mdc, I8250_MCR_OUT2);

    intUnlock (oldlevel);

    return (OK);
    }    

/*******************************************************************************
*
* i8250Open - Set the modem control lines 
*
* Set the modem control lines(RTS, DTR) TRUE if not already set.  
* It also clears the receiver. 
*
* RETURNS: OK
*/
LOCAL STATUS i8250Open
    (
    I8250_CHAN * pChan          /* pointer to channel */
    )
    {
    FAST int     oldlevel;      /* current interrupt level mask */
    char mask;

    /* read modem control register */

    mask = ((*pChan->inByte) (pChan->mdc)) & (I8250_MCR_RTS | I8250_MCR_DTR);

    if (mask != (I8250_MCR_RTS | I8250_MCR_DTR)) 
	{
	/* RTS and DTR not set yet */

	oldlevel = intLock ();

	/* set RTS and DTR TRUE */

	(*pChan->outByte) (pChan->mdc, (I8250_MCR_RTS | I8250_MCR_DTR | \
					I8250_MCR_OUT2));

	intUnlock (oldlevel);
	}

    return (OK);
    }

/******************************************************************************
*
* i8250BaudSet - change baud rate for channel
*
* This routine sets the baud rate for the UART. The interrupts are disabled
* during chip access.
*
* RETURNS: OK to indicate success, otherwise ERROR is returned
*/
LOCAL STATUS  i8250BaudSet
    (
    I8250_CHAN *   pChan,       /* pointer to channel */
    UINT           baud         /* requested baud rate */
    )
    {
    int         oldlevel;
    STATUS      status;
    FAST        int     ix;
    UINT8       lcr;

    /* disable interrupts during chip access */

    oldlevel = intLock ();

    status = ERROR;

    for (ix = 0; ix < NELEMENTS (baudTable); ix++)
	{
	if (baudTable [ix].rate == baud)        /* lookup baud rate value */
	    {
	    lcr = (*pChan->inByte) (pChan->lcr);   
	    (*pChan->outByte) (pChan->lcr, (char)(I8250_LCR_DLAB | lcr));
	    (*pChan->outByte) (pChan->brdh, MSB (baudTable[ix].preset));
	    (*pChan->outByte) (pChan->brdl, LSB (baudTable[ix].preset));
	    (*pChan->outByte) (pChan->lcr, lcr);
	    status = OK;
	    break;
	    }
	}

    intUnlock(oldlevel);

    return (status);
    }

/*******************************************************************************
*
* i8250ModeSet - change channel mode setting
*
* This driver supports both polled and interrupt modes and is capable of
* switching between modes dynamically. 
*
* If interrupt mode is desired this routine enables the channels receiver and 
* transmitter interrupts. If the modem control option is TRUE, the Tx interrupt
* is disabled if the CTS signal is FALSE. It is enabled otherwise. 
*
* If polled mode is desired the device interrupts are disabled. 
*
* RETURNS:
* Returns a status of OK if the mode was set else ERROR.
*/
LOCAL STATUS i8250ModeSet
    (
    I8250_CHAN * pChan,         /* pointer to channel */
    UINT        newMode         /* mode requested */
    )
    {
    FAST int     oldlevel;      /* current interrupt level mask */
    char ier, mask;

    if ((newMode != SIO_MODE_POLL) && (newMode != SIO_MODE_INT))
	return (ERROR);

    oldlevel = intLock();
	   
    if (newMode == SIO_MODE_POLL)
	ier = 0x00;
    else  
	{
	if (pChan->options & CLOCAL) 
	    ier = I8250_IER_RXRDY;
	else  
	    {
	    mask = ((*pChan->inByte) (pChan->msr)) & I8250_MSR_CTS;

	    /* if the CTS is asserted enable Tx interrupt */

	    if (mask & I8250_MSR_CTS)
		ier = (I8250_IER_TBE | I8250_IER_MSI);
	    else
		ier = (I8250_IER_MSI);
	    }   
	}

    (*pChan->outByte) (pChan->ier, ier);
	    
    pChan->channelMode = newMode;  
	  
    intUnlock(oldlevel);

    return (OK);
    }

/*******************************************************************************
*
* i8250OptsSet - set the serial options
*
* Set the channel operating mode to that specified.  All sioLib options
* are supported: CLOCAL, HUPCL, CREAD, CSIZE, PARENB, and PARODD.
*
* RETURNS:
* Returns OK to indicate success, otherwise ERROR is returned
*/
LOCAL STATUS i8250OptsSet
    (
    I8250_CHAN * pChan,         /* pointer to channel */
    UINT options                /* new hardware options */
    )
    {
    FAST int     oldlevel;              /* current interrupt level mask */
    char lcr = 0; 
    char mcr = I8250_MCR_OUT2;
    char ier;
    char mask;

    ier = (*pChan->inByte) (pChan->ier);

    if (pChan == NULL || options & 0xffffff00)
	return ERROR;

    switch (options & CSIZE)
	{
	case CS5:
	    lcr = I8250_LCR_CS5; break;
	case CS6:
	    lcr = I8250_LCR_CS6; break;
	case CS7:
	    lcr = I8250_LCR_CS7; break;
	default:
	case CS8:
	    lcr = I8250_LCR_CS8; break;
	}

    if (options & STOPB)
	lcr |= I8250_LCR_2_STB;
    else
	lcr |= I8250_LCR_1_STB;

    switch (options & (PARENB | PARODD))
	{
	case PARENB|PARODD:
	    lcr |= I8250_LCR_PEN; break;
	case PARENB:
	    lcr |= (I8250_LCR_PEN | I8250_LCR_EPS); break;
	default:
	case 0:
	    lcr |= I8250_LCR_PDIS; break;
	}

    (*pChan->outByte) (pChan->ier, 0x00);

    if (!(options & CLOCAL))
	{
	/* !clocal enables hardware flow control(DTR/DSR) */

	mcr |= (I8250_MCR_DTR | I8250_MCR_RTS);
	ier |= I8250_IER_MSI;    /* enable modem status interrupt */

	mask = ((*pChan->inByte) (pChan->msr)) & I8250_MSR_CTS;

	/* if the CTS is asserted enable Tx interrupt */

	if (mask & I8250_MSR_CTS)
		ier |= I8250_IER_TBE;
	else
		ier &= (~I8250_IER_TBE); 
	}
    else 
	ier &= ~I8250_IER_MSI; /* disable modem status interrupt */ 

    oldlevel = intLock ();

    (*pChan->outByte) (pChan->lcr, lcr);
    (*pChan->outByte) (pChan->mdc, mcr);

    /* now clear the port */

    (*pChan->inByte) (pChan->data);

    if (options & CREAD)  
	ier |= I8250_IER_RXRDY;

    if (pChan->channelMode == SIO_MODE_INT)
	{
	(*pChan->outByte) (pChan->ier, ier);
	}

    intUnlock (oldlevel);

    pChan->options = options;

    return OK;
    }

/*******************************************************************************
*
* i8250Ioctl - special device control
*
* Includes commands to get/set baud rate, mode(INT, POLL), hardware options(
* parity, number of data bits), and modem control(RTS/CTS and DTR/DSR).
* The ioctl commands SIO_HUP and SIO_OPEN are implemented to provide the 
* HUPCL( hang up on last close) function.
*
* RETURNS: OK on success, EIO on device error, ENOSYS on unsupported
*          request.
*/
static int i8250Ioctl
    (
    I8250_CHAN *  pChan,        /* device to control */
    int request,                /* request code */
    int arg                     /* some argument */
    )
    {
    int ix;
    int baudH;
    int baudL;
    int oldlevel;
    UINT8 lcr;
    int status = OK;

    switch (request)
	{
	case SIO_BAUD_SET:
	    status = (i8250BaudSet(pChan, arg) == OK) ? OK : EIO;
	    break;

	case SIO_BAUD_GET:
	    oldlevel = intLock();

	    status = EIO;

	    lcr = (*pChan->inByte) (pChan->lcr);
	    (*pChan->outByte) (pChan->lcr, (char)(I8250_LCR_DLAB | lcr));
	    baudH = (*pChan->inByte)(pChan->brdh);
	    baudL = (*pChan->inByte)(pChan->brdl);
	    (*pChan->outByte) (pChan->lcr, lcr);

	    for (ix = 0; ix < NELEMENTS (baudTable); ix++)
		{
		if ( baudH  == MSB (baudTable[ix].preset) &&
		     baudL  == LSB (baudTable[ix].preset) )
		    {
		    *(int *)arg = baudTable[ix].rate;
		    status = OK;
		    }
		}

	    intUnlock(oldlevel);
	    break;

	case SIO_MODE_SET:
	    status = (i8250ModeSet (pChan, arg) == OK) ? OK : EIO;
	    break;

	case SIO_MODE_GET:
	    *(int *)arg = pChan->channelMode;
	    return (OK);

	case SIO_AVAIL_MODES_GET:
	    *(int *)arg = SIO_MODE_INT | SIO_MODE_POLL;
	    return (OK);

	case SIO_HW_OPTS_SET:
	    status = (i8250OptsSet (pChan, arg) == OK) ? OK : EIO;
	    break;

	case SIO_HW_OPTS_GET:
	    *(int *)arg = pChan->options;
	    break;

	case SIO_HUP:
	    /* check if hupcl option is enabled */

	    if (pChan->options & HUPCL) 
		status = i8250Hup (pChan);
	    break;

	case SIO_OPEN:
	    /* check if hupcl option is enabled */

	    if (pChan->options & HUPCL) 
		status = i8250Open (pChan);
	    break;

	default:
	    status = ENOSYS;
	    break;
	}

    return (status);
    }

/*******************************************************************************
*
* i8250Int - handle a receiver/transmitter interrupt
*
* This routine handles four sources of interrupts from the UART.  If there is
* another character to be transmitted, the character is sent.  When a modem
* status interrupt occurs, the transmit interrupt is enabled if the CTS signal
* is TRUE.
*
* INTERNAL
* The UART interrupts are prioritized in the following order by the Interrupt
* Identification Register:
*
*     Bit 2    bit 1    bit 0    Priority        Interrupt ID
*     -------------------------------------------------------
*     0        0        1        none            none
*     1        1        0        0               serialization error or BREAK
*     1        0        0        1               received data
*     0        1        0        2               transmitter buffer empty
*     0        0        0        3               RS-232 input
*
* In the table, priority level 0 is the highest priority.  These priorities
* are not typically configurable via software.  The interrupt is expressed as
* a two-bit integer.  Bit 0, the pending bit, is negative logic - a value 0
* means that an interrupt is pending.
*
* When nested interrupts occur, the second interrupt is identified in the
* Interrupt Identification Register (IIR) as soon as the first interrupt is
* cleared. Therefore, the interrupt service routine must continue to read IIR
* until bit-0 is 1.
*
* When the 8250 generates an interrupt, all interrupts with an equal or
* lower priority are locked out until the current interrupt has been cleared.
* The operation required to clear an interrupt varies according to the source.
* The actions typically required to clear an interrupt are as follows:
*
*    Source of interrupt                Response required to reset
*    ---------------------------------------------------------------------
*    receiver error or BREAK            read serialization status register
*    received data                      read data from receiver register
*    transmit buffer empty              write to the transmitter or read
*                                       the interrupt ID register
*    RS-232 input                       read the RS-232 status register
*
* In response to an empty transmit buffer (TBE), the interrupt can be
* cleared by writing a byte to the transmitter buffer register.  It can
* also be cleared by reading the interrupt identification register.
*
* Failure to clear all interrupts before returning will leave an interrupt
* pending and prevent the UART from generating further interrupt requests
* to the CPU.
*
* RETURNS: N/A
*/
void i8250Int
    (
    I8250_CHAN  *  pChan
    )
    {
    char outChar;       /* store a byte for transmission */
    char interruptID;   /* store contents of interrupt ID register */
    char lineStatus;    /* store contents of line status register */
    char ier;           /* store contents of interrupt enable register */
    int  ix = 0;        /* allows us to return just in case IP never clears */

    ier = (*pChan->inByte) (pChan->ier);

    /* service UART interrupts until IP bit set or read counter expires */

    while (ix++ < I8250_IIR_READ_MAX)
	{
	interruptID = ((*pChan->inByte)(pChan->iid) & I8250_IIR_MASK);

	if (interruptID == I8250_IIR_IP)
	    {
	    break;  /* interrupt cleared */ 
	    }

	/* filter possible anomalous interrupt ID from PC87307VUL (SPR 26117) */

	interruptID &= 0x06;  /* clear odd-bit to find interrupt ID */

	if (interruptID == I8250_IIR_SEOB)
	    {
	    lineStatus = (*pChan->inByte) (pChan->lst);
	    }
	else if (interruptID == I8250_IIR_RBRF)
	    {
	    if (pChan->putRcvChar != NULL)
		(*pChan->putRcvChar)(pChan->putRcvArg,
			       (*pChan->inByte) (pChan->data));
	    else
		(*pChan->inByte) (pChan->data);
	    }
	else if (interruptID == I8250_IIR_THRE)
	    {
	    if ((pChan->getTxChar != NULL) &&
		(*pChan->getTxChar) (pChan->getTxArg, &outChar) == OK)
		{
		(*pChan->outByte) (pChan->data, outChar);
		}
	    else
	        {
		/*
		 * There are no bytes available for transmission.  Reading
		 * the IIR at the top of this loop will clear the interrupt.
		 */
		/* stop THR empty interrupt */
		ier = (*pChan->inByte) (pChan->ier);
		ier &= ~(I8250_IER_TBE);
		(*pChan->outByte) (pChan->ier, ier);
		}

	    }
	else if (interruptID == I8250_IIR_MSTAT) /* modem status register */
	    {
	    char msr = (*(pChan)->inByte) (pChan->msr);

	    /* (|=) ... DON'T CLOBBER BITS ALREADY SET IN THE IER */

	    ier |= (I8250_IER_RXRDY | I8250_IER_MSI);

#define	SPR_74889_FIX
#ifdef	SPR_74889_FIX
	    /*
	     * If the CTS line changed, enable or
	     * disable tx interrupt accordingly.
	     */
	    if ( msr & I8250_MSR_DCTS )
		{
		if ( msr & I8250_MSR_CTS )
		    {
		    /* CTS was turned on */
		    (*pChan->outByte) (pChan->ier, (I8250_IER_TBE | ier));
		    }
		else
		    {
		    /* CTS was turned off */
		    (*pChan->outByte) (pChan->ier, (ier & (~I8250_IER_TBE)));
		    }
		}
#else	/* SPR_74889_FIX */
	    /* if CTS is asserted by modem, enable tx interrupt */

	    if ((msr & I8250_MSR_CTS) && (msr & I8250_MSR_DCTS))
		{
		(*pChan->outByte) (pChan->ier, (I8250_IER_TBE | ier));
		}
	    else  /* turn off TBE interrupt until CTS from modem */
		{
		(*pChan->outByte) (pChan->ier, (ier & (~I8250_IER_TBE)));
		}
#endif	/* SPR_74889_FIX */
	    }

	}  /* while */
    }

/*******************************************************************************
*
* i8250Startup - transmitter startup routine
*
* Call interrupt level character output routine and enable interrupt if it is
* in interrupt mode with no hardware flow control.
* If the option for hardware flow control is enabled and CTS is set TRUE,
* then the Tx interrupt is enabled.
*
* RETURNS: OK
*/
LOCAL int i8250Startup
    (
    I8250_CHAN *  pChan         /* tty device to start up */
    )
    {
    char  ier = I8250_IER_RXRDY;
    char  mask;

    if (pChan->channelMode == SIO_MODE_INT)
	{
	if (pChan->options & CLOCAL)
	    {
	    /* No modem control */

	    ier |= I8250_IER_TBE;
	    }
	else
	    {
	    mask = ((*pChan->inByte) (pChan->msr)) & I8250_MSR_CTS;

	    /* if the CTS is asserted enable Tx interrupt */

	    if (mask & I8250_MSR_CTS)
		ier |= (I8250_IER_TBE | I8250_IER_MSI);
	    else
		ier |= (I8250_IER_MSI);
	    }

	(*pChan->outByte) (pChan->ier, ier);
	}

    return (OK);
    }

/******************************************************************************
*
* i8250PRxChar - poll the device for input.
*
* RETURNS: OK if a character arrived, EIO on device error, EAGAIN
* if the input buffer if empty.
*/
LOCAL int i8250PRxChar
    (
    I8250_CHAN *    pChan,      /* pointer to channel */
    char *          pChar       /* pointer to char */
    )
    {
    char pollStatus;

    pollStatus = ( *(pChan)->inByte) (pChan->lst);

    if ((pollStatus & I8250_LSR_RXRDY) == 0x00)
	return (EAGAIN);

    /* got a character */

    *pChar = ( *(pChan)->inByte) (pChan->data);

    return (OK);
    }

/******************************************************************************
*
* i8250PTxChar - output a character in polled mode.
* 
* Checks if the transmitter is empty. If empty, a character is written to
* the data register. 
*
* If the hardware flow control is enabled the handshake signal CTS has to be
* asserted in order to send a character.
*
* RETURNS: OK if a character arrived, EIO on device error, EAGAIN
* if the output buffer if full.
*/
LOCAL int i8250PTxChar
    (
    I8250_CHAN *  pChan,        /* pointer to channel */
    char          outChar       /* char to send */
    )
    {
    char pollStatus;
    char msr;

    pollStatus = ( *(pChan)->inByte) (pChan->lst);

    msr = (*(pChan)->inByte) (pChan->msr);

    /* is the transmitter ready to accept a character? */

    if ((pollStatus & I8250_LSR_TEMT) == 0x00)
	return (EAGAIN);

    if (!(pChan->options & CLOCAL))      /* modem flow control ? */
	{
	if (msr & I8250_MSR_CTS)
	    (*(pChan)->outByte) ((pChan)->data, outChar);
	else
	    return (EAGAIN);
	}
    else
	(*(pChan)->outByte) ((pChan)->data, outChar);

    return (OK);
    }
