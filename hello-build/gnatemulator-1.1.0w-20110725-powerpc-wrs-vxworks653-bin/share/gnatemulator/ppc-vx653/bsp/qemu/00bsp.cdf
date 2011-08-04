/* 00bsp.cdf - bsp cdf file for wrSbc750gx BSP */

/* Copyright 1984-2005 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/* 

modification history
--------------------
01f,29jun05,f_l merged Minimal Udp support from psc2.0
01e,24may05,dsb increased BSP_REV to "/5", for fix to TRACE B0010
01d,28apr05,foo increased BSP_REV to "/4"
01c,14sep04,mat increased BSP_REV to "/3"
01b,24sep04,mat increased default of LOCAL_MEM_SIZE to 0x10000000 (256MB)
01a,02jul04,kp  created by teamF1.
*/


Folder wrSbc750gx_FOLDER {
       NAME		wrSbc750gx folder
       SYNOPSIS		ExposewrSbc750gx comps and params
       _CHILDREN	FOLDER_BSP_CONFIG
       CHILDREN		INCLUDE_BSP_MODULES \
			INCLUDE_MMU_BASIC \
			INCLUDE_SERIAL \
			INCLUDE_SYSCLK \
        DEFAULTS	INCLUDE_BSP_MODULES \
			INCLUDE_MMU_BASIC \
			INCLUDE_NE2000END \
			INCLUDE_NETWORK \
			INCLUDE_SERIAL \
			INCLUDE_SYSCLK
}




Component INCLUDE_INSTRUMENTATION {
        NAME		wrSbc750gx configuration components
	SYNOPSIS	 wind view components
}

Component INCLUDE_MMU_BASIC {
        NAME		wrSbc750gx configuration components
	SYNOPSIS	 basic MMU support
}

Component INCLUDE_SERIAL {
        NAME		wrSbc750gx configuration components
	SYNOPSIS	serial driver
}

Component INCLUDE_SYSCLK {
        NAME		wrSbc750gx configuration components
	SYNOPSIS	 PPC decrementer sstem clock
}


Component INCLUDE_BSP_MODULES {
	NAME		 wrSbc750gx configuration components
        CFG_PARAMS	 BSP_REV \
			 BSP_VERSION \
			 BSP_VER_1_1 \
			 BSP_VER_1_2 \
			 BSP_VER_1_3 \
			 DEFAULT_BOOT_LINE \
			 LOCAL_MEM_AUTOSIZE \
			 LOCAL_MEM_END_ADRS \
			 LOCAL_MEM_LOCAL_ADRS \
			 LOCAL_MEM_SIZE \
			 RAM_HIGH_ADRS \
			 RAM_LOW_ADRS \
			 RAM_PAYLOAD_ADRS \
			 RAM_PAYLOAD_SIZE \
			 ROM_BASE_ADRS \
			 ROM_SIZE \
			 ROM_TEXT_ADRS \
			 ROM_WARM_ADRS \
			 USER_D_CACHE_ENABLE \
			 USER_D_CACHE_MODE \
			 USER_I_CACHE_ENABLE \
			 USER_D_CACHE_MODE 
}

Component INCLUDE_END {
	INCLUDE_WHEN	+= INCLUDE_NETWORK
}

Parameter BSP_REV {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	0 for first revision
	TYPE		string
	DEFAULT		"/5"
}

Parameter BSP_VERSION {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	major BSP version
	TYPE		string
	DEFAULT		"1.4"
}

Parameter BSP_VER_1_1 {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	BSP version/revision identification
	TYPE		uint
	DEFAULT		1
}

Parameter BSP_VER_1_2 {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	BSP version/revision identification
	TYPE		uint
	DEFAULT		1
}


Parameter BSP_VER_1_3 {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	BSP version/revision identification
	TYPE		uint
	DEFAULT		1
}

Parameter DEFAULT_BOOT_LINE {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	deafult boot parameter for boot applications
	TYPE		string
	DEFAULT		"ene(0,0)host:http://10.0.2.2:8080/boot.txt h=10.0.2.2 e=10.0.2.1"
}

Parameter LOCAL_MEM_END_ADRS {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	memory configuartion
	TYPE		uint
	DEFAULT		(LOCAL_MEM_LOCAL_ADRS + LOCAL_MEM_SIZE)
}

Parameter LOCAL_MEM_LOCAL_ADRS {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	memory configuartion
	TYPE		uint
	DEFAULT		0x0
}

Parameter LOCAL_MEM_SIZE {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	memory size (256 MB)
	TYPE		uint
	DEFAULT		0x10000000
}

Parameter LOCAL_MEM_AUTOSIZE {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	runtime memory sizing
	TYPE		exists
	DEFAULT		FALSE
}

Parameter CORE_OS_MEM_POOL_RGN_SIZE  {
	DEFAULT          0x01800000
}

Parameter NUM_TTY {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	number of tty channels
	TYPE		uint
	DEFAULT		N_SIO_CHANNELS
}

Parameter RAM_HIGH_ADRS {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	RAM address of bootrom
	TYPE		uint
	DEFAULT		0x00600000
}

Parameter RAM_LOW_ADRS {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	RAM address of bootrom
	TYPE		uint
	DEFAULT		0x00100000
}

Parameter RAM_PAYLOAD_ADRS  {
    NAME              wrSbc750gx configuration Parameter
    SYNOPSIS          RAM address where RAM payload image is installed/loaded
    TYPE              uint
    DEFAULT           (LOCAL_MEM_END_ADRS - RAM_PAYLOAD_SIZE)
}

Parameter RAM_PAYLOAD_SIZE  {
    NAME              wrSbc750gx configuration Parameter
    SYNOPSIS          RAM size reserved for RAM payload image
    TYPE              uint
    DEFAULT           (INCLUDE_PAYLOAD)::(0x00200000) 0
}

Parameter ROM_BASE_ADRS {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	ROM base address
	TYPE		uint
	DEFAULT		0xFFF00000
}

Parameter ROM_SIZE {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	ROM size
	TYPE		uint
	DEFAULT		0x00100000
}

Parameter ROM_TEXT_ADRS {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	with PC and SP
	TYPE		uint
	DEFAULT		0xFFF00100
}

Parameter ROM_WARM_ADRS {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	warm reboot entry
	TYPE		uint
	DEFAULT		(ROM_TEXT_ADRS + 0x0008)
}

Parameter USER_D_CACHE_ENABLE {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	Enable Data Cache
	TYPE		exists
	DEFAULT		TRUE
}

Parameter USER_D_CACHE_MODE {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	selewect copyback or disabled
	TYPE		uint
	DEFAULT		CACHE_COPYBACK
}


Parameter USER_I_CACHE_ENABLE {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	Enable Instruction Cache
	TYPE		exists
	DEFAULT		TRUE
}

Parameter USER_I_CACHE_MODE {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	seleect copyback or disabled
	TYPE		uint
	DEFAULT		CACHE_COPYBACK
}

Parameter USER_RESERVED_MEM  {
    DEFAULT           (0)
}

Parameter WDB_COMM_TYPE {
	NAME		wrSbc750gx configuration parameter
	SYNOPSIS	select copyback or disabled
	TYPE		uint
	DEFAULT		WDB_COMM_END
}

Parameter WDB_END_DEVICE_NAME {
        NAME            Device to use for WDB connection
        SYNOPSIS        Alternative device to use for WDB connection. If none specified, use boot device.
        DEFAULT         "ene"
        TYPE            string
}
