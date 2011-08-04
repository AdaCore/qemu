# Makefile - Make file for Wind River SBC750GX Board
#
# Copyright 2003 - 2006 Wind River Systems, Inc.
#
# modification history
# --------------------
# 01i,21apr06,mnh  fix macro typo
# 01h,15feb06,f_l  merged from PSC 1.6.3 branch
# 01g,17may05,agi  added BSP
# 01f,24sep04,mat increased LOCAL_MEM_SIZE to 10000000 (256MB)
# 01e,29jul04,kam added sysTffs to the doc build
# 01d,10jul04,kp  modified for SBC750GX
# 01c,18mar04,kp  teamF1, added EXTRA_DEFINE = -DTORNADO_AE_653
# 01b,16feb04,kp  increased LOCAL_MEM_SIZE to 08000000 (128MB)
# 01a,19nov03,kp  TeamF1, created from SP7457 BSP Makefile.
#
# DESCRIPTION
# -----------
# This file contains rules for building VxWorks for the
# Wind River wrSbc750gx board 
#


CPU             = PPC604
TOOL            = gnu
BSP		= wrSbc750gx



TGT_DIR = $(WIND_BASE)/target

include $(TGT_DIR)/h/make/defs.bsp
include $(TGT_DIR)/h/make/make.$(CPU)$(TOOL)
include $(TGT_DIR)/h/make/defs.$(WIND_HOST_TYPE)

## Only redefine make definitions below this point, or your definitions will
## be overwritten by the makefile stubs above. All the board specific
## defines are listed below, but it is preferable to use the BSP that
## matches the hardware.  Runtime checks for some board differences are not
## possible, so the kernel should be configured to the specific board.

BSP		= wrSbc750gx
TARGET_DIR	= wrSbc750gx
VENDOR		= WindRiver
EXTRA_DEFINE    = -DTORNADO_AE_653

EXTRA_DEFINE    += -DSBC750GX
BOARD		=   wrSbc750gx

USR_ENTRY       = usrInit

## The constants ROM_TEXT_ADRS, ROM_SIZE, and RAM_HIGH_ADRS are defined
## in config.h, MakeSkel, Makefile, and Makefile.*
## All definitions for these constants must be identical. 

ROM_TEXT_ADRS   = FFF00100  # ROM entry address, PPC boot vector
ROM_SIZE        = 00100000  # number of bytes of ROM space 1MB
RAM_LOW_ADRS    = 00100000  # RAM text/data address for vxWorks
RAM_HIGH_ADRS   = 00600000  # RAM text/data address for bootrom
LOCAL_MEM_SIZE  = 10000000  # 256 MB DDR memory


# Required to pull in selected symbols from the target archive objects
LIB_MODULE_PATH = $(WIND_BASE)/target/lib/obj$(CPU)$(TOOL)vx

EXTRA_LIB_MODULES =  

# BOOT_EXTRA is for any additional bootrom module 

BOOT_EXTRA	= 

# MACH_EXTRA is for any additional runtime modules 

MACH_EXTRA	=

#Use LIB_EXTRA to define extra driver libs to link against.

LIB_EXTRA	=

#BSP doc files for make man.
DOC_FILES	= sysLib

#ADDED_CFLAGS += -g
## Only redefine make definitions above this point, or the expansion of 
## makefile target dependencies may be incorrect.

include $(TGT_DIR)/h/make/rules.bsp
include $(TGT_DIR)/h/make/rules.$(WIND_HOST_TYPE)
