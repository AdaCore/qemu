/* sysSerial.c -  Yellowknife serial device initialization */

/* Copyright 1984-1999 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,01apr02,jnz  added support for 8245/8241 on chip duart
01a,29apr99,jkf  merged into T2.
01a,01apr99,jkf  created based from mcp750 BSP sysSerial.c
*/

/*
DESCRIPTION
This file contains the board-specific routines for serial channel
initialization.

*/

#include "vxWorks.h"
#include "config.h"
#include "intLib.h"
#include "iv.h"
#include "sysLib.h"
#include "config.h"
#include "version.h"

#if defined (_WRS_VXWORKS_MAJOR) && (_WRS_VXWORKS_MAJOR >= 6) && (_WRS_VXWORKS_MINOR >= 5)
#include "drv/sio/i8250Sio.h"		/* TODO use i8250Sio driver */
#else
#include "i8250Sio.h"
#include "i8250Sio.c"
#endif

/* device INIT structs */

typedef struct
    {
    USHORT vector;			/* Interrupt vector */
    ULONG  baseAdrs;			/* Register base address */
    USHORT regSpace;			/* Address Interval */
    USHORT intLevel;			/* Interrupt level */
    } I8250_CHAN_PARAS;

/* static variables */

static I8250_CHAN  i8250Chan[N_UART_CHANNELS];

static I8250_CHAN_PARAS devParas[] = 
    {
       {COM1_INT_VEC, 0, UART_REG_ADDR_INTERVAL, COM1_INT_LVL},
       {COM2_INT_VEC, 0, UART_REG_ADDR_INTERVAL, COM2_INT_LVL}
    };

/*
 * Array of pointers to all serial channels configured in system.
 * See sioChanGet(). It is this array that maps channel pointers
 * to standard device names.  The first entry will become "/tyCo/0",
 * the second "/tyCo/1", and so forth.
 *
 */

SIO_CHAN * sysSioChans [N_SIO_CHANNELS] =
    {
    (SIO_CHAN *)&i8250Chan[0].pDrvFuncs,	/* /tyCo/0 */
    (SIO_CHAN *)&i8250Chan[1].pDrvFuncs,	/* /tyCo/1 */
    };

/* definitions */

#define UART_REG(reg,chan) \
		(devParas[chan].baseAdrs + reg * devParas[chan].regSpace)


/******************************************************************************
*
* sysSerialHwInit - initialize the BSP serial devices to a quiescent state
*
* This routine initializes the BSP serial device descriptors and puts the
* devices in a quiescent state.  It is called from sysHwInit() with
* interrupts locked.   Polled mode serial operations are possible, but not
* interrupt mode operations which are enabled by sysSerialHwInit2().
*
* RETURNS: N/A
*
* SEE ALSO: sysHwInit()
*/ 

void sysSerialHwInit (void)
    {
    int i;

    devParas[0].baseAdrs = COM1_BASE_ADR_DYN;
    devParas[1].baseAdrs = COM2_BASE_ADR_DYN;

    for (i = 0; i < N_UART_CHANNELS; i++)
        {
	i8250Chan[i].int_vec	 = devParas[i].vector;
	i8250Chan[i].channelMode = 0;
	i8250Chan[i].lcr	 = UART_REG(UART_LCR, i);
	i8250Chan[i].data	 = UART_REG(UART_RDR, i);
	i8250Chan[i].brdl	 = UART_REG(UART_BRDL, i);
	i8250Chan[i].brdh	 = UART_REG(UART_BRDH, i);
	i8250Chan[i].ier	 = UART_REG(UART_IER, i);
	i8250Chan[i].iid	 = UART_REG(UART_IID, i);
	i8250Chan[i].mdc	 = UART_REG(UART_MDC, i);
	i8250Chan[i].lst	 = UART_REG(UART_LST, i);
	i8250Chan[i].msr	 = UART_REG(UART_MSR, i);
	i8250Chan[i].outByte	 = (void (*) (int, char))  sysOutByte;
	i8250Chan[i].inByte	 = (UINT8 (*) ()) sysInByte;
	i8250HrdInit (&i8250Chan[i]);
        }
    }

/******************************************************************************
*
* sysSerialHwInit2 - connect BSP serial device interrupts
*
* This routine connects the BSP serial device interrupts.  It is called from
* sysHwInit2().  
*
* Serial device interrupts cannot be connected in sysSerialHwInit() because
* the  kernel memory allocator is not initialized at that point and
* intConnect() calls malloc().
*
* RETURNS: N/A
*
* SEE ALSO: sysHwInit2()
*/ 

void sysSerialHwInit2 (void)
    {
    int i;

    /* connect serial interrupts */

     for (i = 0; i < N_UART_CHANNELS; i++)
         if (i8250Chan[i].int_vec)
	     {
             (void) intConnect (INUM_TO_IVEC ((int)i8250Chan[i].int_vec),
				i8250Int, (int)&i8250Chan[i] );

             intEnable (devParas[i].intLevel); 
             }
    }

/******************************************************************************
*
* sysSerialChanGet - get the SIO_CHAN device associated with a serial channel
*
* This routine returns a pointer to the SIO_CHAN device associated
* with a specified serial channel.  It is called by usrRoot() to obtain 
* pointers when creating the system serial devices, `/tyCo/x'.  It
* is also used by the WDB agent to locate its serial channel.
*
* RETURNS: A pointer to the SIO_CHAN structure for the channel, or ERROR
* if the channel is invalid.
*/

SIO_CHAN * sysSerialChanGet
    (
    int channel         /* serial channel */
    )
    {

    if (channel < 0
     || channel >= NELEMENTS(sysSioChans) )
	return (SIO_CHAN *) ERROR;

    return sysSioChans[channel];
    }
