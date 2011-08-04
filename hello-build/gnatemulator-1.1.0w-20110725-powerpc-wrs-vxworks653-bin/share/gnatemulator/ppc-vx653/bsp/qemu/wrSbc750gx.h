/* wrSbc750gx.h - Sbc750gx board header file */

/* 
 * Copyright (c) 2003-2007 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01f,02mar07,pfl  WIND00089602 - added DEFAULT_DEC_CLK_FREQ
01e,17jun05,foo  updated for fast warm restart
01d,19jan05,rlp  Removed the INCLUDE_MMU_FULL directive.
01c,07jul04,kp   modified for SBC750GX
01c,30jun04,sfs  removed calls to abs()
01b,18mar03,kp   teamF1, defined macros which are under the macro
                 TORNADO_AE_653, to make romInit.s and sysAlib.s common for
                 the both AE653 and Tornado-221 BSPs.
01a,19nov03,kp   created by teamF1.
*/

/*
DESCRIPTION

This file contains I/O addresses and related constants for the SBC7457 board.
*/

#ifndef	__INCwrSbc750gxh
#define	__INCwrSbc750gxh
#ifdef __cplusplus
extern "C" {
#endif


#ifdef TORNADO_AE_653 /* defined in AE Makefile as
                       * EXTRA_DEFINE = -DTORNADO_AE_653
                       */

/* the following macros are not available in AE */
    
#ifndef _WRS_ASM
#define _WRS_ASM(x)		__asm__ volatile (x)
#endif /*  _WRS_ASM */

#ifndef WRS_ASM
#define WRS_ASM(x)		_WRS_ASM(x)
#endif /* WRS_ASM */

#ifndef FUNC_EXPORT
#define FUNC_EXPORT(x)		.globl x
#endif /* FUNC_EXPORT */

#ifndef FUNC_IMPORT
#define FUNC_IMPORT(x)		.extern x
#endif /* FUNC_IMPORT */

#ifndef FUNC_BEGIN
#define FUNC_BEGIN(x)		x:
#endif /* FUNC_BEGIN */

#ifndef FUNC_END
#define FUNC_END(x)
#endif /* FUNC_END */
 
#define PPC_EIEIO_SYNC WRS_ASM (" eieio")

#endif /* TORNADO_AE_653 */
    
/* Local I/O address map */

#define SDRAM_BASE_ADRS		   (0x00000000) /* SDRM start addr */
#define SDRAM_SIZE                 (0x20000000)	/* 512MB RAM */
#define FLASH_BASE_ADRS 	   (0xFF000000) /* Flash (SODIMM) */
#define FLASH_MEM_SIZE	           (0x01000000) /* Main flash size 16 MB*/

/* Timer constants */

#define	SYS_CLK_RATE_MIN  	   3   	        /* min system clock rate */
#define	SYS_CLK_RATE_MAX  	   100	 	/* max system clock rate */
#define	AUX_CLK_RATE_MIN  	   3    	/* min auxiliary clock rate */
#define	AUX_CLK_RATE_MAX  	   100	 	/* max auxiliary clock rate */

#define INCLUDE_CACHE_L2
 
/* create a single macro INCLUDE_MMU */

#define INCLUDE_MMU

/* Only one can be selected, FULL overrides BASIC */

#undef INCLUDE_MMU_BASIC

/* bus frequency */
#define BUS_FREQUENCY_100_MHZ     /* default bus frequency is 100 MHz */
    
/* decrementer constants */
#ifdef  BUS_FREQUENCY_100_MHZ        /* bus frequency is 100 Mhz */

#define DEC_CLOCK_FREQ            100000000    /* BUS frequency */
#define TCLK_FREQ                 100000000    /* 100MHz bus */    

#else   /* bus frequency is 133 Mhz */    

#define DEC_CLOCK_FREQ            133000000    /* BUS frequency */
#define TCLK_FREQ                 133000000    /* 133MHz bus */
    
#endif /* BUS_FREQUENCY_100_MHZ */

#define DEFAULT_DEC_CLK_FREQ 	  DEC_CLOCK_FREQ
#define DEC_CLK_TO_INC	          4            /* clocks per increment */    
    
#define LOCAL_ABS(i)		  ((int)(i) >= 0 ? (int)(i) : -(int)(i))
#define DELTA(a,b)		  (((int)(a) > (int)(b)) ? \
				   ((int)(a) - (int)(b)) : \
				   ((int)(b) - (int)(a)))
    
