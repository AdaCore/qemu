/*
 * entry point tables
 */

#include <slVersionLib.h>

/* entry point table "Cert" */

int CREATE_BLACKBOARD();
int DISPLAY_BLACKBOARD();
int READ_BLACKBOARD();
int CLEAR_BLACKBOARD();
int GET_BLACKBOARD_ID();
int GET_BLACKBOARD_STATUS();
int CREATE_BUFFER();
int SEND_BUFFER();
int RECEIVE_BUFFER();
int GET_BUFFER_ID();
int GET_BUFFER_STATUS();
int REPORT_APPLICATION_MESSAGE();
int CREATE_ERROR_HANDLER();
int GET_ERROR_STATUS();
int RAISE_APPLICATION_ERROR();
int CREATE_QUEUING_PORT();
int SEND_QUEUING_MESSAGE();
int RECEIVE_QUEUING_MESSAGE();
int GET_QUEUING_PORT_ID();
int GET_QUEUING_PORT_STATUS();
int CREATE_EVENT();
int SET_EVENT();
int RESET_EVENT();
int WAIT_EVENT();
int GET_EVENT_ID();
int GET_EVENT_STATUS();
int GET_PARTITION_STATUS();
int SET_PARTITION_MODE();
int SET_SCHEDULE_MODE();
int CREATE_SAMPLING_PORT();
int WRITE_SAMPLING_MESSAGE();
int READ_SAMPLING_MESSAGE();
int GET_SAMPLING_PORT_ID();
int GET_SAMPLING_PORT_STATUS();
int TIMED_WAIT();
int PERIODIC_WAIT();
int GET_TIME();
int REPLENISH();
int DELAYED_START();
int GET_PROCESS_ID();
int GET_MY_ID();
int GET_PROCESS_STATUS();
int CREATE_PROCESS();
int SET_PRIORITY();
int SUSPEND_SELF();
int SUSPEND();
int RESUME();
int STOP_SELF();
int STOP();
int START();
int LOCK_PREEMPTION();
int UNLOCK_PREEMPTION();
int CREATE_SEMAPHORE();
int WAIT_SEMAPHORE();
int SIGNAL_SEMAPHORE();
int GET_SEMAPHORE_ID();
int GET_SEMAPHORE_STATUS();
int CREATE_SAP_PORT();
int GET_SAP_PORT_ID();
int GET_SAP_PORT_STATUS();
int SEND_SAP_MESSAGE();
int RECEIVE_SAP_MESSAGE();
int apexTimeMonitorProcGet();
int procIdFromTaskIdGet();
int taskIdFromProcIdGet();
int __ashldi3();
int __cmpdi2();
int __ctypePtrGet();
int __div64();
int __divdi3();
int __errno();
int __fixdfdi();
int __fixunsdfdi();
int __floatdidf();
int __floatdisf();
int __localePtrGet();
int __lshrdi3();
int __moddi3();
int __rem64();
int __ucmpdi2();
int __udiv64();
int __udivdi3();
int __umoddi3();
int __urem64();
int _func_apexLockLevelGetPtrGet();
int _func_logMsgPtrGet();
int _func_procCreateHookFctPtrGet();
int _func_procStartHookFctPtrGet();
int _func_pthread_setcanceltypePtrGet();
int _func_sigTimeoutRecalcPtrGet();
int _func_tasknameRecordPtrGet();
int _msgQReceive();
int _msgQSend();
int _semTake();
int _semBQHeadTake();
int _taskDelay();
int _taskDelayAbs();
int _wdStart();
int _wdStartAbs();
int abs();
int acos();
int asin();
int atan2();
int atan();
int atof();
int atoi();
int atol();
int bcmp();
int bcopy();
int bcopyFast();
int bcopyBytes();
int bcopyLongs();
int bcopyWords();
int bfill();
int bfillBytes();
int bswap();
int bzero();
int bzeroFast();
int cbrt();
int calloc();
int ceil();
int chdir();
int classClassIdPtrGet();
int classInit();
int close();
int clock_gettime();
int clock_setres();
int clock_settime();
int clockLibInit();
int consoleFdPtrGet();
int cos();
int creat();
int div_r();
int dllAdd();
int dllCount();
int dllCreate();
int dllEach();
int dllGet();
int dllInit();
int dllInsert();
int dllRemove();
int errnoGet();
int errnoOfTaskGet();
int errnoOfTaskSet();
int errnoSet();
int evtActionPtrGet();
int excHookAdd();
int excJobAdd();
int exp();
int fabs();
int fdprintf();
int fdTablePtrGet();
int ffsMsb();
int fioFormatV();
int floor();
int fmod();
int fppProbe();
int fppRestore();
int fppSave();
int fppTaskRegsGet();
int fppTaskRegsSet();
int fpTypeGet();
int frexp();
int fstat();
int getcwd();
int gmtime();
int gmtime_r();
int index();
int intContext();
int intCount();
int intCntPtrGet();
int intLock();
int intLockLevelSet();
int intUnlock();
int ioDefPathGet();
int ioDefPathSet();
int ioGlobalStdGet();
int ioGlobalStdSet();
int ioTaskStdGet();
int ioTaskStdSet();
int ioctl();
int iosDevAdd();
int iosDevFind();
int iosDrvInstall();
int iosFdFreeHookRtnPtrGet();
int iosFdNewHookRtnPtrGet();
int iosFdValue();
int iosLibInitializedPtrGet();
int iosRelinquish();
int isalnum();
int isalpha();
int iscntrl();
int isdigit();
int isgraph();
int islower();
int isprint();
int ispunct();
int isspace();
int isupper();
int isxdigit();
int kill();
int kernelStatePtrGet();
int kernelTimeSlice();
int kernelVersion();
int labs();
int ldiv_r();
int ldexp();
int localeconv();
int log();
int log10();
int logMsg();
int longjmp();
int lseek();
int malloc();
int maxDriversPtrGet();
int maxFilesPtrGet();
int memalign();
int memAddToPool();
int memchr();
int memcmp();
int memcpy();
int memmove();
int memPartAddToPool();
int memPartAlignedAlloc();
int memPartAlloc();
int memPartAllocDisable();
int memPartCreate();
int memPartInit();
int memset();
int memSysPartIdPtrGet();
int msgQClassIdPtrGet();
int msgQCreate();
int msgQInit();
int msgQNumMsgs();
int msgQReceive();
int msgQSend();
int modf();
int objAlloc();
int objAllocExtra();
int objCoreInit();
int open();
int partitionNormalModeSet();
int partitionSwitchInHookAdd();
int pathCat();
int pow();
int printErr();
int printf();
int procCreateHookAdd();
int procStartHookAdd();
int pTaskLastDspTcbPtrGet();
int pTaskLastFpTcbPtrGet();
int qFifoClassIdPtrGet();
int qInit();
int qPriListClassIdPtrGet();
int raise();
int read();
int readyQHeadPtrGet();
int reboot();
int rebootHookAdd();
int remove();
int rename();
int rngBufGet();
int rngBufPut();
int rngCreate();
int rngFlush();
int rngFreeBytes();
int rngIsEmpty();
int rngIsFull();
int rngMoveAhead();
int rngNBytes();
int rngPutAhead();
int roundRobinOnPtrGet();
int roundRobinSlicePtrGet();
int sdRgnAddrGet();
int sdRgnInfoGet();
int sdRgnAllGet();
int sdRgnFirstGet();
int sdRgnNextGet();
int semBCreate();
int semBInit();
int semCCreate();
int semCInit();
int semClassIdPtrGet();
int semCLibInit();
int semEvStart();
int semEvStop();
int semFlush();
int semGive();
int semMCreate();
int semMInit();
int semTake();
int setjmp();
int setlocale();
int sigaction();
int sigaddset();
int sigdelset();
int sigemptyset();
int signal();
int sigPendInit();
int sigPendKill();
int sigprocmask();
int sigsetmask();
int sigvec();
int sin();
int sllCreate();
int sllGet();
int sllInit();
int sllPutAtHead();
int sllPutAtTail();
int sllRemove();
int sprintf();
int sqrt();
int sscanf();
int statSymTblPtrGet();
int strcat();
int strchr();
int strcmp();
int strcpy();
int strlen();
int strncat();
int strncmp();
int strncpy();
int strpbrk();
int strrchr();
int strspn();
int strstr();
int strtod();
int strtok_r();
int strtol();
int strtoul();
int sysClkRateGet();
int sysSymTblPtrGet();
int sysTimestamp64DiffToNsec();
int sysTimestamp64Get();
int sysToMonitor();
int swab();
int tan();
int taskActivate();
int taskArgsGet();
int taskClassIdPtrGet();
int taskCreat();
int taskCreateHookAdd();
int taskDelay();
int taskExitHookPtrGet();
int taskIdCurrentPtrGet();
int taskIdDefault();
int taskIdListGet();
int taskIdSelf();
int taskIdVerify();
int taskInfoGet();
int taskInit();
int taskLock();
int taskName();
int taskOptionsGet();
int taskPcGet();
int taskPriorityGet();
int taskPrioritySet();
int taskReset();
int taskResume();
int taskSafe();
int taskSafeWait();
int taskSpawn();
int taskSuspend();
int taskSwapHookAdd();
int taskSwitchHookAdd();
int taskTcb();
int taskUndelay();
int taskUnlock();
int taskUnsafe();
int taskVarAdd();
int taskVarGet();
int taskVarInit();
int taskVarSet();
int tick64Get();
int tickQHeadPtrGet();
int timeMonitorClear();
int timeMonitorStart();
int timeMonitorStop();
int timeMonitorTaskGet();
int timeMonitorSysGet();
int tolower();
int toupper();
int trgEvtClassPtrGet();
int unlink();
int uswab();
int valloc();
int vprintf();
int vsprintf();
int vThreadsEventHandlerRegister();
int vThreadsOsInvoke();
int vThreadsOsInvokeCustom();
int vThreadsSysInfoPtrGet();
int vxCurrentPcGet();
int vxMemProbe();
int vxTas();
int vxTicksPtrGet();
int vxAbsTicksPtrGet();
int vxJobAddTicksPtrGet();
int wdCancel();
int wdCreate();
int wdStart();
int windExit();
int windPendQFlush();
int windPendQGet();
int windPendQPut();
int windPendQRemove();
int windReadyQPut();
int windSuspend();
int windTickRecalibrate();
int workQAdd1();
int workQAdd2();
int write();
int wvEvtClassPtrGet();
int configRecordFieldGet();
int hmEventInject();
int hmEventLog();
int hmIsAutoLog();
int hmLogEntriesGet();
int hmErrorHandlerCreate();
int hmErrorHandlerEventGet();
int hmErrorHandlerTaskIdGet();
int hmErrorHandlerParamSet();

