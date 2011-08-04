/*
 * linkage table initialization
 */

#include <slVersionLib.h>

static SL_VERSION slVersion =
	{
	8, "vThreads",
	4, "Cert"
	};

static void * slTable;
static void stubInit (void) {
	vxRegisterOrLink(SLVERSION_LINK, &slVersion, &slTable);
}

static const void * init __attribute__((section(".ctors.stubs"),used)) = stubInit;

/*
 * linkage table stubs
 */

__asm__ (".section .text");

__asm__ (".globl CREATE_BLACKBOARD");
__asm__ (".type CREATE_BLACKBOARD,@function");
__asm__ ("CREATE_BLACKBOARD:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,0(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl DISPLAY_BLACKBOARD");
__asm__ (".type DISPLAY_BLACKBOARD,@function");
__asm__ ("DISPLAY_BLACKBOARD:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,4(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl READ_BLACKBOARD");
__asm__ (".type READ_BLACKBOARD,@function");
__asm__ ("READ_BLACKBOARD:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,8(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl CLEAR_BLACKBOARD");
__asm__ (".type CLEAR_BLACKBOARD,@function");
__asm__ ("CLEAR_BLACKBOARD:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,12(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_BLACKBOARD_ID");
__asm__ (".type GET_BLACKBOARD_ID,@function");
__asm__ ("GET_BLACKBOARD_ID:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,16(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_BLACKBOARD_STATUS");
__asm__ (".type GET_BLACKBOARD_STATUS,@function");
__asm__ ("GET_BLACKBOARD_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,20(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl CREATE_BUFFER");
__asm__ (".type CREATE_BUFFER,@function");
__asm__ ("CREATE_BUFFER:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,24(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SEND_BUFFER");
__asm__ (".type SEND_BUFFER,@function");
__asm__ ("SEND_BUFFER:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,28(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl RECEIVE_BUFFER");
__asm__ (".type RECEIVE_BUFFER,@function");
__asm__ ("RECEIVE_BUFFER:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,32(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_BUFFER_ID");
__asm__ (".type GET_BUFFER_ID,@function");
__asm__ ("GET_BUFFER_ID:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,36(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_BUFFER_STATUS");
__asm__ (".type GET_BUFFER_STATUS,@function");
__asm__ ("GET_BUFFER_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,40(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl REPORT_APPLICATION_MESSAGE");
__asm__ (".type REPORT_APPLICATION_MESSAGE,@function");
__asm__ ("REPORT_APPLICATION_MESSAGE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,44(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl CREATE_ERROR_HANDLER");
__asm__ (".type CREATE_ERROR_HANDLER,@function");
__asm__ ("CREATE_ERROR_HANDLER:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,48(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_ERROR_STATUS");
__asm__ (".type GET_ERROR_STATUS,@function");
__asm__ ("GET_ERROR_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,52(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl RAISE_APPLICATION_ERROR");
__asm__ (".type RAISE_APPLICATION_ERROR,@function");
__asm__ ("RAISE_APPLICATION_ERROR:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,56(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl CREATE_QUEUING_PORT");
__asm__ (".type CREATE_QUEUING_PORT,@function");
__asm__ ("CREATE_QUEUING_PORT:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,60(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SEND_QUEUING_MESSAGE");
__asm__ (".type SEND_QUEUING_MESSAGE,@function");
__asm__ ("SEND_QUEUING_MESSAGE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,64(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl RECEIVE_QUEUING_MESSAGE");
__asm__ (".type RECEIVE_QUEUING_MESSAGE,@function");
__asm__ ("RECEIVE_QUEUING_MESSAGE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,68(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_QUEUING_PORT_ID");
__asm__ (".type GET_QUEUING_PORT_ID,@function");
__asm__ ("GET_QUEUING_PORT_ID:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,72(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_QUEUING_PORT_STATUS");
__asm__ (".type GET_QUEUING_PORT_STATUS,@function");
__asm__ ("GET_QUEUING_PORT_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,76(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl CREATE_EVENT");
__asm__ (".type CREATE_EVENT,@function");
__asm__ ("CREATE_EVENT:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,80(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SET_EVENT");
__asm__ (".type SET_EVENT,@function");
__asm__ ("SET_EVENT:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,84(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl RESET_EVENT");
__asm__ (".type RESET_EVENT,@function");
__asm__ ("RESET_EVENT:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,88(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl WAIT_EVENT");
__asm__ (".type WAIT_EVENT,@function");
__asm__ ("WAIT_EVENT:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,92(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_EVENT_ID");
__asm__ (".type GET_EVENT_ID,@function");
__asm__ ("GET_EVENT_ID:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,96(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_EVENT_STATUS");
__asm__ (".type GET_EVENT_STATUS,@function");
__asm__ ("GET_EVENT_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,100(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_PARTITION_STATUS");
__asm__ (".type GET_PARTITION_STATUS,@function");
__asm__ ("GET_PARTITION_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,104(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SET_PARTITION_MODE");
__asm__ (".type SET_PARTITION_MODE,@function");
__asm__ ("SET_PARTITION_MODE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,108(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SET_SCHEDULE_MODE");
__asm__ (".type SET_SCHEDULE_MODE,@function");
__asm__ ("SET_SCHEDULE_MODE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,112(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl CREATE_SAMPLING_PORT");
__asm__ (".type CREATE_SAMPLING_PORT,@function");
__asm__ ("CREATE_SAMPLING_PORT:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,116(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl WRITE_SAMPLING_MESSAGE");
__asm__ (".type WRITE_SAMPLING_MESSAGE,@function");
__asm__ ("WRITE_SAMPLING_MESSAGE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,120(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl READ_SAMPLING_MESSAGE");
__asm__ (".type READ_SAMPLING_MESSAGE,@function");
__asm__ ("READ_SAMPLING_MESSAGE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,124(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_SAMPLING_PORT_ID");
__asm__ (".type GET_SAMPLING_PORT_ID,@function");
__asm__ ("GET_SAMPLING_PORT_ID:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,128(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_SAMPLING_PORT_STATUS");
__asm__ (".type GET_SAMPLING_PORT_STATUS,@function");
__asm__ ("GET_SAMPLING_PORT_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,132(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl TIMED_WAIT");
__asm__ (".type TIMED_WAIT,@function");
__asm__ ("TIMED_WAIT:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,136(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl PERIODIC_WAIT");
__asm__ (".type PERIODIC_WAIT,@function");
__asm__ ("PERIODIC_WAIT:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,140(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_TIME");
__asm__ (".type GET_TIME,@function");
__asm__ ("GET_TIME:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,144(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl REPLENISH");
__asm__ (".type REPLENISH,@function");
__asm__ ("REPLENISH:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,148(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl DELAYED_START");
__asm__ (".type DELAYED_START,@function");
__asm__ ("DELAYED_START:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,152(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_PROCESS_ID");
__asm__ (".type GET_PROCESS_ID,@function");
__asm__ ("GET_PROCESS_ID:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,156(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_MY_ID");
__asm__ (".type GET_MY_ID,@function");
__asm__ ("GET_MY_ID:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,160(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_PROCESS_STATUS");
__asm__ (".type GET_PROCESS_STATUS,@function");
__asm__ ("GET_PROCESS_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,164(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl CREATE_PROCESS");
__asm__ (".type CREATE_PROCESS,@function");
__asm__ ("CREATE_PROCESS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,168(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SET_PRIORITY");
__asm__ (".type SET_PRIORITY,@function");
__asm__ ("SET_PRIORITY:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,172(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SUSPEND_SELF");
__asm__ (".type SUSPEND_SELF,@function");
__asm__ ("SUSPEND_SELF:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,176(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SUSPEND");
__asm__ (".type SUSPEND,@function");
__asm__ ("SUSPEND:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,180(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl RESUME");
__asm__ (".type RESUME,@function");
__asm__ ("RESUME:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,184(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl STOP_SELF");
__asm__ (".type STOP_SELF,@function");
__asm__ ("STOP_SELF:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,188(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl STOP");
__asm__ (".type STOP,@function");
__asm__ ("STOP:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,192(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl START");
__asm__ (".type START,@function");
__asm__ ("START:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,196(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl LOCK_PREEMPTION");
__asm__ (".type LOCK_PREEMPTION,@function");
__asm__ ("LOCK_PREEMPTION:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,200(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl UNLOCK_PREEMPTION");
__asm__ (".type UNLOCK_PREEMPTION,@function");
__asm__ ("UNLOCK_PREEMPTION:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,204(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl CREATE_SEMAPHORE");
__asm__ (".type CREATE_SEMAPHORE,@function");
__asm__ ("CREATE_SEMAPHORE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,208(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl WAIT_SEMAPHORE");
__asm__ (".type WAIT_SEMAPHORE,@function");
__asm__ ("WAIT_SEMAPHORE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,212(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SIGNAL_SEMAPHORE");
__asm__ (".type SIGNAL_SEMAPHORE,@function");
__asm__ ("SIGNAL_SEMAPHORE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,216(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_SEMAPHORE_ID");
__asm__ (".type GET_SEMAPHORE_ID,@function");
__asm__ ("GET_SEMAPHORE_ID:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,220(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_SEMAPHORE_STATUS");
__asm__ (".type GET_SEMAPHORE_STATUS,@function");
__asm__ ("GET_SEMAPHORE_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,224(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl CREATE_SAP_PORT");
__asm__ (".type CREATE_SAP_PORT,@function");
__asm__ ("CREATE_SAP_PORT:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,228(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_SAP_PORT_ID");
__asm__ (".type GET_SAP_PORT_ID,@function");
__asm__ ("GET_SAP_PORT_ID:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,232(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl GET_SAP_PORT_STATUS");
__asm__ (".type GET_SAP_PORT_STATUS,@function");
__asm__ ("GET_SAP_PORT_STATUS:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,236(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl SEND_SAP_MESSAGE");
__asm__ (".type SEND_SAP_MESSAGE,@function");
__asm__ ("SEND_SAP_MESSAGE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,240(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl RECEIVE_SAP_MESSAGE");
__asm__ (".type RECEIVE_SAP_MESSAGE,@function");
__asm__ ("RECEIVE_SAP_MESSAGE:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,244(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl apexTimeMonitorProcGet");
__asm__ (".type apexTimeMonitorProcGet,@function");
__asm__ ("apexTimeMonitorProcGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,248(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl procIdFromTaskIdGet");
__asm__ (".type procIdFromTaskIdGet,@function");
__asm__ ("procIdFromTaskIdGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,252(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskIdFromProcIdGet");
__asm__ (".type taskIdFromProcIdGet,@function");
__asm__ ("taskIdFromProcIdGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,256(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __ashldi3");
__asm__ (".type __ashldi3,@function");
__asm__ ("__ashldi3:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,260(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __cmpdi2");
__asm__ (".type __cmpdi2,@function");
__asm__ ("__cmpdi2:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,264(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __ctypePtrGet");
__asm__ (".type __ctypePtrGet,@function");
__asm__ ("__ctypePtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,268(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __div64");
__asm__ (".type __div64,@function");
__asm__ ("__div64:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,272(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __divdi3");
__asm__ (".type __divdi3,@function");
__asm__ ("__divdi3:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,276(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __errno");
__asm__ (".type __errno,@function");
__asm__ ("__errno:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,280(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __fixdfdi");
__asm__ (".type __fixdfdi,@function");
__asm__ ("__fixdfdi:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,284(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __fixunsdfdi");
__asm__ (".type __fixunsdfdi,@function");
__asm__ ("__fixunsdfdi:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,288(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __floatdidf");
__asm__ (".type __floatdidf,@function");
__asm__ ("__floatdidf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,292(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __floatdisf");
__asm__ (".type __floatdisf,@function");
__asm__ ("__floatdisf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,296(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __localePtrGet");
__asm__ (".type __localePtrGet,@function");
__asm__ ("__localePtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,300(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __lshrdi3");
__asm__ (".type __lshrdi3,@function");
__asm__ ("__lshrdi3:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,304(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __moddi3");
__asm__ (".type __moddi3,@function");
__asm__ ("__moddi3:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,308(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __rem64");
__asm__ (".type __rem64,@function");
__asm__ ("__rem64:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,312(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __ucmpdi2");
__asm__ (".type __ucmpdi2,@function");
__asm__ ("__ucmpdi2:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,316(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __udiv64");
__asm__ (".type __udiv64,@function");
__asm__ ("__udiv64:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,320(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __udivdi3");
__asm__ (".type __udivdi3,@function");
__asm__ ("__udivdi3:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,324(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __umoddi3");
__asm__ (".type __umoddi3,@function");
__asm__ ("__umoddi3:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,328(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl __urem64");
__asm__ (".type __urem64,@function");
__asm__ ("__urem64:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,332(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _func_apexLockLevelGetPtrGet");
__asm__ (".type _func_apexLockLevelGetPtrGet,@function");
__asm__ ("_func_apexLockLevelGetPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,336(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _func_logMsgPtrGet");
__asm__ (".type _func_logMsgPtrGet,@function");
__asm__ ("_func_logMsgPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,340(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _func_procCreateHookFctPtrGet");
__asm__ (".type _func_procCreateHookFctPtrGet,@function");
__asm__ ("_func_procCreateHookFctPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,344(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _func_procStartHookFctPtrGet");
__asm__ (".type _func_procStartHookFctPtrGet,@function");
__asm__ ("_func_procStartHookFctPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,348(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _func_pthread_setcanceltypePtrGet");
__asm__ (".type _func_pthread_setcanceltypePtrGet,@function");
__asm__ ("_func_pthread_setcanceltypePtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,352(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _func_sigTimeoutRecalcPtrGet");
__asm__ (".type _func_sigTimeoutRecalcPtrGet,@function");
__asm__ ("_func_sigTimeoutRecalcPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,356(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _func_tasknameRecordPtrGet");
__asm__ (".type _func_tasknameRecordPtrGet,@function");
__asm__ ("_func_tasknameRecordPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,360(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _msgQReceive");
__asm__ (".type _msgQReceive,@function");
__asm__ ("_msgQReceive:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,364(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _msgQSend");
__asm__ (".type _msgQSend,@function");
__asm__ ("_msgQSend:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,368(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _semTake");
__asm__ (".type _semTake,@function");
__asm__ ("_semTake:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,372(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _semBQHeadTake");
__asm__ (".type _semBQHeadTake,@function");
__asm__ ("_semBQHeadTake:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,376(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _taskDelay");
__asm__ (".type _taskDelay,@function");
__asm__ ("_taskDelay:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,380(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _taskDelayAbs");
__asm__ (".type _taskDelayAbs,@function");
__asm__ ("_taskDelayAbs:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,384(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _wdStart");
__asm__ (".type _wdStart,@function");
__asm__ ("_wdStart:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,388(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl _wdStartAbs");
__asm__ (".type _wdStartAbs,@function");
__asm__ ("_wdStartAbs:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,392(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl abs");
__asm__ (".type abs,@function");
__asm__ ("abs:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,396(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl acos");
__asm__ (".type acos,@function");
__asm__ ("acos:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,400(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl asin");
__asm__ (".type asin,@function");
__asm__ ("asin:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,404(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl atan2");
__asm__ (".type atan2,@function");
__asm__ ("atan2:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,408(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl atan");
__asm__ (".type atan,@function");
__asm__ ("atan:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,412(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl atof");
__asm__ (".type atof,@function");
__asm__ ("atof:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,416(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl atoi");
__asm__ (".type atoi,@function");
__asm__ ("atoi:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,420(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl atol");
__asm__ (".type atol,@function");
__asm__ ("atol:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,424(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bcmp");
__asm__ (".type bcmp,@function");
__asm__ ("bcmp:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,428(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bcopy");
__asm__ (".type bcopy,@function");
__asm__ ("bcopy:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,432(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bcopyFast");
__asm__ (".type bcopyFast,@function");
__asm__ ("bcopyFast:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,436(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bcopyBytes");
__asm__ (".type bcopyBytes,@function");
__asm__ ("bcopyBytes:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,440(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bcopyLongs");
__asm__ (".type bcopyLongs,@function");
__asm__ ("bcopyLongs:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,444(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bcopyWords");
__asm__ (".type bcopyWords,@function");
__asm__ ("bcopyWords:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,448(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bfill");
__asm__ (".type bfill,@function");
__asm__ ("bfill:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,452(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bfillBytes");
__asm__ (".type bfillBytes,@function");
__asm__ ("bfillBytes:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,456(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bswap");
__asm__ (".type bswap,@function");
__asm__ ("bswap:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,460(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bzero");
__asm__ (".type bzero,@function");
__asm__ ("bzero:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,464(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl bzeroFast");
__asm__ (".type bzeroFast,@function");
__asm__ ("bzeroFast:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,468(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl cbrt");
__asm__ (".type cbrt,@function");
__asm__ ("cbrt:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,472(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl calloc");
__asm__ (".type calloc,@function");
__asm__ ("calloc:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,476(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ceil");
__asm__ (".type ceil,@function");
__asm__ ("ceil:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,480(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl chdir");
__asm__ (".type chdir,@function");
__asm__ ("chdir:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,484(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl classClassIdPtrGet");
__asm__ (".type classClassIdPtrGet,@function");
__asm__ ("classClassIdPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,488(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl classInit");
__asm__ (".type classInit,@function");
__asm__ ("classInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,492(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl close");
__asm__ (".type close,@function");
__asm__ ("close:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,496(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl clock_gettime");
__asm__ (".type clock_gettime,@function");
__asm__ ("clock_gettime:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,500(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl clock_setres");
__asm__ (".type clock_setres,@function");
__asm__ ("clock_setres:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,504(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl clock_settime");
__asm__ (".type clock_settime,@function");
__asm__ ("clock_settime:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,508(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl clockLibInit");
__asm__ (".type clockLibInit,@function");
__asm__ ("clockLibInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,512(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl consoleFdPtrGet");
__asm__ (".type consoleFdPtrGet,@function");
__asm__ ("consoleFdPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,516(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl cos");
__asm__ (".type cos,@function");
__asm__ ("cos:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,520(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl creat");
__asm__ (".type creat,@function");
__asm__ ("creat:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,524(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl div_r");
__asm__ (".type div_r,@function");
__asm__ ("div_r:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,528(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl dllAdd");
__asm__ (".type dllAdd,@function");
__asm__ ("dllAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,532(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl dllCount");
__asm__ (".type dllCount,@function");
__asm__ ("dllCount:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,536(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl dllCreate");
__asm__ (".type dllCreate,@function");
__asm__ ("dllCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,540(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl dllEach");
__asm__ (".type dllEach,@function");
__asm__ ("dllEach:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,544(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl dllGet");
__asm__ (".type dllGet,@function");
__asm__ ("dllGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,548(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl dllInit");
__asm__ (".type dllInit,@function");
__asm__ ("dllInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,552(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl dllInsert");
__asm__ (".type dllInsert,@function");
__asm__ ("dllInsert:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,556(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl dllRemove");
__asm__ (".type dllRemove,@function");
__asm__ ("dllRemove:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,560(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl errnoGet");
__asm__ (".type errnoGet,@function");
__asm__ ("errnoGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,564(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl errnoOfTaskGet");
__asm__ (".type errnoOfTaskGet,@function");
__asm__ ("errnoOfTaskGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,568(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl errnoOfTaskSet");
__asm__ (".type errnoOfTaskSet,@function");
__asm__ ("errnoOfTaskSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,572(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl errnoSet");
__asm__ (".type errnoSet,@function");
__asm__ ("errnoSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,576(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl evtActionPtrGet");
__asm__ (".type evtActionPtrGet,@function");
__asm__ ("evtActionPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,580(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl excHookAdd");
__asm__ (".type excHookAdd,@function");
__asm__ ("excHookAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,584(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl excJobAdd");
__asm__ (".type excJobAdd,@function");
__asm__ ("excJobAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,588(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl exp");
__asm__ (".type exp,@function");
__asm__ ("exp:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,592(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fabs");
__asm__ (".type fabs,@function");
__asm__ ("fabs:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,596(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fdprintf");
__asm__ (".type fdprintf,@function");
__asm__ ("fdprintf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,600(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fdTablePtrGet");
__asm__ (".type fdTablePtrGet,@function");
__asm__ ("fdTablePtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,604(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ffsMsb");
__asm__ (".type ffsMsb,@function");
__asm__ ("ffsMsb:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,608(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fioFormatV");
__asm__ (".type fioFormatV,@function");
__asm__ ("fioFormatV:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,612(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl floor");
__asm__ (".type floor,@function");
__asm__ ("floor:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,616(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fmod");
__asm__ (".type fmod,@function");
__asm__ ("fmod:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,620(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fppProbe");
__asm__ (".type fppProbe,@function");
__asm__ ("fppProbe:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,624(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fppRestore");
__asm__ (".type fppRestore,@function");
__asm__ ("fppRestore:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,628(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fppSave");
__asm__ (".type fppSave,@function");
__asm__ ("fppSave:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,632(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fppTaskRegsGet");
__asm__ (".type fppTaskRegsGet,@function");
__asm__ ("fppTaskRegsGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,636(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fppTaskRegsSet");
__asm__ (".type fppTaskRegsSet,@function");
__asm__ ("fppTaskRegsSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,640(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fpTypeGet");
__asm__ (".type fpTypeGet,@function");
__asm__ ("fpTypeGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,644(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl frexp");
__asm__ (".type frexp,@function");
__asm__ ("frexp:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,648(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl fstat");
__asm__ (".type fstat,@function");
__asm__ ("fstat:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,652(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl getcwd");
__asm__ (".type getcwd,@function");
__asm__ ("getcwd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,656(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl gmtime");
__asm__ (".type gmtime,@function");
__asm__ ("gmtime:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,660(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl gmtime_r");
__asm__ (".type gmtime_r,@function");
__asm__ ("gmtime_r:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,664(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl index");
__asm__ (".type index,@function");
__asm__ ("index:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,668(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl intContext");
__asm__ (".type intContext,@function");
__asm__ ("intContext:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,672(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl intCount");
__asm__ (".type intCount,@function");
__asm__ ("intCount:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,676(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl intCntPtrGet");
__asm__ (".type intCntPtrGet,@function");
__asm__ ("intCntPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,680(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl intLock");
__asm__ (".type intLock,@function");
__asm__ ("intLock:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,684(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl intLockLevelSet");
__asm__ (".type intLockLevelSet,@function");
__asm__ ("intLockLevelSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,688(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl intUnlock");
__asm__ (".type intUnlock,@function");
__asm__ ("intUnlock:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,692(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ioDefPathGet");
__asm__ (".type ioDefPathGet,@function");
__asm__ ("ioDefPathGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,696(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ioDefPathSet");
__asm__ (".type ioDefPathSet,@function");
__asm__ ("ioDefPathSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,700(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ioGlobalStdGet");
__asm__ (".type ioGlobalStdGet,@function");
__asm__ ("ioGlobalStdGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,704(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ioGlobalStdSet");
__asm__ (".type ioGlobalStdSet,@function");
__asm__ ("ioGlobalStdSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,708(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ioTaskStdGet");
__asm__ (".type ioTaskStdGet,@function");
__asm__ ("ioTaskStdGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,712(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ioTaskStdSet");
__asm__ (".type ioTaskStdSet,@function");
__asm__ ("ioTaskStdSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,716(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ioctl");
__asm__ (".type ioctl,@function");
__asm__ ("ioctl:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,720(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl iosDevAdd");
__asm__ (".type iosDevAdd,@function");
__asm__ ("iosDevAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,724(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl iosDevFind");
__asm__ (".type iosDevFind,@function");
__asm__ ("iosDevFind:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,728(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl iosDrvInstall");
__asm__ (".type iosDrvInstall,@function");
__asm__ ("iosDrvInstall:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,732(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl iosFdFreeHookRtnPtrGet");
__asm__ (".type iosFdFreeHookRtnPtrGet,@function");
__asm__ ("iosFdFreeHookRtnPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,736(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl iosFdNewHookRtnPtrGet");
__asm__ (".type iosFdNewHookRtnPtrGet,@function");
__asm__ ("iosFdNewHookRtnPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,740(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl iosFdValue");
__asm__ (".type iosFdValue,@function");
__asm__ ("iosFdValue:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,744(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl iosLibInitializedPtrGet");
__asm__ (".type iosLibInitializedPtrGet,@function");
__asm__ ("iosLibInitializedPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,748(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl iosRelinquish");
__asm__ (".type iosRelinquish,@function");
__asm__ ("iosRelinquish:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,752(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl isalnum");
__asm__ (".type isalnum,@function");
__asm__ ("isalnum:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,756(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl isalpha");
__asm__ (".type isalpha,@function");
__asm__ ("isalpha:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,760(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl iscntrl");
__asm__ (".type iscntrl,@function");
__asm__ ("iscntrl:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,764(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl isdigit");
__asm__ (".type isdigit,@function");
__asm__ ("isdigit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,768(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl isgraph");
__asm__ (".type isgraph,@function");
__asm__ ("isgraph:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,772(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl islower");
__asm__ (".type islower,@function");
__asm__ ("islower:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,776(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl isprint");
__asm__ (".type isprint,@function");
__asm__ ("isprint:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,780(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ispunct");
__asm__ (".type ispunct,@function");
__asm__ ("ispunct:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,784(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl isspace");
__asm__ (".type isspace,@function");
__asm__ ("isspace:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,788(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl isupper");
__asm__ (".type isupper,@function");
__asm__ ("isupper:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,792(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl isxdigit");
__asm__ (".type isxdigit,@function");
__asm__ ("isxdigit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,796(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl kill");
__asm__ (".type kill,@function");
__asm__ ("kill:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,800(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl kernelStatePtrGet");
__asm__ (".type kernelStatePtrGet,@function");
__asm__ ("kernelStatePtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,804(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl kernelTimeSlice");
__asm__ (".type kernelTimeSlice,@function");
__asm__ ("kernelTimeSlice:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,808(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl kernelVersion");
__asm__ (".type kernelVersion,@function");
__asm__ ("kernelVersion:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,812(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl labs");
__asm__ (".type labs,@function");
__asm__ ("labs:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,816(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ldiv_r");
__asm__ (".type ldiv_r,@function");
__asm__ ("ldiv_r:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,820(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl ldexp");
__asm__ (".type ldexp,@function");
__asm__ ("ldexp:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,824(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl localeconv");
__asm__ (".type localeconv,@function");
__asm__ ("localeconv:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,828(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl log");
__asm__ (".type log,@function");
__asm__ ("log:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,832(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl log10");
__asm__ (".type log10,@function");
__asm__ ("log10:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,836(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl logMsg");
__asm__ (".type logMsg,@function");
__asm__ ("logMsg:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,840(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl longjmp");
__asm__ (".type longjmp,@function");
__asm__ ("longjmp:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,844(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl lseek");
__asm__ (".type lseek,@function");
__asm__ ("lseek:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,848(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl malloc");
__asm__ (".type malloc,@function");
__asm__ ("malloc:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,852(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl maxDriversPtrGet");
__asm__ (".type maxDriversPtrGet,@function");
__asm__ ("maxDriversPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,856(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl maxFilesPtrGet");
__asm__ (".type maxFilesPtrGet,@function");
__asm__ ("maxFilesPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,860(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memalign");
__asm__ (".type memalign,@function");
__asm__ ("memalign:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,864(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memAddToPool");
__asm__ (".type memAddToPool,@function");
__asm__ ("memAddToPool:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,868(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memchr");
__asm__ (".type memchr,@function");
__asm__ ("memchr:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,872(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memcmp");
__asm__ (".type memcmp,@function");
__asm__ ("memcmp:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,876(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memcpy");
__asm__ (".type memcpy,@function");
__asm__ ("memcpy:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,880(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memmove");
__asm__ (".type memmove,@function");
__asm__ ("memmove:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,884(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memPartAddToPool");
__asm__ (".type memPartAddToPool,@function");
__asm__ ("memPartAddToPool:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,888(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memPartAlignedAlloc");
__asm__ (".type memPartAlignedAlloc,@function");
__asm__ ("memPartAlignedAlloc:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,892(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memPartAlloc");
__asm__ (".type memPartAlloc,@function");
__asm__ ("memPartAlloc:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,896(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memPartAllocDisable");
__asm__ (".type memPartAllocDisable,@function");
__asm__ ("memPartAllocDisable:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,900(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memPartCreate");
__asm__ (".type memPartCreate,@function");
__asm__ ("memPartCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,904(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memPartInit");
__asm__ (".type memPartInit,@function");
__asm__ ("memPartInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,908(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memset");
__asm__ (".type memset,@function");
__asm__ ("memset:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,912(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl memSysPartIdPtrGet");
__asm__ (".type memSysPartIdPtrGet,@function");
__asm__ ("memSysPartIdPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,916(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl msgQClassIdPtrGet");
__asm__ (".type msgQClassIdPtrGet,@function");
__asm__ ("msgQClassIdPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,920(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl msgQCreate");
__asm__ (".type msgQCreate,@function");
__asm__ ("msgQCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,924(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl msgQInit");
__asm__ (".type msgQInit,@function");
__asm__ ("msgQInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,928(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl msgQNumMsgs");
__asm__ (".type msgQNumMsgs,@function");
__asm__ ("msgQNumMsgs:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,932(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl msgQReceive");
__asm__ (".type msgQReceive,@function");
__asm__ ("msgQReceive:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,936(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl msgQSend");
__asm__ (".type msgQSend,@function");
__asm__ ("msgQSend:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,940(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl modf");
__asm__ (".type modf,@function");
__asm__ ("modf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,944(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl objAlloc");
__asm__ (".type objAlloc,@function");
__asm__ ("objAlloc:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,948(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl objAllocExtra");
__asm__ (".type objAllocExtra,@function");
__asm__ ("objAllocExtra:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,952(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl objCoreInit");
__asm__ (".type objCoreInit,@function");
__asm__ ("objCoreInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,956(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl open");
__asm__ (".type open,@function");
__asm__ ("open:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,960(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl partitionNormalModeSet");
__asm__ (".type partitionNormalModeSet,@function");
__asm__ ("partitionNormalModeSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,964(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl partitionSwitchInHookAdd");
__asm__ (".type partitionSwitchInHookAdd,@function");
__asm__ ("partitionSwitchInHookAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,968(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl pathCat");
__asm__ (".type pathCat,@function");
__asm__ ("pathCat:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,972(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl pow");
__asm__ (".type pow,@function");
__asm__ ("pow:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,976(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl printErr");
__asm__ (".type printErr,@function");
__asm__ ("printErr:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,980(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl printf");
__asm__ (".type printf,@function");
__asm__ ("printf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,984(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl procCreateHookAdd");
__asm__ (".type procCreateHookAdd,@function");
__asm__ ("procCreateHookAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,988(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl procStartHookAdd");
__asm__ (".type procStartHookAdd,@function");
__asm__ ("procStartHookAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,992(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl pTaskLastDspTcbPtrGet");
__asm__ (".type pTaskLastDspTcbPtrGet,@function");
__asm__ ("pTaskLastDspTcbPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,996(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl pTaskLastFpTcbPtrGet");
__asm__ (".type pTaskLastFpTcbPtrGet,@function");
__asm__ ("pTaskLastFpTcbPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1000(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl qFifoClassIdPtrGet");
__asm__ (".type qFifoClassIdPtrGet,@function");
__asm__ ("qFifoClassIdPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1004(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl qInit");
__asm__ (".type qInit,@function");
__asm__ ("qInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1008(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl qPriListClassIdPtrGet");
__asm__ (".type qPriListClassIdPtrGet,@function");
__asm__ ("qPriListClassIdPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1012(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl raise");
__asm__ (".type raise,@function");
__asm__ ("raise:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1016(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl read");
__asm__ (".type read,@function");
__asm__ ("read:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1020(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl readyQHeadPtrGet");
__asm__ (".type readyQHeadPtrGet,@function");
__asm__ ("readyQHeadPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1024(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl reboot");
__asm__ (".type reboot,@function");
__asm__ ("reboot:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1028(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rebootHookAdd");
__asm__ (".type rebootHookAdd,@function");
__asm__ ("rebootHookAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1032(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl remove");
__asm__ (".type remove,@function");
__asm__ ("remove:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1036(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rename");
__asm__ (".type rename,@function");
__asm__ ("rename:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1040(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngBufGet");
__asm__ (".type rngBufGet,@function");
__asm__ ("rngBufGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1044(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngBufPut");
__asm__ (".type rngBufPut,@function");
__asm__ ("rngBufPut:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1048(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngCreate");
__asm__ (".type rngCreate,@function");
__asm__ ("rngCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1052(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngFlush");
__asm__ (".type rngFlush,@function");
__asm__ ("rngFlush:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1056(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngFreeBytes");
__asm__ (".type rngFreeBytes,@function");
__asm__ ("rngFreeBytes:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1060(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngIsEmpty");
__asm__ (".type rngIsEmpty,@function");
__asm__ ("rngIsEmpty:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1064(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngIsFull");
__asm__ (".type rngIsFull,@function");
__asm__ ("rngIsFull:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1068(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngMoveAhead");
__asm__ (".type rngMoveAhead,@function");
__asm__ ("rngMoveAhead:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1072(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngNBytes");
__asm__ (".type rngNBytes,@function");
__asm__ ("rngNBytes:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1076(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl rngPutAhead");
__asm__ (".type rngPutAhead,@function");
__asm__ ("rngPutAhead:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1080(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl roundRobinOnPtrGet");
__asm__ (".type roundRobinOnPtrGet,@function");
__asm__ ("roundRobinOnPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1084(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl roundRobinSlicePtrGet");
__asm__ (".type roundRobinSlicePtrGet,@function");
__asm__ ("roundRobinSlicePtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1088(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sdRgnAddrGet");
__asm__ (".type sdRgnAddrGet,@function");
__asm__ ("sdRgnAddrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1092(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sdRgnInfoGet");
__asm__ (".type sdRgnInfoGet,@function");
__asm__ ("sdRgnInfoGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1096(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sdRgnAllGet");
__asm__ (".type sdRgnAllGet,@function");
__asm__ ("sdRgnAllGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1100(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sdRgnFirstGet");
__asm__ (".type sdRgnFirstGet,@function");
__asm__ ("sdRgnFirstGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1104(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sdRgnNextGet");
__asm__ (".type sdRgnNextGet,@function");
__asm__ ("sdRgnNextGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1108(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semBCreate");
__asm__ (".type semBCreate,@function");
__asm__ ("semBCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1112(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semBInit");
__asm__ (".type semBInit,@function");
__asm__ ("semBInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1116(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semCCreate");
__asm__ (".type semCCreate,@function");
__asm__ ("semCCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1120(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semCInit");
__asm__ (".type semCInit,@function");
__asm__ ("semCInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1124(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semClassIdPtrGet");
__asm__ (".type semClassIdPtrGet,@function");
__asm__ ("semClassIdPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1128(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semCLibInit");
__asm__ (".type semCLibInit,@function");
__asm__ ("semCLibInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1132(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semEvStart");
__asm__ (".type semEvStart,@function");
__asm__ ("semEvStart:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1136(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semEvStop");
__asm__ (".type semEvStop,@function");
__asm__ ("semEvStop:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1140(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semFlush");
__asm__ (".type semFlush,@function");
__asm__ ("semFlush:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1144(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semGive");
__asm__ (".type semGive,@function");
__asm__ ("semGive:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1148(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semMCreate");
__asm__ (".type semMCreate,@function");
__asm__ ("semMCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1152(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semMInit");
__asm__ (".type semMInit,@function");
__asm__ ("semMInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1156(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl semTake");
__asm__ (".type semTake,@function");
__asm__ ("semTake:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1160(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl setjmp");
__asm__ (".type setjmp,@function");
__asm__ ("setjmp:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1164(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl setlocale");
__asm__ (".type setlocale,@function");
__asm__ ("setlocale:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1168(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sigaction");
__asm__ (".type sigaction,@function");
__asm__ ("sigaction:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1172(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sigaddset");
__asm__ (".type sigaddset,@function");
__asm__ ("sigaddset:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1176(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sigdelset");
__asm__ (".type sigdelset,@function");
__asm__ ("sigdelset:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1180(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sigemptyset");
__asm__ (".type sigemptyset,@function");
__asm__ ("sigemptyset:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1184(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl signal");
__asm__ (".type signal,@function");
__asm__ ("signal:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1188(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sigPendInit");
__asm__ (".type sigPendInit,@function");
__asm__ ("sigPendInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1192(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sigPendKill");
__asm__ (".type sigPendKill,@function");
__asm__ ("sigPendKill:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1196(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sigprocmask");
__asm__ (".type sigprocmask,@function");
__asm__ ("sigprocmask:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1200(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sigsetmask");
__asm__ (".type sigsetmask,@function");
__asm__ ("sigsetmask:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1204(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sigvec");
__asm__ (".type sigvec,@function");
__asm__ ("sigvec:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1208(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sin");
__asm__ (".type sin,@function");
__asm__ ("sin:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1212(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sllCreate");
__asm__ (".type sllCreate,@function");
__asm__ ("sllCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1216(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sllGet");
__asm__ (".type sllGet,@function");
__asm__ ("sllGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1220(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sllInit");
__asm__ (".type sllInit,@function");
__asm__ ("sllInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1224(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sllPutAtHead");
__asm__ (".type sllPutAtHead,@function");
__asm__ ("sllPutAtHead:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1228(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sllPutAtTail");
__asm__ (".type sllPutAtTail,@function");
__asm__ ("sllPutAtTail:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1232(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sllRemove");
__asm__ (".type sllRemove,@function");
__asm__ ("sllRemove:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1236(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sprintf");
__asm__ (".type sprintf,@function");
__asm__ ("sprintf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1240(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sqrt");
__asm__ (".type sqrt,@function");
__asm__ ("sqrt:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1244(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sscanf");
__asm__ (".type sscanf,@function");
__asm__ ("sscanf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1248(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl statSymTblPtrGet");
__asm__ (".type statSymTblPtrGet,@function");
__asm__ ("statSymTblPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1252(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strcat");
__asm__ (".type strcat,@function");
__asm__ ("strcat:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1256(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strchr");
__asm__ (".type strchr,@function");
__asm__ ("strchr:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1260(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strcmp");
__asm__ (".type strcmp,@function");
__asm__ ("strcmp:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1264(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strcpy");
__asm__ (".type strcpy,@function");
__asm__ ("strcpy:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1268(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strlen");
__asm__ (".type strlen,@function");
__asm__ ("strlen:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1272(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strncat");
__asm__ (".type strncat,@function");
__asm__ ("strncat:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1276(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strncmp");
__asm__ (".type strncmp,@function");
__asm__ ("strncmp:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1280(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strncpy");
__asm__ (".type strncpy,@function");
__asm__ ("strncpy:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1284(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strpbrk");
__asm__ (".type strpbrk,@function");
__asm__ ("strpbrk:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1288(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strrchr");
__asm__ (".type strrchr,@function");
__asm__ ("strrchr:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1292(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strspn");
__asm__ (".type strspn,@function");
__asm__ ("strspn:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1296(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strstr");
__asm__ (".type strstr,@function");
__asm__ ("strstr:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1300(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strtod");
__asm__ (".type strtod,@function");
__asm__ ("strtod:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1304(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strtok_r");
__asm__ (".type strtok_r,@function");
__asm__ ("strtok_r:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1308(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strtol");
__asm__ (".type strtol,@function");
__asm__ ("strtol:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1312(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl strtoul");
__asm__ (".type strtoul,@function");
__asm__ ("strtoul:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1316(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sysClkRateGet");
__asm__ (".type sysClkRateGet,@function");
__asm__ ("sysClkRateGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1320(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sysSymTblPtrGet");
__asm__ (".type sysSymTblPtrGet,@function");
__asm__ ("sysSymTblPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1324(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sysTimestamp64DiffToNsec");
__asm__ (".type sysTimestamp64DiffToNsec,@function");
__asm__ ("sysTimestamp64DiffToNsec:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1328(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sysTimestamp64Get");
__asm__ (".type sysTimestamp64Get,@function");
__asm__ ("sysTimestamp64Get:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1332(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl sysToMonitor");
__asm__ (".type sysToMonitor,@function");
__asm__ ("sysToMonitor:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1336(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl swab");
__asm__ (".type swab,@function");
__asm__ ("swab:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1340(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl tan");
__asm__ (".type tan,@function");
__asm__ ("tan:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1344(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskActivate");
__asm__ (".type taskActivate,@function");
__asm__ ("taskActivate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1348(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskArgsGet");
__asm__ (".type taskArgsGet,@function");
__asm__ ("taskArgsGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1352(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskClassIdPtrGet");
__asm__ (".type taskClassIdPtrGet,@function");
__asm__ ("taskClassIdPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1356(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskCreat");
__asm__ (".type taskCreat,@function");
__asm__ ("taskCreat:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1360(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskCreateHookAdd");
__asm__ (".type taskCreateHookAdd,@function");
__asm__ ("taskCreateHookAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1364(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskDelay");
__asm__ (".type taskDelay,@function");
__asm__ ("taskDelay:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1368(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskExitHookPtrGet");
__asm__ (".type taskExitHookPtrGet,@function");
__asm__ ("taskExitHookPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1372(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskIdCurrentPtrGet");
__asm__ (".type taskIdCurrentPtrGet,@function");
__asm__ ("taskIdCurrentPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1376(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskIdDefault");
__asm__ (".type taskIdDefault,@function");
__asm__ ("taskIdDefault:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1380(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskIdListGet");
__asm__ (".type taskIdListGet,@function");
__asm__ ("taskIdListGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1384(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskIdSelf");
__asm__ (".type taskIdSelf,@function");
__asm__ ("taskIdSelf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1388(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskIdVerify");
__asm__ (".type taskIdVerify,@function");
__asm__ ("taskIdVerify:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1392(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskInfoGet");
__asm__ (".type taskInfoGet,@function");
__asm__ ("taskInfoGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1396(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskInit");
__asm__ (".type taskInit,@function");
__asm__ ("taskInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1400(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskLock");
__asm__ (".type taskLock,@function");
__asm__ ("taskLock:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1404(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskName");
__asm__ (".type taskName,@function");
__asm__ ("taskName:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1408(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskOptionsGet");
__asm__ (".type taskOptionsGet,@function");
__asm__ ("taskOptionsGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1412(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskPcGet");
__asm__ (".type taskPcGet,@function");
__asm__ ("taskPcGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1416(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskPriorityGet");
__asm__ (".type taskPriorityGet,@function");
__asm__ ("taskPriorityGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1420(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskPrioritySet");
__asm__ (".type taskPrioritySet,@function");
__asm__ ("taskPrioritySet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1424(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskReset");
__asm__ (".type taskReset,@function");
__asm__ ("taskReset:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1428(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskResume");
__asm__ (".type taskResume,@function");
__asm__ ("taskResume:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1432(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskSafe");
__asm__ (".type taskSafe,@function");
__asm__ ("taskSafe:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1436(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskSafeWait");
__asm__ (".type taskSafeWait,@function");
__asm__ ("taskSafeWait:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1440(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskSpawn");
__asm__ (".type taskSpawn,@function");
__asm__ ("taskSpawn:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1444(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskSuspend");
__asm__ (".type taskSuspend,@function");
__asm__ ("taskSuspend:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1448(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskSwapHookAdd");
__asm__ (".type taskSwapHookAdd,@function");
__asm__ ("taskSwapHookAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1452(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskSwitchHookAdd");
__asm__ (".type taskSwitchHookAdd,@function");
__asm__ ("taskSwitchHookAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1456(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskTcb");
__asm__ (".type taskTcb,@function");
__asm__ ("taskTcb:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1460(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskUndelay");
__asm__ (".type taskUndelay,@function");
__asm__ ("taskUndelay:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1464(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskUnlock");
__asm__ (".type taskUnlock,@function");
__asm__ ("taskUnlock:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1468(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskUnsafe");
__asm__ (".type taskUnsafe,@function");
__asm__ ("taskUnsafe:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1472(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskVarAdd");
__asm__ (".type taskVarAdd,@function");
__asm__ ("taskVarAdd:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1476(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskVarGet");
__asm__ (".type taskVarGet,@function");
__asm__ ("taskVarGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1480(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskVarInit");
__asm__ (".type taskVarInit,@function");
__asm__ ("taskVarInit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1484(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl taskVarSet");
__asm__ (".type taskVarSet,@function");
__asm__ ("taskVarSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1488(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl tick64Get");
__asm__ (".type tick64Get,@function");
__asm__ ("tick64Get:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1492(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl tickQHeadPtrGet");
__asm__ (".type tickQHeadPtrGet,@function");
__asm__ ("tickQHeadPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1496(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl timeMonitorClear");
__asm__ (".type timeMonitorClear,@function");
__asm__ ("timeMonitorClear:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1500(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl timeMonitorStart");
__asm__ (".type timeMonitorStart,@function");
__asm__ ("timeMonitorStart:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1504(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl timeMonitorStop");
__asm__ (".type timeMonitorStop,@function");
__asm__ ("timeMonitorStop:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1508(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl timeMonitorTaskGet");
__asm__ (".type timeMonitorTaskGet,@function");
__asm__ ("timeMonitorTaskGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1512(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl timeMonitorSysGet");
__asm__ (".type timeMonitorSysGet,@function");
__asm__ ("timeMonitorSysGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1516(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl tolower");
__asm__ (".type tolower,@function");
__asm__ ("tolower:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1520(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl toupper");
__asm__ (".type toupper,@function");
__asm__ ("toupper:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1524(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl trgEvtClassPtrGet");
__asm__ (".type trgEvtClassPtrGet,@function");
__asm__ ("trgEvtClassPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1528(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl unlink");
__asm__ (".type unlink,@function");
__asm__ ("unlink:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1532(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl uswab");
__asm__ (".type uswab,@function");
__asm__ ("uswab:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1536(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl valloc");
__asm__ (".type valloc,@function");
__asm__ ("valloc:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1540(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vprintf");
__asm__ (".type vprintf,@function");
__asm__ ("vprintf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1544(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vsprintf");
__asm__ (".type vsprintf,@function");
__asm__ ("vsprintf:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1548(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vThreadsEventHandlerRegister");
__asm__ (".type vThreadsEventHandlerRegister,@function");
__asm__ ("vThreadsEventHandlerRegister:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1552(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vThreadsOsInvoke");
__asm__ (".type vThreadsOsInvoke,@function");
__asm__ ("vThreadsOsInvoke:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1556(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vThreadsOsInvokeCustom");
__asm__ (".type vThreadsOsInvokeCustom,@function");
__asm__ ("vThreadsOsInvokeCustom:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1560(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vThreadsSysInfoPtrGet");
__asm__ (".type vThreadsSysInfoPtrGet,@function");
__asm__ ("vThreadsSysInfoPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1564(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vxCurrentPcGet");
__asm__ (".type vxCurrentPcGet,@function");
__asm__ ("vxCurrentPcGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1568(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vxMemProbe");
__asm__ (".type vxMemProbe,@function");
__asm__ ("vxMemProbe:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1572(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vxTas");
__asm__ (".type vxTas,@function");
__asm__ ("vxTas:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1576(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vxTicksPtrGet");
__asm__ (".type vxTicksPtrGet,@function");
__asm__ ("vxTicksPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1580(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vxAbsTicksPtrGet");
__asm__ (".type vxAbsTicksPtrGet,@function");
__asm__ ("vxAbsTicksPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1584(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl vxJobAddTicksPtrGet");
__asm__ (".type vxJobAddTicksPtrGet,@function");
__asm__ ("vxJobAddTicksPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1588(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl wdCancel");
__asm__ (".type wdCancel,@function");
__asm__ ("wdCancel:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1592(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl wdCreate");
__asm__ (".type wdCreate,@function");
__asm__ ("wdCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1596(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl wdStart");
__asm__ (".type wdStart,@function");
__asm__ ("wdStart:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1600(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl windExit");
__asm__ (".type windExit,@function");
__asm__ ("windExit:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1604(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl windPendQFlush");
__asm__ (".type windPendQFlush,@function");
__asm__ ("windPendQFlush:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1608(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl windPendQGet");
__asm__ (".type windPendQGet,@function");
__asm__ ("windPendQGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1612(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl windPendQPut");
__asm__ (".type windPendQPut,@function");
__asm__ ("windPendQPut:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1616(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl windPendQRemove");
__asm__ (".type windPendQRemove,@function");
__asm__ ("windPendQRemove:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1620(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl windReadyQPut");
__asm__ (".type windReadyQPut,@function");
__asm__ ("windReadyQPut:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1624(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl windSuspend");
__asm__ (".type windSuspend,@function");
__asm__ ("windSuspend:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1628(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl windTickRecalibrate");
__asm__ (".type windTickRecalibrate,@function");
__asm__ ("windTickRecalibrate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1632(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl workQAdd1");
__asm__ (".type workQAdd1,@function");
__asm__ ("workQAdd1:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1636(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl workQAdd2");
__asm__ (".type workQAdd2,@function");
__asm__ ("workQAdd2:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1640(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl write");
__asm__ (".type write,@function");
__asm__ ("write:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1644(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl wvEvtClassPtrGet");
__asm__ (".type wvEvtClassPtrGet,@function");
__asm__ ("wvEvtClassPtrGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1648(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl configRecordFieldGet");
__asm__ (".type configRecordFieldGet,@function");
__asm__ ("configRecordFieldGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1652(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl hmEventInject");
__asm__ (".type hmEventInject,@function");
__asm__ ("hmEventInject:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1656(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl hmEventLog");
__asm__ (".type hmEventLog,@function");
__asm__ ("hmEventLog:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1660(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl hmIsAutoLog");
__asm__ (".type hmIsAutoLog,@function");
__asm__ ("hmIsAutoLog:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1664(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl hmLogEntriesGet");
__asm__ (".type hmLogEntriesGet,@function");
__asm__ ("hmLogEntriesGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1668(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl hmErrorHandlerCreate");
__asm__ (".type hmErrorHandlerCreate,@function");
__asm__ ("hmErrorHandlerCreate:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1672(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl hmErrorHandlerEventGet");
__asm__ (".type hmErrorHandlerEventGet,@function");
__asm__ ("hmErrorHandlerEventGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1676(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl hmErrorHandlerTaskIdGet");
__asm__ (".type hmErrorHandlerTaskIdGet,@function");
__asm__ ("hmErrorHandlerTaskIdGet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1680(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");

__asm__ (".globl hmErrorHandlerParamSet");
__asm__ (".type hmErrorHandlerParamSet,@function");
__asm__ ("hmErrorHandlerParamSet:");
__asm__ ("lis   11,slTable@ha");
__asm__ ("lwz   11,slTable@l(11)");
__asm__ ("lwz   11,1684(11)");
__asm__ ("mtctr 11");
__asm__ ("bctr");