#define CPU_TYPE	 (vxPvrGet() & 0x00ffff00)  /* IBM PPC specific */  
#define CPU_TYPE_750GX   0x00020100  /* 0x700201r0 - revision 0 ,
                                      * 0x700201r1 - revision 1
                                      */    
/* nvram def */
#define NV_RAM_SIZE		0x1FFF		/* 8Kb */	
#define NV_RAM_ADRS		0x0		/* this is an offset */
#define NV_RAM_INTRVL		0x00000001
#define NV_RAM_READ(x)		sysNvRead(x)	/* sysLib.c */
#define NV_RAM_WRITE(x,y)	sysNvWrite(x,y)	/* sysLib.c */
#define NVRAM_BASE_ADDR		0x30000000
#undef  NV_BOOT_OFFSET
#define NV_BOOT_OFFSET		0
#define NV_ENET0_OFFSET		0x0200 /* Offset of Ethernet HW adrs from the
                                         boot offset */
#define NV_ENET1_OFFSET		0x0300 /* Offset of Ethernet HW adrs from
                                          the boot offset */    
#define NV_ENET2_OFFSET		0x0400 /* Offset of Ethernet HW adrs from
                                          the boot offset */

#ifndef	_ASMLANGUAGE
UINT8 sysNvRead(ULONG);
void sysNvWrite (ULONG, UCHAR);
#endif /*_ASMLANGUAGE */

#define PCI_MSTR_ISA_IO_LOCAL	0x80000000  	/* CPU PCI/ISA IO */
#define PCI_MSTR_IACK_LOCAL	0xbffffff0   	/* CPU to PCI IACK */


#define INT_VEC_IRQ0		0x0
#define INT_NUM_IRQ0            INT_VEC_IRQ0
#define EXT_INTERRUPT_BASE 	INT_NUM_IRQ0

#define N_UART_CHANNELS 	2       	/* 2 serial ports  */
#define N_SIO_CHANNELS  	N_UART_CHANNELS	/* 2 serial I/O channels */

#define COM1_INT_LVL    (0x04 + INT_NUM_IRQ0)   /* com1 interrupt level  */
#define COM2_INT_LVL    (0x03 + INT_NUM_IRQ0)   /* com2 interrupt level  */
#define COM1_INT_VEC    ((COM1_INT_LVL-INT_NUM_IRQ0) + INT_VEC_IRQ0)
#define COM2_INT_VEC    ((COM2_INT_LVL-INT_NUM_IRQ0) + INT_VEC_IRQ0)
#define UART_REG_ADDR_INTERVAL  1    /* distance between ports */

/* COM (serial) PORT DEFINITIONS */

#define COM1_ADR        	0x03f8		/* Serial port com1 */
#define COM2_ADR        	0x02f8		/* Serial port com2 */

/* These are dynamically setup for MAP A or MAP B */

#define COM1_BASE_ADR_DYN	(PCI_MSTR_ISA_IO_LOCAL + COM1_ADR)
#define COM2_BASE_ADR_DYN	(PCI_MSTR_ISA_IO_LOCAL + COM2_ADR)

/* NE2000  */
#define IO_ADRS_ENE (PCI_MSTR_ISA_IO_LOCAL + 0x300)
#define INT_LVL_ENE 9

/* IDE.  */
#define IDE_CNTRLR0_INT_LVL 13

#ifdef __cplusplus
}
#endif

#endif	/* __INCwrSbc750gxh */						   


    
