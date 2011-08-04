/* w83553PciIbc.h - Winbond W83C553F System I/O Controller with PCI
 *                     Register definitions */

/* Copyright 1984-1999 Wind River Systems, Inc. */
/* Copyright 1996, 1998 Motorola, Inc. */

/*
modification history
--------------------
01a,10oct99,mtl   written from SPS/Motorola & yk 750 by teamF1
*/

#ifndef	__INCw83553PciIbch
#define	__INCw83553PciIbch

#ifdef __cplusplus
extern "C" {
#endif

/* structure */

#define  WB_MAX_IRQS      	16

/*  Hardware access methods */

#ifndef  PIC_BYTE_OUT

/*   Write byte to the PIC register  */

#define  PIC_BYTE_OUT(reg,data)  \
    (sysOutByte (reg + PCI_MSTR_ISA_IO_LOCAL,data))

#endif	/* PIC_BYTE_OUT */

#ifndef	PIC_BYTE_IN
    
/*   Read byte from the PIC register  */

#define  PIC_BYTE_IN(reg,pData)  \
    (*pData = sysInByte (reg+PCI_MSTR_ISA_IO_LOCAL))

#endif	/* PIC_BYTE_IN */

#define  MEM_READ(reg,pData)  (*pData = sysInByte (reg))
#define  MEM_WRITE(reg,data)  (sysOutByte (reg,data))

/*  Programmable Interrupt Controller Registers  */

#define PIC1_ICW1		0x20	  /* Initialization write only */
#define PIC2_ICW1		0xA0	  /* Initialization write only */

#define PIC1_OCW2		0x20	  /* Operation write only */
#define PIC2_OCW2		0xA0	  /* Operation write only */

#define PIC1_OCW3		0x20	  /* Operation */
#define PIC2_OCW3		0xA0	  /* Operation */

#define PIC1_ICW2		0x21	  /* Initialization write only */
#define PIC2_ICW2		0xA1	  /* Initialization write only */

#define PIC1_ICW3		0x21	  /* Initialization write only */
#define PIC2_ICW3		0xA1	  /* Initialization write only */

#define PIC1_ICW4		0x21	  /* Initialization write only  */
#define PIC2_ICW4		0xA1	  /* Initialization write only  */

#define PIC1_OCW1		0x21	  /* Operation */
#define PIC2_OCW1		0xA1	  /* Operation */

#define PIC1_IACK		0x00	  /* port 1 base address */
#define PIC2_IACK		0x01	  /* port 2 base address */

/*  These are mapped to ISA I/O Space  */

#define W83553_INT1_CTRL	0x0020 /* PIC1 */
#define W83553_INT1_MASK	0x0021
#define W83553_INT1_ECLR	0x04d0
#define W83553_INT2_CTRL	0x00a0 /* PIC2 */
#define W83553_INT2_MASK	0x00a1
#define W83553_INT2_ECLR	0x04d1
#define W83553_TRM1_CNT0	0x0040
#define W83553_TRM1_CNT1	0x0041
#define W83553_TRM1_CNT2	0x0042
#define W83553_TRM1_CMOD	0x0043
#define W83553_RST_IRQ12	0x0060
#define W83553_NMI_SCTRL	0x0061
#define W83553_PCOP		0x0c04
#define W83553_TMCP		0x0c01

IMPORT STATUS  	sysPciIbcIntrInit (int winBondIrq);
    
#ifdef __cplusplus
}
#endif

#endif	/* __INCw83553PciIbch */

