/* prjConfig.c - dynamically generated configuration file */


/*
GENERATED: Thu Aug 04 12:09:57 +0200 2011
DO NOT EDIT - file is regenerated whenever the project changes.
This file contains the non-BSP system initialization code
for Create a bootable VxWorks image (custom configured).
*/


/* includes */

#include "vxWorks.h"
#include "config.h"
#include "apex/portMonitorLib.h"
#include "arch/ppc/vxPpcLib.h"
#include "bootLib.h"
#include "bsdSockLib.h"
#include "bufLib.h"
#include "cacheLib.h"
#include "classLib.h"
#include "configNet.h"
#include "drv/sio/ns16552Sio.h"
#include "drv/timer/ppcDecTimer.h"
#include "drv/timer/timerDev.h"
#include "drv/timer/timestampDev.h"
#include "drv/wdb/wdbEndPktDrv.h"
#include "drv/wdb/wdbVioDrv.h"
#include "end.h"
#include "envLib.h"
#include "etherMultiLib.h"
#include "excLib.h"
#include "fioLib.h"
#include "ftpLib.h"
#include "hostLib.h"
#include "ifLib.h"
#include "in.h"
#include "inetLib.h"
#include "intLib.h"
#include "ioLib.h"
#include "iosLib.h"
#include "ipProto.h"
#include "linkPathLib.h"
#include "logLib.h"
#include "ltLib.h"
#include "math.h"
#include "memAttrLib.h"
#include "memLib.h"
#include "memPartLib.h"
#include "mgrLib.h"
#include "miiLib.h"
#include "msgQLib.h"
#include "muxLib.h"
#include "muxTkLib.h"
#include "net/inet.h"
#include "net/mbuf.h"
#include "netBufLib.h"
#include "netDrv.h"
#include "netLib.h"
#include "netinet/if_ether.h"
#include "netinet/ppp/random.h"
#include "pdLib.h"
#include "pgPoolLib.h"
#include "pgPoolLstLib.h"
#include "pipeDrv.h"
#include "ppsSchedLib.h"
#include "private/arincSchedLibP.h"
#include "private/ftpLibP.h"
#include "private/funcBindP.h"
#include "private/kernelLibP.h"
#include "private/ltLibP.h"
#include "private/objLibP.h"
#include "private/offsetsP.h"
#include "private/partitionLibP.h"
#include "private/payloadLibP.h"
#include "private/pdLibP.h"
#include "private/sigLibP.h"
#include "private/taskLibP.h"
#include "private/valSysCallLib.h"
#include "private/workQLibP.h"
#include "ptyDrv.h"
#include "qPriBMapLib.h"
#include "rebootLib.h"
#include "remLib.h"
#include "routeLib.h"
#include "sdLib.h"
#include "selectLib.h"
#include "semLib.h"
#include "slLib.h"
#include "speLib.h"
#include "stdio.h"
#include "string.h"
#include "sys/socket.h"
#include "sysLib.h"
#include "taskHookLib.h"
#include "taskLib.h"
#include "taskVarLib.h"
#include "tickLib.h"
#include "time.h"
#include "timeMonitorLib.h"
#include "ttyLib.h"
#include "usrConfig.h"
#include "val.h"
#include "version.h"
#include "vmLib.h"
#include "vxLib.h"
#include "wdLib.h"
#include "wdb/wdb.h"
#include "wdb/wdbBpLib.h"
#include "wdb/wdbCommIfLib.h"
#include "wdb/wdbLib.h"
#include "wdb/wdbLibP.h"
#include "wdb/wdbMbufLib.h"
#include "wdb/wdbRpcLib.h"
#include "wdb/wdbRtIfLib.h"
#include "wdb/wdbSvcLib.h"
#include "wdb/wdbUdpLib.h"


/* prototypes */

