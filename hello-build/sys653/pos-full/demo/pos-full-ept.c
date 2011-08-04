/*
 * entry point tables
 */

#include <slVersionLib.h>

/* entry point table "app" */

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
int __assert();
int __cmpdi2();
int __ctypePtrGet();
int __div64();
int __divdi3();
int __fixdfdi();
int __fixunsdfdi();
int __floatdidf();
int __floatdisf();
int __ieee754_fmod();
int __ieee754_pow();
int __ieee754_remainder();
int __ieee754_sqrt();
int __errno();
int __localePtrGet();
int __lshrdi3();
int __moddi3();
int __rem64();
int __srget();
int __stdin();
int __stdout();
int __stderr();
int __swbuf();
int __ucmpdi2();
int __udiv64();
int __udivdi3();
int __umoddi3();
int __urem64();
int _func_apexLockLevelGetPtrGet();
int _func_evtLogM1PtrGet();
int _func_evtLogOIntLockPtrGet();
int _func_logMsgPtrGet();
int _func_procCreateHookFctPtrGet();
int _func_procStartHookFctPtrGet();
int _func_pthread_setcanceltypePtrGet();
int _func_sigTimeoutRecalcPtrGet();
int _func_tasknameRecordPtrGet();
int _func_trgCheckPtrGet();
int _func_symEachPtrGet();
int _func_symFindByNamePtrGet();
int _msgQReceive();
int _msgQSend();
int _semTake();
int _semBQHeadTake();
int _taskDelay();
int _taskDelayAbs();
int _wdStart();
int _wdStartAbs();
int abort();
int abs();
int acos();
int acosh();
int apexInitializedPtrGet();
int asctime();
int asctime_r();
int asin();
int asinh();
int atan2();
int atan();
int atanh();
int atexit();
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
int binvert();
int bsearch();
int bswap();
int bzero();
int bzeroFast();
int cabs();
int cbrt();
int calloc();
int ceil();
int cfree();
int chdir();
int checksum();
int classClassIdPtrGet();
int classInit();
int classInstrument();
int classMemPartIdSet();
int classShowConnect();
int close();
int clearerr();
int clock();
int clock_getres();
int clock_gettime();
int clock_setres();
int clock_settime();
int clock_show();
int clockLibInit();
int closedir();
int consoleFdPtrGet();
int cos();
int cosh();
int creat();
int ctime();
int ctime_r();
int difftime();
int div();
int div_r();
int dllAdd();
int dllCount();
int dllCreate();
int dllDelete();
int dllEach();
int dllGet();
int dllInit();
int dllInsert();
int dllRemove();
int dllTerminate();
int envPrivateCreate();
int envPrivateDestroy();
int envShow();
int errnoGet();
int errnoOfTaskGet();
int errnoOfTaskSet();
int errnoSet();
int evtActionPtrGet();
int excHookAdd();
int excJobAdd();
int exit();
int exp();
int fabs();
int fclose();
int fdopen();
int fdprintf();
int fdTablePtrGet();
int feof();
int ferror();
int fflush();
int ffsMsb();
int fgetc();
int fgetpos();
int fgets();
int fileno();
int fioFormatV();
int fioRdString();
int fioRead();
int floor();
int fmod();
int fopen();
int fprintf();
int fputc();
int fputs();
int fread();
int freopen();
int fpClassIdPtrGet();
int fppProbe();
int fppRestore();
int fppSave();
int fppTaskRegsGet();
int fppTaskRegsSet();
int fpTypeGet();
int free();
int frexp();
int fscanf();
int fseek();
int fsetpos();
int fstat();
int fstatfs();
int ftell();
int fwrite();
int getc();
int getchar();
int getcwd();
int getenv();
int gets();
int getw();
int getwd();
int gmtime();
int gmtime_r();
int hashFuncIterScale();
int hashFuncModulo();
int hashFuncMultiply();
int hashKeyCmp();
int hashKeyStrCmp();
int hashTblCreate();
int hashTblDelete();
int hashTblDestroy();
int hashTblEach();
int hashTblFind();
int hashTblInit();
int hashTblPut();
int hashTblRemove();
int hashTblTerminate();
int hypot();
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
int iosDevDelete();
int iosDevFind();
int iosDrvInstall();
int iosDrvRemove();
int iosFdFreeHookRtnPtrGet();
int iosFdNewHookRtnPtrGet();
int iosFdValue();
int iosLibInitializedPtrGet();
int iosRelinquish();
int isalnum();
int isalpha();
int isatty();
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
int ldiv();
int ldiv_r();
int ldexp();
int localeconv();
int localtime();
int localtime_r();
int log();
int log10();
int logFdAdd();
int logFdDelete();
int logFdSet();
int logMsg();
int longjmp();
int lseek();
int lstAdd();
int lstConcat();
int lstCount();
int lstDelete();
int lstExtract();
int lstFind();
int lstFirst();
int lstFree();
int lstGet();
int lstInit();
int lstInsert();
int lstLast();
int lstNStep();
int lstNext();
int lstNth();
int lstPrevious();
int malloc();
int maxDriversPtrGet();
int maxFilesPtrGet();
int mblen();
int mbstowcs();
int mbtowc();
int memalign();
int memAddToPool();
int memchr();
int memcmp();
int memcpy();
int memDevCreate();
int memDevCreateDir();
int memDevDelete();
int memDrv();
int memFindMax();
int memmove();
int memOptionsSet();
int memPartAddToPool();
int memPartAlignedAlloc();
int memPartAlloc();
int memPartAllocDisable();
int memPartCreate();
int memPartFree();
int memPartFindMax();
int memPartInit();
int memPartOptionsSet();
int memPartRealloc();
int memset();
int memSysPartIdPtrGet();
int mktime();
int msgQClassIdPtrGet();
int msgQCreate();
int msgQDelete();
int msgQInfoGet();
int msgQInit();
int msgQNumMsgs();
int msgQReceive();
int msgQSend();
int modf();
int objAlloc();
int objAllocExtra();
int objCoreInit();
int objCoreTerminate();
int objFree();
int objShow();
int open();
int opendir();
int partitionNormalModeSet();
int partitionSwitchInHookAdd();
int pathBuild();
int pathCat();
int pathCondense();
int pathLastName();
int pathLastNamePtr();
int pathParse();
int pathSplit();
int pause();
int perror();
int pipeDevCreate();
int pipeDevDelete();
int pipeDrv();
int pow();
int printErr();
int printf();
int procCreateHookAdd();
int procCreateHookDelete();
int procStartHookAdd();
int procStartHookDelete();
int pTaskLastDspTcbPtrGet();
int pTaskLastFpTcbPtrGet();
int putc();
int putchar();
int putenv();
int puts();
int putw();
int qFifoClassIdPtrGet();
int qFirst();
int qInit();
int qPriListClassIdPtrGet();
int qsort();
int ramDevCreate();
int ramDrv();
int rand();
int raise();
int read();
int readdir();
int readyQHeadPtrGet();
int realloc();
int reboot();
int rebootHookAdd();
int remove();
int rename();
int rewind();
int rewinddir();
int rindex();
int rint();
int rngBufGet();
int rngBufPut();
int rngCreate();
int rngDelete();
int rngFlush();
int rngFreeBytes();
int rngIsEmpty();
int rngIsFull();
int rngMoveAhead();
int rngNBytes();
int rngPutAhead();
int roundRobinOnPtrGet();
int roundRobinSlicePtrGet();
int scanf();
int sdRgnAddrGet();
int sdRgnInfoGet();
int sdRgnAllGet();
int sdRgnFirstGet();
int sdRgnNextGet();
int select();
int selNodeAdd();
int selNodeDelete();
int selWakeup();
int selWakeupAll();
int selWakeupListInit();
int selWakeupListLen();
int selWakeupType();
int semBCreate();
int semBInit();
int semCCreate();
int semCInit();
int semClassIdPtrGet();
int semClear();
int semCLibInit();
int semCreate();
int semDelete();
int semEvStart();
int semEvStop();
int semFlush();
int semGive();
int semInfo();
int semMCreate();
int semMGiveForce();
int semMInit();
int semTake();
int setbuf();
int setbuffer();
int setjmp();
int setlinebuf();
int setlocale();
int setvbuf();
int sigaction();
int sigaddset();
int sigblock();
int sigdelset();
int sigemptyset();
int sigfillset();
int sigismember();
int signal();
int sigPendDestroy();
int sigpending();
int sigPendInit();
int sigPendKill();
int sigprocmask();
int sigqueue();
int sigqueueInit();
int sigsetmask();
int sigsetjmp();
int sigsuspend();
int sigtimedwait();
int sigvec();
int sigwait();
int sigwaitinfo();
int sin();
int sinh();
int sllCount();
int sllCreate();
int sllDelete();
int sllEach();
int sllGet();
int sllInit();
int sllPrevious();
int sllPutAtHead();
int sllPutAtTail();
int sllRemove();
int sllTerminate();
int sprintf();
int sqrt();
int srand();
int sscanf();
int stat();
int statfs();
int statSymTblPtrGet();
int stdioFp();
int strcat();
int strchr();
int strcmp();
int strcoll();
int strcpy();
int strcspn();
int strerror();
int strerror_r();
int strftime();
int strlen();
int strncat();
int strncmp();
int strncpy();
int strpbrk();
int strrchr();
int strspn();
int strstr();
int strtod();
int strtok();
int strtok_r();
int strtol();
int strtoul();
int strxfrm();
int symAdd();
int symFindByName();
int symFindByNameAndType();
int symFindByValue();
int symFindByValueAndType();
int symLibInit();
int symRemove();
int symTblAdd();
int symTblClassIdPtrGet();
int symTblCreate();
int symTblDelete();
int symTblLock();
int symTblRemove();
int symTblUnlock();
int sysClkRateGet();
int sysSymTblPtrGet();
int system();
int sysTimestamp64DiffToNsec();
int sysTimestamp64Get();
int sysToMonitor();
int swab();
int tan();
int tanh();
int taskActivate();
int taskArgsGet();
int taskClassIdPtrGet();
int taskCreat();
int taskCreateHookAdd();
int taskCreateHookDelete();
int taskDelay();
int taskDelete();
int taskDeleteForce();
int taskDeleteHookAdd();
int taskDeleteHookDelete();
int taskExitHookPtrGet();
int taskIdCurrentPtrGet();
int taskIdDefault();
int taskIdListGet();
int taskIsReady();
int taskIdSelf();
int taskIsSuspended();
int taskIdVerify();
int taskInfoGet();
int taskInit();
int taskLock();
int taskName();
int taskNameToId();
int taskOptionsGet();
int taskOptionsSet();
int taskPcGet();
int taskPriorityGet();
int taskPrioritySet();
int taskRegsGet();
int taskRegsSet();
int taskReset();
int taskRestart();
int taskResume();
int taskSafe();
int taskSafeWait();
int taskSpawn();
int taskSuspend();
int taskSwapHookAdd();
int taskSwapHookDelete();
int taskSwitchHookAdd();
int taskSwitchHookDelete();
int taskTcb();
int taskUndelay();
int taskUnlock();
int taskUnsafe();
int taskVarAdd();
int taskVarDelete();
int taskVarGet();
int taskVarInfo();
int taskVarInit();
int taskVarSet();
int tick64Get();
int tickGet();
int tickQHeadPtrGet();
int time();
int timeMonitorClear();
int timeMonitorStart();
int timeMonitorStop();
int timeMonitorTaskGet();
int timeMonitorSysGet();
int tmpfile();
int tmpnam();
int tolower();
int toupper();
int trgEvtClassPtrGet();
int ungetc();
int unlink();
int uswab();
int utime();
int valloc();
int vfdprintf();
int vfprintf();
int vprintf();
int vsprintf();
int vThreadsDbgPrint();
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
int wcstombs();
int wctomb();
int wdCancel();
int wdCreate();
int wdDelete();
int wdStart();
int windDelete();
int windExit();
int windPendQFlush();
int windPendQGet();
int windPendQPut();
int windPendQRemove();
int windPendQTerminate();
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
int hmErrorHandlerSuspend();
int hmErrorHandlerResume();
int hmErrorHandlerDelete();
int hmErrorHandlerParamSet();
int ppGlobalEnvironPtrGet();

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
	/*   66 */ __assert,
	/*   67 */ __cmpdi2,
	/*   68 */ __ctypePtrGet,
	/*   69 */ __div64,
	/*   70 */ __divdi3,
	/*   71 */ __fixdfdi,
	/*   72 */ __fixunsdfdi,
	/*   73 */ __floatdidf,
	/*   74 */ __floatdisf,
	/*   75 */ __ieee754_fmod,
	/*   76 */ __ieee754_pow,
	/*   77 */ __ieee754_remainder,
	/*   78 */ __ieee754_sqrt,
	/*   79 */ __errno,
	/*   80 */ __localePtrGet,
	/*   81 */ __lshrdi3,
	/*   82 */ __moddi3,
	/*   83 */ __rem64,
	/*   84 */ __srget,
	/*   85 */ __stdin,
	/*   86 */ __stdout,
	/*   87 */ __stderr,
	/*   88 */ __swbuf,
	/*   89 */ __ucmpdi2,
	/*   90 */ __udiv64,
	/*   91 */ __udivdi3,
	/*   92 */ __umoddi3,
	/*   93 */ __urem64,
	/*   94 */ _func_apexLockLevelGetPtrGet,
	/*   95 */ _func_evtLogM1PtrGet,
	/*   96 */ _func_evtLogOIntLockPtrGet,
	/*   97 */ _func_logMsgPtrGet,
	/*   98 */ _func_procCreateHookFctPtrGet,
	/*   99 */ _func_procStartHookFctPtrGet,
	/*  100 */ _func_pthread_setcanceltypePtrGet,
	/*  101 */ _func_sigTimeoutRecalcPtrGet,
	/*  102 */ _func_tasknameRecordPtrGet,
	/*  103 */ _func_trgCheckPtrGet,
	/*  104 */ _func_symEachPtrGet,
	/*  105 */ _func_symFindByNamePtrGet,
	/*  106 */ _msgQReceive,
	/*  107 */ _msgQSend,
	/*  108 */ _semTake,
	/*  109 */ _semBQHeadTake,
	/*  110 */ _taskDelay,
	/*  111 */ _taskDelayAbs,
	/*  112 */ _wdStart,
	/*  113 */ _wdStartAbs,
	/*  114 */ abort,
	/*  115 */ abs,
	/*  116 */ acos,
	/*  117 */ acosh,
	/*  118 */ apexInitializedPtrGet,
	/*  119 */ asctime,
	/*  120 */ asctime_r,
	/*  121 */ asin,
	/*  122 */ asinh,
	/*  123 */ atan2,
	/*  124 */ atan,
	/*  125 */ atanh,
	/*  126 */ atexit,
	/*  127 */ atof,
	/*  128 */ atoi,
	/*  129 */ atol,
	/*  130 */ bcmp,
	/*  131 */ bcopy,
	/*  132 */ bcopyFast,
	/*  133 */ bcopyBytes,
	/*  134 */ bcopyLongs,
	/*  135 */ bcopyWords,
	/*  136 */ bfill,
	/*  137 */ bfillBytes,
	/*  138 */ binvert,
	/*  139 */ bsearch,
	/*  140 */ bswap,
	/*  141 */ bzero,
	/*  142 */ bzeroFast,
	/*  143 */ cabs,
	/*  144 */ cbrt,
	/*  145 */ calloc,
	/*  146 */ ceil,
	/*  147 */ cfree,
	/*  148 */ chdir,
	/*  149 */ checksum,
	/*  150 */ classClassIdPtrGet,
	/*  151 */ classInit,
	/*  152 */ classInstrument,
	/*  153 */ classMemPartIdSet,
	/*  154 */ classShowConnect,
	/*  155 */ close,
	/*  156 */ clearerr,
	/*  157 */ clock,
	/*  158 */ clock_getres,
	/*  159 */ clock_gettime,
	/*  160 */ clock_setres,
	/*  161 */ clock_settime,
	/*  162 */ clock_show,
	/*  163 */ clockLibInit,
	/*  164 */ closedir,
	/*  165 */ consoleFdPtrGet,
	/*  166 */ cos,
	/*  167 */ cosh,
	/*  168 */ creat,
	/*  169 */ ctime,
	/*  170 */ ctime_r,
	/*  171 */ difftime,
	/*  172 */ div,
	/*  173 */ div_r,
	/*  174 */ dllAdd,
	/*  175 */ dllCount,
	/*  176 */ dllCreate,
	/*  177 */ dllDelete,
	/*  178 */ dllEach,
	/*  179 */ dllGet,
	/*  180 */ dllInit,
	/*  181 */ dllInsert,
	/*  182 */ dllRemove,
	/*  183 */ dllTerminate,
	/*  184 */ envPrivateCreate,
	/*  185 */ envPrivateDestroy,
	/*  186 */ envShow,
	/*  187 */ errnoGet,
	/*  188 */ errnoOfTaskGet,
	/*  189 */ errnoOfTaskSet,
	/*  190 */ errnoSet,
	/*  191 */ evtActionPtrGet,
	/*  192 */ excHookAdd,
	/*  193 */ excJobAdd,
	/*  194 */ exit,
	/*  195 */ exp,
	/*  196 */ fabs,
	/*  197 */ fclose,
	/*  198 */ fdopen,
	/*  199 */ fdprintf,
	/*  200 */ fdTablePtrGet,
	/*  201 */ feof,
	/*  202 */ ferror,
	/*  203 */ fflush,
	/*  204 */ ffsMsb,
	/*  205 */ fgetc,
	/*  206 */ fgetpos,
	/*  207 */ fgets,
	/*  208 */ fileno,
	/*  209 */ fioFormatV,
	/*  210 */ fioRdString,
	/*  211 */ fioRead,
	/*  212 */ floor,
	/*  213 */ fmod,
	/*  214 */ fopen,
	/*  215 */ fprintf,
	/*  216 */ fputc,
	/*  217 */ fputs,
	/*  218 */ fread,
	/*  219 */ freopen,
	/*  220 */ fpClassIdPtrGet,
	/*  221 */ fppProbe,
	/*  222 */ fppRestore,
	/*  223 */ fppSave,
	/*  224 */ fppTaskRegsGet,
	/*  225 */ fppTaskRegsSet,
	/*  226 */ fpTypeGet,
	/*  227 */ free,
	/*  228 */ frexp,
	/*  229 */ fscanf,
	/*  230 */ fseek,
	/*  231 */ fsetpos,
	/*  232 */ fstat,
	/*  233 */ fstatfs,
	/*  234 */ ftell,
	/*  235 */ fwrite,
	/*  236 */ getc,
	/*  237 */ getchar,
	/*  238 */ getcwd,
	/*  239 */ getenv,
	/*  240 */ gets,
	/*  241 */ getw,
	/*  242 */ getwd,
	/*  243 */ gmtime,
	/*  244 */ gmtime_r,
	/*  245 */ hashFuncIterScale,
	/*  246 */ hashFuncModulo,
	/*  247 */ hashFuncMultiply,
	/*  248 */ hashKeyCmp,
	/*  249 */ hashKeyStrCmp,
	/*  250 */ hashTblCreate,
	/*  251 */ hashTblDelete,
	/*  252 */ hashTblDestroy,
	/*  253 */ hashTblEach,
	/*  254 */ hashTblFind,
	/*  255 */ hashTblInit,
	/*  256 */ hashTblPut,
	/*  257 */ hashTblRemove,
	/*  258 */ hashTblTerminate,
	/*  259 */ hypot,
	/*  260 */ index,
	/*  261 */ intContext,
	/*  262 */ intCount,
	/*  263 */ intCntPtrGet,
	/*  264 */ intLock,
	/*  265 */ intLockLevelSet,
	/*  266 */ intUnlock,
	/*  267 */ ioDefPathGet,
	/*  268 */ ioDefPathSet,
	/*  269 */ ioGlobalStdGet,
	/*  270 */ ioGlobalStdSet,
	/*  271 */ ioTaskStdGet,
	/*  272 */ ioTaskStdSet,
	/*  273 */ ioctl,
	/*  274 */ iosDevAdd,
	/*  275 */ iosDevDelete,
	/*  276 */ iosDevFind,
	/*  277 */ iosDrvInstall,
	/*  278 */ iosDrvRemove,
	/*  279 */ iosFdFreeHookRtnPtrGet,
	/*  280 */ iosFdNewHookRtnPtrGet,
	/*  281 */ iosFdValue,
	/*  282 */ iosLibInitializedPtrGet,
	/*  283 */ iosRelinquish,
	/*  284 */ isalnum,
	/*  285 */ isalpha,
	/*  286 */ isatty,
	/*  287 */ iscntrl,
	/*  288 */ isdigit,
	/*  289 */ isgraph,
	/*  290 */ islower,
	/*  291 */ isprint,
	/*  292 */ ispunct,
	/*  293 */ isspace,
	/*  294 */ isupper,
	/*  295 */ isxdigit,
	/*  296 */ kill,
	/*  297 */ kernelStatePtrGet,
	/*  298 */ kernelTimeSlice,
	/*  299 */ kernelVersion,
	/*  300 */ labs,
	/*  301 */ ldiv,
	/*  302 */ ldiv_r,
	/*  303 */ ldexp,
	/*  304 */ localeconv,
	/*  305 */ localtime,
	/*  306 */ localtime_r,
	/*  307 */ log,
	/*  308 */ log10,
	/*  309 */ logFdAdd,
	/*  310 */ logFdDelete,
	/*  311 */ logFdSet,
	/*  312 */ logMsg,
	/*  313 */ longjmp,
	/*  314 */ lseek,
	/*  315 */ lstAdd,
	/*  316 */ lstConcat,
	/*  317 */ lstCount,
	/*  318 */ lstDelete,
	/*  319 */ lstExtract,
	/*  320 */ lstFind,
	/*  321 */ lstFirst,
	/*  322 */ lstFree,
	/*  323 */ lstGet,
	/*  324 */ lstInit,
	/*  325 */ lstInsert,
	/*  326 */ lstLast,
	/*  327 */ lstNStep,
	/*  328 */ lstNext,
	/*  329 */ lstNth,
	/*  330 */ lstPrevious,
	/*  331 */ malloc,
	/*  332 */ maxDriversPtrGet,
	/*  333 */ maxFilesPtrGet,
	/*  334 */ mblen,
	/*  335 */ mbstowcs,
	/*  336 */ mbtowc,
	/*  337 */ memalign,
	/*  338 */ memAddToPool,
	/*  339 */ memchr,
	/*  340 */ memcmp,
	/*  341 */ memcpy,
	/*  342 */ memDevCreate,
	/*  343 */ memDevCreateDir,
	/*  344 */ memDevDelete,
	/*  345 */ memDrv,
	/*  346 */ memFindMax,
	/*  347 */ memmove,
	/*  348 */ memOptionsSet,
	/*  349 */ memPartAddToPool,
	/*  350 */ memPartAlignedAlloc,
	/*  351 */ memPartAlloc,
	/*  352 */ memPartAllocDisable,
	/*  353 */ memPartCreate,
	/*  354 */ memPartFree,
	/*  355 */ memPartFindMax,
	/*  356 */ memPartInit,
	/*  357 */ memPartOptionsSet,
	/*  358 */ memPartRealloc,
	/*  359 */ memset,
	/*  360 */ memSysPartIdPtrGet,
	/*  361 */ mktime,
	/*  362 */ msgQClassIdPtrGet,
	/*  363 */ msgQCreate,
	/*  364 */ msgQDelete,
	/*  365 */ msgQInfoGet,
	/*  366 */ msgQInit,
	/*  367 */ msgQNumMsgs,
	/*  368 */ msgQReceive,
	/*  369 */ msgQSend,
	/*  370 */ modf,
	/*  371 */ objAlloc,
	/*  372 */ objAllocExtra,
	/*  373 */ objCoreInit,
	/*  374 */ objCoreTerminate,
	/*  375 */ objFree,
	/*  376 */ objShow,
	/*  377 */ open,
	/*  378 */ opendir,
	/*  379 */ partitionNormalModeSet,
	/*  380 */ partitionSwitchInHookAdd,
	/*  381 */ pathBuild,
	/*  382 */ pathCat,
	/*  383 */ pathCondense,
	/*  384 */ pathLastName,
	/*  385 */ pathLastNamePtr,
	/*  386 */ pathParse,
	/*  387 */ pathSplit,
	/*  388 */ pause,
	/*  389 */ perror,
	/*  390 */ pipeDevCreate,
	/*  391 */ pipeDevDelete,
	/*  392 */ pipeDrv,
	/*  393 */ pow,
	/*  394 */ printErr,
	/*  395 */ printf,
	/*  396 */ procCreateHookAdd,
	/*  397 */ procCreateHookDelete,
	/*  398 */ procStartHookAdd,
	/*  399 */ procStartHookDelete,
	/*  400 */ pTaskLastDspTcbPtrGet,
	/*  401 */ pTaskLastFpTcbPtrGet,
	/*  402 */ putc,
	/*  403 */ putchar,
	/*  404 */ putenv,
	/*  405 */ puts,
	/*  406 */ putw,
	/*  407 */ qFifoClassIdPtrGet,
	/*  408 */ qFirst,
	/*  409 */ qInit,
	/*  410 */ qPriListClassIdPtrGet,
	/*  411 */ qsort,
	/*  412 */ ramDevCreate,
	/*  413 */ ramDrv,
	/*  414 */ rand,
	/*  415 */ raise,
	/*  416 */ read,
	/*  417 */ readdir,
	/*  418 */ readyQHeadPtrGet,
	/*  419 */ realloc,
	/*  420 */ reboot,
	/*  421 */ rebootHookAdd,
	/*  422 */ remove,
	/*  423 */ rename,
	/*  424 */ rewind,
	/*  425 */ rewinddir,
	/*  426 */ rindex,
	/*  427 */ rint,
	/*  428 */ rngBufGet,
	/*  429 */ rngBufPut,
	/*  430 */ rngCreate,
	/*  431 */ rngDelete,
	/*  432 */ rngFlush,
	/*  433 */ rngFreeBytes,
	/*  434 */ rngIsEmpty,
	/*  435 */ rngIsFull,
	/*  436 */ rngMoveAhead,
	/*  437 */ rngNBytes,
	/*  438 */ rngPutAhead,
	/*  439 */ roundRobinOnPtrGet,
	/*  440 */ roundRobinSlicePtrGet,
	/*  441 */ scanf,
	/*  442 */ sdRgnAddrGet,
	/*  443 */ sdRgnInfoGet,
	/*  444 */ sdRgnAllGet,
	/*  445 */ sdRgnFirstGet,
	/*  446 */ sdRgnNextGet,
	/*  447 */ select,
	/*  448 */ selNodeAdd,
	/*  449 */ selNodeDelete,
	/*  450 */ selWakeup,
	/*  451 */ selWakeupAll,
	/*  452 */ selWakeupListInit,
	/*  453 */ selWakeupListLen,
	/*  454 */ selWakeupType,
	/*  455 */ semBCreate,
	/*  456 */ semBInit,
	/*  457 */ semCCreate,
	/*  458 */ semCInit,
	/*  459 */ semClassIdPtrGet,
	/*  460 */ semClear,
	/*  461 */ semCLibInit,
	/*  462 */ semCreate,
	/*  463 */ semDelete,
	/*  464 */ semEvStart,
	/*  465 */ semEvStop,
	/*  466 */ semFlush,
	/*  467 */ semGive,
	/*  468 */ semInfo,
	/*  469 */ semMCreate,
	/*  470 */ semMGiveForce,
	/*  471 */ semMInit,
	/*  472 */ semTake,
	/*  473 */ setbuf,
	/*  474 */ setbuffer,
	/*  475 */ setjmp,
	/*  476 */ setlinebuf,
	/*  477 */ setlocale,
	/*  478 */ setvbuf,
	/*  479 */ sigaction,
	/*  480 */ sigaddset,
	/*  481 */ sigblock,
	/*  482 */ sigdelset,
	/*  483 */ sigemptyset,
	/*  484 */ sigfillset,
	/*  485 */ sigismember,
	/*  486 */ signal,
	/*  487 */ sigPendDestroy,
	/*  488 */ sigpending,
	/*  489 */ sigPendInit,
	/*  490 */ sigPendKill,
	/*  491 */ sigprocmask,
	/*  492 */ sigqueue,
	/*  493 */ sigqueueInit,
	/*  494 */ sigsetmask,
	/*  495 */ sigsetjmp,
	/*  496 */ sigsuspend,
	/*  497 */ sigtimedwait,
	/*  498 */ sigvec,
	/*  499 */ sigwait,
	/*  500 */ sigwaitinfo,
	/*  501 */ sin,
	/*  502 */ sinh,
	/*  503 */ sllCount,
	/*  504 */ sllCreate,
	/*  505 */ sllDelete,
	/*  506 */ sllEach,
	/*  507 */ sllGet,
	/*  508 */ sllInit,
	/*  509 */ sllPrevious,
	/*  510 */ sllPutAtHead,
	/*  511 */ sllPutAtTail,
	/*  512 */ sllRemove,
	/*  513 */ sllTerminate,
	/*  514 */ sprintf,
	/*  515 */ sqrt,
	/*  516 */ srand,
	/*  517 */ sscanf,
	/*  518 */ stat,
	/*  519 */ statfs,
	/*  520 */ statSymTblPtrGet,
	/*  521 */ stdioFp,
	/*  522 */ strcat,
	/*  523 */ strchr,
	/*  524 */ strcmp,
	/*  525 */ strcoll,
	/*  526 */ strcpy,
	/*  527 */ strcspn,
	/*  528 */ strerror,
	/*  529 */ strerror_r,
	/*  530 */ strftime,
	/*  531 */ strlen,
	/*  532 */ strncat,
	/*  533 */ strncmp,
	/*  534 */ strncpy,
	/*  535 */ strpbrk,
	/*  536 */ strrchr,
	/*  537 */ strspn,
	/*  538 */ strstr,
	/*  539 */ strtod,
	/*  540 */ strtok,
	/*  541 */ strtok_r,
	/*  542 */ strtol,
	/*  543 */ strtoul,
	/*  544 */ strxfrm,
	/*  545 */ symAdd,
	/*  546 */ symFindByName,
	/*  547 */ symFindByNameAndType,
	/*  548 */ symFindByValue,
	/*  549 */ symFindByValueAndType,
	/*  550 */ symLibInit,
	/*  551 */ symRemove,
	/*  552 */ symTblAdd,
	/*  553 */ symTblClassIdPtrGet,
	/*  554 */ symTblCreate,
	/*  555 */ symTblDelete,
	/*  556 */ symTblLock,
	/*  557 */ symTblRemove,
	/*  558 */ symTblUnlock,
	/*  559 */ sysClkRateGet,
	/*  560 */ sysSymTblPtrGet,
	/*  561 */ system,
	/*  562 */ sysTimestamp64DiffToNsec,
	/*  563 */ sysTimestamp64Get,
	/*  564 */ sysToMonitor,
	/*  565 */ swab,
	/*  566 */ tan,
	/*  567 */ tanh,
	/*  568 */ taskActivate,
	/*  569 */ taskArgsGet,
	/*  570 */ taskClassIdPtrGet,
	/*  571 */ taskCreat,
	/*  572 */ taskCreateHookAdd,
	/*  573 */ taskCreateHookDelete,
	/*  574 */ taskDelay,
	/*  575 */ taskDelete,
	/*  576 */ taskDeleteForce,
	/*  577 */ taskDeleteHookAdd,
	/*  578 */ taskDeleteHookDelete,
	/*  579 */ taskExitHookPtrGet,
	/*  580 */ taskIdCurrentPtrGet,
	/*  581 */ taskIdDefault,
	/*  582 */ taskIdListGet,
	/*  583 */ taskIsReady,
	/*  584 */ taskIdSelf,
	/*  585 */ taskIsSuspended,
	/*  586 */ taskIdVerify,
	/*  587 */ taskInfoGet,
	/*  588 */ taskInit,
	/*  589 */ taskLock,
	/*  590 */ taskName,
	/*  591 */ taskNameToId,
	/*  592 */ taskOptionsGet,
	/*  593 */ taskOptionsSet,
	/*  594 */ taskPcGet,
	/*  595 */ taskPriorityGet,
	/*  596 */ taskPrioritySet,
	/*  597 */ taskRegsGet,
	/*  598 */ taskRegsSet,
	/*  599 */ taskReset,
	/*  600 */ taskRestart,
	/*  601 */ taskResume,
	/*  602 */ taskSafe,
	/*  603 */ taskSafeWait,
	/*  604 */ taskSpawn,
	/*  605 */ taskSuspend,
	/*  606 */ taskSwapHookAdd,
	/*  607 */ taskSwapHookDelete,
	/*  608 */ taskSwitchHookAdd,
	/*  609 */ taskSwitchHookDelete,
	/*  610 */ taskTcb,
	/*  611 */ taskUndelay,
	/*  612 */ taskUnlock,
	/*  613 */ taskUnsafe,
	/*  614 */ taskVarAdd,
	/*  615 */ taskVarDelete,
	/*  616 */ taskVarGet,
	/*  617 */ taskVarInfo,
	/*  618 */ taskVarInit,
	/*  619 */ taskVarSet,
	/*  620 */ tick64Get,
	/*  621 */ tickGet,
	/*  622 */ tickQHeadPtrGet,
	/*  623 */ time,
	/*  624 */ timeMonitorClear,
	/*  625 */ timeMonitorStart,
	/*  626 */ timeMonitorStop,
	/*  627 */ timeMonitorTaskGet,
	/*  628 */ timeMonitorSysGet,
	/*  629 */ tmpfile,
	/*  630 */ tmpnam,
	/*  631 */ tolower,
	/*  632 */ toupper,
	/*  633 */ trgEvtClassPtrGet,
	/*  634 */ ungetc,
	/*  635 */ unlink,
	/*  636 */ uswab,
	/*  637 */ utime,
	/*  638 */ valloc,
	/*  639 */ vfdprintf,
	/*  640 */ vfprintf,
	/*  641 */ vprintf,
	/*  642 */ vsprintf,
	/*  643 */ vThreadsDbgPrint,
	/*  644 */ vThreadsEventHandlerRegister,
	/*  645 */ vThreadsOsInvoke,
	/*  646 */ vThreadsOsInvokeCustom,
	/*  647 */ vThreadsSysInfoPtrGet,
	/*  648 */ vxCurrentPcGet,
	/*  649 */ vxMemProbe,
	/*  650 */ vxTas,
	/*  651 */ vxTicksPtrGet,
	/*  652 */ vxAbsTicksPtrGet,
	/*  653 */ vxJobAddTicksPtrGet,
	/*  654 */ wcstombs,
	/*  655 */ wctomb,
	/*  656 */ wdCancel,
	/*  657 */ wdCreate,
	/*  658 */ wdDelete,
	/*  659 */ wdStart,
	/*  660 */ windDelete,
	/*  661 */ windExit,
	/*  662 */ windPendQFlush,
	/*  663 */ windPendQGet,
	/*  664 */ windPendQPut,
	/*  665 */ windPendQRemove,
	/*  666 */ windPendQTerminate,
	/*  667 */ windReadyQPut,
	/*  668 */ windSuspend,
	/*  669 */ windTickRecalibrate,
	/*  670 */ workQAdd1,
	/*  671 */ workQAdd2,
	/*  672 */ write,
	/*  673 */ wvEvtClassPtrGet,
	/*  674 */ configRecordFieldGet,
	/*  675 */ hmEventInject,
	/*  676 */ hmEventLog,
	/*  677 */ hmIsAutoLog,
	/*  678 */ hmLogEntriesGet,
	/*  679 */ hmErrorHandlerCreate,
	/*  680 */ hmErrorHandlerEventGet,
	/*  681 */ hmErrorHandlerTaskIdGet,
	/*  682 */ hmErrorHandlerSuspend,
	/*  683 */ hmErrorHandlerResume,
	/*  684 */ hmErrorHandlerDelete,
	/*  685 */ hmErrorHandlerParamSet,
	/*  686 */ ppGlobalEnvironPtrGet,

};

static SL_VERSION slVersion0 =
	{
	8, "vThreads",
	3, "app"
	};

/*
 * entry point table initialization
 */

static void epInit (void) {
	vxRegisterOrLink(SLVERSION_REGISTER, &slVersion0, (void*) slTable0);
}

static const void * init __attribute__((section(".ctors.stubs"),used)) = epInit;