static void * const slTable0[] __attribute__((section(".rodata"))) = {

	/*    0 */ CREATE_BLACKBOARD,
	/*    1 */ DISPLAY_BLACKBOARD,
	/*    2 */ READ_BLACKBOARD,
	/*    3 */ CLEAR_BLACKBOARD,
	/*    4 */ GET_BLACKBOARD_ID,
	/*    5 */ GET_BLACKBOARD_STATUS,
	/*    6 */ CREATE_BUFFER,
	/*    7 */ SEND_BUFFER,
	/*    8 */ RECEIVE_BUFFER,
	/*    9 */ GET_BUFFER_ID,
	/*   10 */ GET_BUFFER_STATUS,
	/*   11 */ REPORT_APPLICATION_MESSAGE,
	/*   12 */ CREATE_ERROR_HANDLER,
	/*   13 */ GET_ERROR_STATUS,
	/*   14 */ RAISE_APPLICATION_ERROR,
	/*   15 */ CREATE_QUEUING_PORT,
	/*   16 */ SEND_QUEUING_MESSAGE,
	/*   17 */ RECEIVE_QUEUING_MESSAGE,
	/*   18 */ GET_QUEUING_PORT_ID,
	/*   19 */ GET_QUEUING_PORT_STATUS,
	/*   20 */ CREATE_EVENT,
	/*   21 */ SET_EVENT,
	/*   22 */ RESET_EVENT,
	/*   23 */ WAIT_EVENT,
	/*   24 */ GET_EVENT_ID,
	/*   25 */ GET_EVENT_STATUS,
	/*   26 */ GET_PARTITION_STATUS,
	/*   27 */ SET_PARTITION_MODE,
	/*   28 */ SET_SCHEDULE_MODE,
	/*   29 */ CREATE_SAMPLING_PORT,
	/*   30 */ WRITE_SAMPLING_MESSAGE,
	/*   31 */ READ_SAMPLING_MESSAGE,
	/*   32 */ GET_SAMPLING_PORT_ID,
	/*   33 */ GET_SAMPLING_PORT_STATUS,
	/*   34 */ TIMED_WAIT,
	/*   35 */ PERIODIC_WAIT,
	/*   36 */ GET_TIME,
	/*   37 */ REPLENISH,
	/*   38 */ DELAYED_START,
	/*   39 */ GET_PROCESS_ID,
	/*   40 */ GET_MY_ID,
	/*   41 */ GET_PROCESS_STATUS,
	/*   42 */ CREATE_PROCESS,
	/*   43 */ SET_PRIORITY,
	/*   44 */ SUSPEND_SELF,
	/*   45 */ SUSPEND,
	/*   46 */ RESUME,
	/*   47 */ STOP_SELF,
	/*   48 */ STOP,
	/*   49 */ START,
	/*   50 */ LOCK_PREEMPTION,
	/*   51 */ UNLOCK_PREEMPTION,
	/*   52 */ CREATE_SEMAPHORE,
	/*   53 */ WAIT_SEMAPHORE,
	/*   54 */ SIGNAL_SEMAPHORE,
	/*   55 */ GET_SEMAPHORE_ID,
	/*   56 */ GET_SEMAPHORE_STATUS,
	/*   57 */ CREATE_SAP_PORT,
	/*   58 */ GET_SAP_PORT_ID,
	/*   59 */ GET_SAP_PORT_STATUS,
	/*   60 */ SEND_SAP_MESSAGE,
	/*   61 */ RECEIVE_SAP_MESSAGE,
	/*   62 */ apexTimeMonitorProcGet,
	/*   63 */ procIdFromTaskIdGet,
	/*   64 */ taskIdFromProcIdGet,
	/*   65 */ __ashldi3,
	/*   66 */ __cmpdi2,
	/*   67 */ __ctypePtrGet,
	/*   68 */ __div64,
	/*   69 */ __divdi3,
	/*   70 */ __errno,
	/*   71 */ __fixdfdi,
	/*   72 */ __fixunsdfdi,
	/*   73 */ __floatdidf,
	/*   74 */ __floatdisf,
	/*   75 */ __localePtrGet,
	/*   76 */ __lshrdi3,
	/*   77 */ __moddi3,
	/*   78 */ __rem64,
	/*   79 */ __ucmpdi2,
	/*   80 */ __udiv64,
	/*   81 */ __udivdi3,
	/*   82 */ __umoddi3,
	/*   83 */ __urem64,
	/*   84 */ _func_apexLockLevelGetPtrGet,
	/*   85 */ _func_logMsgPtrGet,
	/*   86 */ _func_procCreateHookFctPtrGet,
	/*   87 */ _func_procStartHookFctPtrGet,
	/*   88 */ _func_pthread_setcanceltypePtrGet,
	/*   89 */ _func_sigTimeoutRecalcPtrGet,
	/*   90 */ _func_tasknameRecordPtrGet,
	/*   91 */ _msgQReceive,
	/*   92 */ _msgQSend,
	/*   93 */ _semTake,
	/*   94 */ _semBQHeadTake,
	/*   95 */ _taskDelay,
	/*   96 */ _taskDelayAbs,
	/*   97 */ _wdStart,
	/*   98 */ _wdStartAbs,
	/*   99 */ abs,
	/*  100 */ acos,
	/*  101 */ asin,
	/*  102 */ atan2,
	/*  103 */ atan,
	/*  104 */ atof,
	/*  105 */ atoi,
	/*  106 */ atol,
	/*  107 */ bcmp,
	/*  108 */ bcopy,
	/*  109 */ bcopyFast,
	/*  110 */ bcopyBytes,
	/*  111 */ bcopyLongs,
	/*  112 */ bcopyWords,
	/*  113 */ bfill,
	/*  114 */ bfillBytes,
	/*  115 */ bswap,
	/*  116 */ bzero,
	/*  117 */ bzeroFast,
	/*  118 */ cbrt,
	/*  119 */ calloc,
	/*  120 */ ceil,
	/*  121 */ chdir,
	/*  122 */ classClassIdPtrGet,
	/*  123 */ classInit,
	/*  124 */ close,
	/*  125 */ clock_gettime,
	/*  126 */ clock_setres,
	/*  127 */ clock_settime,
	/*  128 */ clockLibInit,
	/*  129 */ consoleFdPtrGet,
	/*  130 */ cos,
	/*  131 */ creat,
	/*  132 */ div_r,
	/*  133 */ dllAdd,
	/*  134 */ dllCount,
	/*  135 */ dllCreate,
	/*  136 */ dllEach,
	/*  137 */ dllGet,
	/*  138 */ dllInit,
	/*  139 */ dllInsert,
	/*  140 */ dllRemove,
	/*  141 */ errnoGet,
	/*  142 */ errnoOfTaskGet,
	/*  143 */ errnoOfTaskSet,
	/*  144 */ errnoSet,
	/*  145 */ evtActionPtrGet,
	/*  146 */ excHookAdd,
	/*  147 */ excJobAdd,
	/*  148 */ exp,
	/*  149 */ fabs,
	/*  150 */ fdprintf,
	/*  151 */ fdTablePtrGet,
	/*  152 */ ffsMsb,
	/*  153 */ fioFormatV,
	/*  154 */ floor,
	/*  155 */ fmod,
	/*  156 */ fppProbe,
	/*  157 */ fppRestore,
	/*  158 */ fppSave,
	/*  159 */ fppTaskRegsGet,
	/*  160 */ fppTaskRegsSet,
	/*  161 */ fpTypeGet,
	/*  162 */ frexp,
	/*  163 */ fstat,
	/*  164 */ getcwd,
	/*  165 */ gmtime,
	/*  166 */ gmtime_r,
	/*  167 */ index,
	/*  168 */ intContext,
	/*  169 */ intCount,
	/*  170 */ intCntPtrGet,
	/*  171 */ intLock,
	/*  172 */ intLockLevelSet,
	/*  173 */ intUnlock,
	/*  174 */ ioDefPathGet,
	/*  175 */ ioDefPathSet,
	/*  176 */ ioGlobalStdGet,
	/*  177 */ ioGlobalStdSet,
	/*  178 */ ioTaskStdGet,
	/*  179 */ ioTaskStdSet,
	/*  180 */ ioctl,
	/*  181 */ iosDevAdd,
	/*  182 */ iosDevFind,
	/*  183 */ iosDrvInstall,
	/*  184 */ iosFdFreeHookRtnPtrGet,
	/*  185 */ iosFdNewHookRtnPtrGet,
	/*  186 */ iosFdValue,
	/*  187 */ iosLibInitializedPtrGet,
	/*  188 */ iosRelinquish,
	/*  189 */ isalnum,
	/*  190 */ isalpha,
	/*  191 */ iscntrl,
	/*  192 */ isdigit,
	/*  193 */ isgraph,
	/*  194 */ islower,
	/*  195 */ isprint,
	/*  196 */ ispunct,
	/*  197 */ isspace,
	/*  198 */ isupper,
	/*  199 */ isxdigit,
	/*  200 */ kill,
	/*  201 */ kernelStatePtrGet,
	/*  202 */ kernelTimeSlice,
	/*  203 */ kernelVersion,
	/*  204 */ labs,
	/*  205 */ ldiv_r,
	/*  206 */ ldexp,
	/*  207 */ localeconv,
	/*  208 */ log,
	/*  209 */ log10,
	/*  210 */ logMsg,
	/*  211 */ longjmp,
	/*  212 */ lseek,
	/*  213 */ malloc,
	/*  214 */ maxDriversPtrGet,
	/*  215 */ maxFilesPtrGet,
	/*  216 */ memalign,
	/*  217 */ memAddToPool,
	/*  218 */ memchr,
	/*  219 */ memcmp,
	/*  220 */ memcpy,
	/*  221 */ memmove,
	/*  222 */ memPartAddToPool,
	/*  223 */ memPartAlignedAlloc,
	/*  224 */ memPartAlloc,
	/*  225 */ memPartAllocDisable,
	/*  226 */ memPartCreate,
	/*  227 */ memPartInit,
	/*  228 */ memset,
	/*  229 */ memSysPartIdPtrGet,
	/*  230 */ msgQClassIdPtrGet,
	/*  231 */ msgQCreate,
	/*  232 */ msgQInit,
	/*  233 */ msgQNumMsgs,
	/*  234 */ msgQReceive,
	/*  235 */ msgQSend,
	/*  236 */ modf,
	/*  237 */ objAlloc,
	/*  238 */ objAllocExtra,
	/*  239 */ objCoreInit,
	/*  240 */ open,
	/*  241 */ partitionNormalModeSet,
	/*  242 */ partitionSwitchInHookAdd,
	/*  243 */ pathCat,
	/*  244 */ pow,
	/*  245 */ printErr,
	/*  246 */ printf,
	/*  247 */ procCreateHookAdd,
	/*  248 */ procStartHookAdd,
	/*  249 */ pTaskLastDspTcbPtrGet,
	/*  250 */ pTaskLastFpTcbPtrGet,
	/*  251 */ qFifoClassIdPtrGet,
	/*  252 */ qInit,
	/*  253 */ qPriListClassIdPtrGet,
	/*  254 */ raise,
	/*  255 */ read,
	/*  256 */ readyQHeadPtrGet,
	/*  257 */ reboot,
	/*  258 */ rebootHookAdd,
	/*  259 */ remove,
	/*  260 */ rename,
	/*  261 */ rngBufGet,
	/*  262 */ rngBufPut,
	/*  263 */ rngCreate,
	/*  264 */ rngFlush,
	/*  265 */ rngFreeBytes,
	/*  266 */ rngIsEmpty,
	/*  267 */ rngIsFull,
	/*  268 */ rngMoveAhead,
	/*  269 */ rngNBytes,
	/*  270 */ rngPutAhead,
	/*  271 */ roundRobinOnPtrGet,
	/*  272 */ roundRobinSlicePtrGet,
	/*  273 */ sdRgnAddrGet,
	/*  274 */ sdRgnInfoGet,
	/*  275 */ sdRgnAllGet,
	/*  276 */ sdRgnFirstGet,
	/*  277 */ sdRgnNextGet,
	/*  278 */ semBCreate,
	/*  279 */ semBInit,
	/*  280 */ semCCreate,
	/*  281 */ semCInit,
	/*  282 */ semClassIdPtrGet,
	/*  283 */ semCLibInit,
	/*  284 */ semEvStart,
	/*  285 */ semEvStop,
	/*  286 */ semFlush,
	/*  287 */ semGive,
	/*  288 */ semMCreate,
	/*  289 */ semMInit,
	/*  290 */ semTake,
	/*  291 */ setjmp,
	/*  292 */ setlocale,
	/*  293 */ sigaction,
	/*  294 */ sigaddset,
	/*  295 */ sigdelset,
	/*  296 */ sigemptyset,
	/*  297 */ signal,
	/*  298 */ sigPendInit,
	/*  299 */ sigPendKill,
	/*  300 */ sigprocmask,
	/*  301 */ sigsetmask,
	/*  302 */ sigvec,
	/*  303 */ sin,
	/*  304 */ sllCreate,
	/*  305 */ sllGet,
	/*  306 */ sllInit,
	/*  307 */ sllPutAtHead,
	/*  308 */ sllPutAtTail,
	/*  309 */ sllRemove,
	/*  310 */ sprintf,
	/*  311 */ sqrt,
	/*  312 */ sscanf,
	/*  313 */ statSymTblPtrGet,
	/*  314 */ strcat,
	/*  315 */ strchr,
	/*  316 */ strcmp,
	/*  317 */ strcpy,
	/*  318 */ strlen,
	/*  319 */ strncat,
	/*  320 */ strncmp,
	/*  321 */ strncpy,
	/*  322 */ strpbrk,
	/*  323 */ strrchr,
	/*  324 */ strspn,
	/*  325 */ strstr,
	/*  326 */ strtod,
	/*  327 */ strtok_r,
	/*  328 */ strtol,
	/*  329 */ strtoul,
	/*  330 */ sysClkRateGet,
	/*  331 */ sysSymTblPtrGet,
	/*  332 */ sysTimestamp64DiffToNsec,
	/*  333 */ sysTimestamp64Get,
	/*  334 */ sysToMonitor,
	/*  335 */ swab,
	/*  336 */ tan,
	/*  337 */ taskActivate,
	/*  338 */ taskArgsGet,
	/*  339 */ taskClassIdPtrGet,
	/*  340 */ taskCreat,
	/*  341 */ taskCreateHookAdd,
	/*  342 */ taskDelay,
	/*  343 */ taskExitHookPtrGet,
	/*  344 */ taskIdCurrentPtrGet,
	/*  345 */ taskIdDefault,
	/*  346 */ taskIdListGet,
	/*  347 */ taskIdSelf,
	/*  348 */ taskIdVerify,
	/*  349 */ taskInfoGet,
	/*  350 */ taskInit,
	/*  351 */ taskLock,
	/*  352 */ taskName,
	/*  353 */ taskOptionsGet,
	/*  354 */ taskPcGet,
	/*  355 */ taskPriorityGet,
	/*  356 */ taskPrioritySet,
	/*  357 */ taskReset,
	/*  358 */ taskResume,
	/*  359 */ taskSafe,
	/*  360 */ taskSafeWait,
	/*  361 */ taskSpawn,
	/*  362 */ taskSuspend,
	/*  363 */ taskSwapHookAdd,
	/*  364 */ taskSwitchHookAdd,
	/*  365 */ taskTcb,
	/*  366 */ taskUndelay,
	/*  367 */ taskUnlock,
	/*  368 */ taskUnsafe,
	/*  369 */ taskVarAdd,
	/*  370 */ taskVarGet,
	/*  371 */ taskVarInit,
	/*  372 */ taskVarSet,
	/*  373 */ tick64Get,
	/*  374 */ tickQHeadPtrGet,
	/*  375 */ timeMonitorClear,
	/*  376 */ timeMonitorStart,
	/*  377 */ timeMonitorStop,
	/*  378 */ timeMonitorTaskGet,
	/*  379 */ timeMonitorSysGet,
	/*  380 */ tolower,
	/*  381 */ toupper,
	/*  382 */ trgEvtClassPtrGet,
	/*  383 */ unlink,
	/*  384 */ uswab,
	/*  385 */ valloc,
	/*  386 */ vprintf,
	/*  387 */ vsprintf,
	/*  388 */ vThreadsEventHandlerRegister,
	/*  389 */ vThreadsOsInvoke,
	/*  390 */ vThreadsOsInvokeCustom,
	/*  391 */ vThreadsSysInfoPtrGet,
	/*  392 */ vxCurrentPcGet,
	/*  393 */ vxMemProbe,
	/*  394 */ vxTas,
	/*  395 */ vxTicksPtrGet,
	/*  396 */ vxAbsTicksPtrGet,
	/*  397 */ vxJobAddTicksPtrGet,
	/*  398 */ wdCancel,
	/*  399 */ wdCreate,
	/*  400 */ wdStart,
	/*  401 */ windExit,
	/*  402 */ windPendQFlush,
	/*  403 */ windPendQGet,
	/*  404 */ windPendQPut,
	/*  405 */ windPendQRemove,
	/*  406 */ windReadyQPut,
	/*  407 */ windSuspend,
	/*  408 */ windTickRecalibrate,
	/*  409 */ workQAdd1,
	/*  410 */ workQAdd2,
	/*  411 */ write,
	/*  412 */ wvEvtClassPtrGet,
	/*  413 */ configRecordFieldGet,
	/*  414 */ hmEventInject,
	/*  415 */ hmEventLog,
	/*  416 */ hmIsAutoLog,
	/*  417 */ hmLogEntriesGet,
	/*  418 */ hmErrorHandlerCreate,
	/*  419 */ hmErrorHandlerEventGet,
	/*  420 */ hmErrorHandlerTaskIdGet,
	/*  421 */ hmErrorHandlerParamSet,

};

static SL_VERSION slVersion0 =
	{
	8, "vThreads",
	4, "Cert"
	};

/*
 * entry point table initialization
 */

static void epInit (void) {
	vxRegisterOrLink(SLVERSION_REGISTER, &slVersion0, (void*) slTable0);
}

static const void * init __attribute__((section(".ctors.stubs"),used)) = epInit;