STATUS timeMonitorLibInit2(BOOL enable);
void usrArincSchedulerInit (BOOL validateSchedules, int maxJitter); STATUS ppsSchedulerInit (UINT32 activationWindow);
void usrDebugCoreInit (void);
void usrIosCoreInit (void);
void usrIosExtraCoreInit (void);
void usrKernelBasicInit (void);
void usrKernelBasicInit2 (int sapHeaderSize, int payloadAlignment, BOOL idapDmaCopyReqd);
void usrKernelFullInit(void);
void usrPartitionInit(void);
void usrPostKernelCoreInit (void);
void usrPostKernelCoreInit2 (void);
void usrPreKernelCoreInit (void);
void usrSystemStartInit (int startType);


/* BSP_STUBS */



/* configlettes */

#include "sysComms.c"
#include "net/usrBootLine.c"
#include "net/usrBsdSocket.c"
#include "net/usrEndLib.c"
#include "net/usrMuxAddrRes.c"
#include "net/usrNetBoot.c"
#include "net/usrNetBootUtil.c"
#include "net/usrNetConfig.c"
#include "net/usrNetConfigIf.c"
#include "net/usrNetEndBoot.c"
#include "net/usrNetHostCfg.c"
#include "net/usrNetIcmp.c"
#include "net/usrNetIpLib.c"
#include "net/usrNetLib.c"
#include "net/usrNetLoopbackStart.c"
#include "net/usrNetMuxCfg.c"
#include "net/usrNetRemoteCfg.c"
#include "net/usrTcp.c"
#include "net/usrUdp.c"
#include "sio/ns16550Sio.c"
#include "timer/ppcDecTimer.c"
#include "usrArincScheduler.c"
#include "usrDebugCore.c"
#include "usrIosCore.c"
#include "usrIosExtraCore.c"
#include "usrKernelBasic.c"
#include "usrKernelCore.c"
#include "usrKernelFull.c"
#include "usrPartitionInit.c"
#include "usrSpe.c"
#include "usrWdbCore.c"
#include "wdbEnd.c"


/******************************************************************************
*
* usrInit - pre-kernel initialization
*/

void usrInit (int startType)
    {
    usrSystemStartInit (startType);     /* basic operations before starting the Wind kernel */
    usrPreKernelCoreInit ();            /* initialize various core features before the kernel initialization */
    }



/******************************************************************************
*
* usrIosExtraInit - extended I/O system
*/

void usrIosExtraInit (void)
    {
    usrIosExtraCoreInit ();             /* Initialize the core extended I/O system. */
    }



/******************************************************************************
*
* usrKernelExtraInit - extended kernel facilities
*/

void usrKernelExtraInit (void)
    {
    usrKernelFullInit ();               /* full kernel support */
    usrSpeInit ();                      /* Spe register save/restore routines */
    }



/******************************************************************************
*
* usrNetProtoInit - Initialize the network protocol stacks
*/

void usrNetProtoInit (void)
    {
    usrBsdSockLibInit();                /* BSD Socket Support */
    hostTblInit();                      /* Host Table Support */
    usrIpLibInit();                     /* BSD 4.4 IPv4 */
    udpLibInit (&udpCfgParams);         /* BSD 4.4. UDPv4 */
    tcpLibInit (&tcpCfgParams);         /* BSD 4.4 TCPv4 */
    icmpLibInit (&icmpCfgParams);       /* BSD 4.4 ICMPv4 */
    igmpLibInit();                      /* BSD 4.4 IGMPv1 */
    netTaskInit();                      /* Support for the network task and job queue. */
    netLibInit();                       /* creates the network task that runs low-level network interface routines in a task context */
                                        /* Includes the ARP cache APIs */
    }



/******************************************************************************
*
* usrNetRemoteInit - 
*/

void usrNetRemoteInit (void)
    {
    usrNetHostSetup ();                 /* Route creation and hostname setup */
    remLibInit(RSH_STDERR_SETUP_TIMEOUT); /* Remote command library */
    usrNetRemoteCreate ();              /* Allows access to file system on boot host */
    }



/******************************************************************************
*
* usrNetworkDevStart - Attach a network device and start the loopback driver
*/

