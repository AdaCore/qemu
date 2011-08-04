/* i8250Sio.h - header file for binary interface Intel 8250 UART driver */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,03jun98,hdn  changed I8250_IIR_MASK from 6 to 7 to fix the TX-int problem.
01c,22may97,db	 added macros for all i8250 registers(SPR #7542).
01b,15jun95,ms   updated for new driver.
01a,15mar95,myz  written (from i8250Serial.h)
*/

#ifndef	__INCi8250Sioh
#define	__INCi8250Sioh

#ifdef __cplusplus
extern "C" {
#endif


#ifndef _ASMLANGUAGE

#include "vxWorks.h"
#include "sioLib.h"

/* channel data structure  */

typedef struct 		/* I8250_CHAN */
    {
    SIO_DRV_FUNCS * pDrvFuncs;       /* driver functions */

    STATUS      (*getTxChar) ();     /* pointer to xmitr function */
    STATUS      (*putRcvChar) ();    /* pointer tp rcvr function */
    void *      getTxArg;
    void *      putRcvArg;

    UINT16  int_vec;                 /* interrupt vector number */
    UINT16  channelMode;             /* SIO_MODE_[INT | POLL] */
    UCHAR   (*inByte) (int);         /* routine to read a byte from register */
    void    (*outByte)(int,char);    /* routine to write a byte to register */

    ULONG   lcr;                     /* UART line control register */
    ULONG   lst;                     /* UART line status register */
    ULONG   mdc;                     /* UART modem control register */
    ULONG   msr;                     /* UART modem status register */
    ULONG   ier;                     /* UART interrupt enable register */
    ULONG   iid;                     /* UART interrupt ID register */
    ULONG   brdl;                    /* UART baud rate register */
    ULONG   brdh;                    /* UART baud rate register */
    ULONG   data;                    /* UART data register */
    ULONG   options;		     /* UART hardware options */
    } I8250_CHAN;



typedef struct			/* BAUD */
    {
    int rate;		/* a baud rate */
    int preset;		/* counter preset value to write to DUSCC_CTPR[HL]A */
    } BAUD;

/* register definitions */

#define UART_THR        0x00	/* Transmitter holding reg. */
#define UART_RDR        0x00	/* Receiver data reg.       */
#define UART_BRDL       0x00	/* Baud rate divisor (LSB)  */
#define UART_BRDH       0x01	/* Baud rate divisor (MSB)  */
#define UART_IER        0x01	/* Interrupt enable reg.    */
#define UART_IID        0x02	/* Interrupt ID reg.        */
#define UART_LCR        0x03	/* Line control reg.        */
#define UART_MDC        0x04	/* Modem control reg.       */
#define UART_LST        0x05	/* Line status reg.         */
#define UART_MSR        0x06	/* Modem status reg.        */

/* equates for interrupt enable register */

#define I8250_IER_RXRDY	0x01	/* receiver data ready */
#define I8250_IER_TBE	0x02	/* transmit bit enable */
#define I8250_IER_LST	0x04	/* line status interrupts */
#define I8250_IER_MSI	0x08	/* modem status interrupts */

/* equates for interrupt identification register */

#define I8250_IIR_IP	0x01	/* interrupt pending bit */
#define I8250_IIR_MASK	0x07	/* interrupt id bits mask */
#define I8250_IIR_MSTAT  0x00	/* modem status interrupt */
#define I8250_IIR_THRE	0X02	/* transmit holding register empty */
#define I8250_IIR_RBRF	0x04	/* receiver buffer register full */
#define I8250_IIR_SEOB	0x06	/* serialization error or break */

/* equates for line control register */

#define I8250_LCR_CS5	0x00	/* 5 bits data size */
#define I8250_LCR_CS6	0x01	/* 6 bits data size */
#define I8250_LCR_CS7	0x02	/* 7 bits data size */
#define I8250_LCR_CS8	0x03	/* 8 bits data size */
#define I8250_LCR_2_STB  0x04	/* 2 stop bits */
#define I8250_LCR_1_STB	0x00	/* 1 stop bit */
#define I8250_LCR_PEN	0x08	/* parity enable */
#define I8250_LCR_PDIS   0x00	/* parity disable */
#define I8250_LCR_EPS	0x10	/* even parity slect */
#define I8250_LCR_SP	0x20	/* stick parity select */
#define I8250_LCR_SBRK	0x40	/* break control bit */
#define I8250_LCR_DLAB	0x80	/* divisor latch access enable */

/* equates for the modem control register */

#define I8250_MCR_DTR	0x01	/* dtr output */
#define I8250_MCR_RTS	0x02	/* rts output */
#define I8250_MCR_OUT1	0x04	/* output #1 */
#define I8250_MCR_OUT2	0x08	/* output #2 */
#define I8250_MCR_LOOP	0x10	/* loop back */
 
/* equates for line status register */

#define I8250_LSR_RXRDY	0x01	/* receiver data available */
#define I8250_LSR_OE	0x02	/* overrun error */
#define I8250_LSR_PE	0x04	/* parity error */
#define I8250_LSR_FE	0x08	/* framing error */
#define I8250_LSR_BI	0x10	/* break interrupt */
#define I8250_LSR_THRE	0x20	/* transmit holding register empty */
#define I8250_LSR_TEMT	0x40	/* transmitter empty */

/* equates for modem status register */

#define I8250_MSR_DCTS	0x01	/* cts change */
#define I8250_MSR_DDSR	0x02	/* dsr change */
#define I8250_MSR_DRI	0x04	/* ring change */
#define I8250_MSR_DDCD	0x08	/* data carrier change */
#define I8250_MSR_CTS	0x10	/* complement of cts */
#define I8250_MSR_DSR	0x20	/* complement of dsr */
#define I8250_MSR_RI	0x40	/* complement of ring signal */
#define I8250_MSR_DCD	0x80	/* complement of dcd */


#if defined(__STDC__) || defined(__cplusplus)

extern void i8250HrdInit(I8250_CHAN *pDev);
extern void i8250Int (I8250_CHAN  *pDev);

#else

extern void i8250HrdInit();
extern void i8250Int();
	     
#endif  /* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif	/* __INCi8250h */