void usrNetworkDevStart (void)
    {
    usrNetEndDevStart (pDevName, uNum); /* Uses boot parameters to start an END driver */
    usrNetLoopbackStart ();             /* loopback interface for routining to localhost */
    }



/******************************************************************************
*
* usrNetworkBoot - Setup a network device using the boot parameters
*/

void usrNetworkBoot (void)
    {
    usrNetBoot ();                      /* Reads the enet address from the bootline parameters */
    usrNetmaskGet ();                   /* Extracts netmask value from address field */
    usrNetDevNameGet ();                /* Gets name from "other" field if booting from disk */
    usrNetworkDevStart ();              /* Attach a network device and start the loopback driver */
    }



/******************************************************************************
*
* usrNetworkAddrCheck - Verify the device IP address if assigned dynamically
*/

void usrNetworkAddrCheck (void)
    {
    usrNetConfig (pDevName, uNum, pTgtName, pAddrString); /* Assigns an IP address and netmask */
    }



/******************************************************************************
*
* usrNetworkInit - Initialize the network subsystem
*/

void usrNetworkInit (void)
    {
                                        /* network buffer creation and device support */
    usrNetProtoInit ();                 /* Initialize the network protocol stacks */
    usrMuxLibInit();                    /* network driver to protocol multiplexer */
    usrMuxAddrResInit ();               /* Support for IP over Ethernet */
    miiLibInit ();                      /* MII Library support */
    usrEndLibInit();                    /* Support for network devices using MUX/END interface */
                                        /* This should always be included for backward compatibility */
    usrNetworkBoot ();                  /* Setup a network device using the boot parameters */
    usrNetworkAddrCheck ();             /* Verify the device IP address if assigned dynamically */
    usrNetRemoteInit ();                /* initialize network remote I/O access */
    }



/******************************************************************************
*
* usrWdbInit - the WDB target agent
*/

void usrWdbInit (void)
    {
    wdbInit ();                         /* software agent to support the tornado tools */
    }



/******************************************************************************
*
* usrToolsInit - software development tools
*/

void usrToolsInit (void)
    {
    usrWdbInit ();                      /* the WDB target agent */
    usrDebugCoreInit ();                /* debug core engine used by WDB agent and Target Shell debuggers */
    }



/******************************************************************************
*
* usrRoot - Entry point for post-kernel initialization
*/

void usrRoot ()
    {
    usrPostKernelCoreInit ();           /* initialize various core features after the kernel initialization */
    usrKernelBasicInit ();              /* basic kernel initialization phase 1 */
    usrPostKernelCoreInit2 ();          /* initialize various core features after the kernel initialization */
    usrArincSchedulerInit (SCHED_VALIDATE_SCHEDULES, SCHED_MAXIMUM_JITTER); ppsSchedulerInit (PPS_ACTIVATION_WINDOW); /* Arinc 653 partition scheduler initialization */
    usrBootLineParse (BOOT_LINE_ADRS);  /* parse some boot device configuration info  */
    usrIosCoreInit ();                  /* Initialize the Core I/O system. */
    usrKernelBasicInit2 (APEX_PORT_HEADER_MAX_SIZE, APEX_PORT_ALIGN_SIZE, APEX_PORT_DAP_DMA_COPY_REQUIRED); /* basic kernel initialization phase 2 */
    usrKernelExtraInit ();              /* extended kernel facilities */
    usrIosExtraInit ();                 /* extended I/O system */
    usrNetworkInit ();                  /* Initialize the network subsystem */
    usrToolsInit ();                    /* software development tools */
    timeMonitorLibInit2(TIME_MONITOR_ENABLE); /* enables the time monitor feature in the system (VxWorks 653) */
    usrPartitionInit();                 /* enables the partition domain feature in the system (VxWorks 653) */
    usrAppInit ();                      /* call usrAppInit() (in your usrAppInit.c project file) after startup. */
    usrKernelNormalMode ();             /* set kernel to normal mode. */
    }



/******************************************************************************
*
* usrRootTerm - Entry point for post-kernel termination
*/

void usrRootTerm ()
    {
    }

