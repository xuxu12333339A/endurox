##
## @brief Enduro Execution platform's ATMI library/ies tests
##   Using Cgreen framework for tests.
##
## @file CMakeLists.txt
##
## -----------------------------------------------------------------------------
## Enduro/X Middleware Platform for Distributed Transaction Processing
## Copyright (C) 2009-2016, ATR Baltic, Ltd. All Rights Reserved.
## Copyright (C) 2017-2023, Mavimax, Ltd. All Rights Reserved.
## This software is released under one of the following licenses:
## AGPL (with Java and Go exceptions) or Mavimax's license for commercial use.
## See LICENSE file for full text.
## -----------------------------------------------------------------------------
## AGPL license:
##
## This program is free software; you can redistribute it and/or modify it under
## the terms of the GNU Affero General Public License, version 3 as published
## by the Free Software Foundation;
##
## This program is distributed in the hope that it will be useful, but WITHOUT ANY
## WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
## PARTICULAR PURPOSE. See the GNU Affero General Public License, version 3
## for more details.
##
## You should have received a copy of the GNU Affero General Public License along 
## with this program; if not, write to the Free Software Foundation, Inc.,
## 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
##
## -----------------------------------------------------------------------------
## A commercial use license is available from Mavimax, Ltd
## contact@mavimax.com
## -----------------------------------------------------------------------------
##
cmake_minimum_required(VERSION 3.5)

# Make sure the compiler can find include files from UBF library
include_directories (${ENDUROX_SOURCE_DIR}/libubf
					 ${ENDUROX_SOURCE_DIR}/include
					 ${ENDUROX_SOURCE_DIR}/libnstd
					 ${ENDUROX_SOURCE_DIR}/ubftest)

# Make sure the linker can find the UBF library once it is built.
link_directories (${ENDUROX_BINARY_DIR}/libubf) 

# Sub Tests
add_subdirectory (test000_system)
add_subdirectory (test001_basiccall)
add_subdirectory (test002_basicforward)
add_subdirectory (test003_basicconvers)
add_subdirectory (test004_basicevent)
add_subdirectory (test005_convconfload)
add_subdirectory (test006_ulog)
add_subdirectory (test007_advertise)
add_subdirectory (test008_extensions)
add_subdirectory (test009_srvdie)
add_subdirectory (test010_strtracecond)
add_subdirectory (test011_tout)
add_subdirectory (test012_admqfull)
add_subdirectory (test013_procnorsp)
add_subdirectory (test015_threads)
add_subdirectory (test016_contextsw)
add_subdirectory (test017_srvthread)
add_subdirectory (test018_tpacalltout)
add_subdirectory (test021_xafull)
add_subdirectory (test022_typedstring)
add_subdirectory (test023_typedcarray)
add_subdirectory (test024_json)
add_subdirectory (test026_typedjson)

IF (NOT DEFINE_DISABLEPSCRIPT)
	add_subdirectory (test027_pscript)
ENDIF()

add_subdirectory (test028_tmq)
add_subdirectory (test029_inicfg)
add_subdirectory (test030_cconfsrv)
add_subdirectory (test031_logging)
add_subdirectory (test032_oapi)
add_subdirectory (test034_shmreuse)
add_subdirectory (test035_hkeep)
add_subdirectory (test036_tprecover)
add_subdirectory (test037_down)
add_subdirectory (test038_tpnotify)
add_subdirectory (test039_tpbroadcast)
add_subdirectory (test040_typedview)
add_subdirectory (test041_bigmsg)
add_subdirectory (test042_bignet)
add_subdirectory (test043_encrypt)
add_subdirectory (test044_ping)
add_subdirectory (test045_tpcallnoblock)
add_subdirectory (test046_twopasscfg)

IF (ENABLE_TEST47)
add_subdirectory (test047_oradb)
ENDIF()

add_subdirectory (test048_cache)
add_subdirectory (test049_masksvc)
add_subdirectory (test050_ubfdb)
add_subdirectory (test051_settout)
add_subdirectory (test052_minstart)
add_subdirectory (test053_logoff)
add_subdirectory (test054_svwrap)
add_subdirectory (test055_envs)
add_subdirectory (test056_tpimpexp)
add_subdirectory (test057_invlcmd)

IF (DEFINED EX_USE_SYSVQ)
	add_subdirectory (test058_systemv)
ENDIF()

add_subdirectory (test059_pq)
add_subdirectory (test060_ndxdfork)
add_subdirectory (test062_memlimits)
add_subdirectory (test063_cpmrange)
add_subdirectory (test064_bufswitch)
add_subdirectory (test065_tpcancel)
add_subdirectory (test066_tmstartserver)

IF (ENABLE_POSTGRES)
add_subdirectory (test067_postgres)
ENDIF()

add_subdirectory (test068_tpadm)
add_subdirectory (test069_wnormal)
add_subdirectory (test070_ubfhook)
add_subdirectory (test072_qos)
add_subdirectory (test074_sanitulog)
add_subdirectory (test075_dispthread)
add_subdirectory (test076_dispthreadxa)
add_subdirectory (test077_diedslowstrt)
add_subdirectory (test078_tpacallmix)
add_subdirectory (test079_cliquote)
add_subdirectory (test080_tpexit)
add_subdirectory (test081_lcf)
add_subdirectory (test082_autotran)
add_subdirectory (test084_ddr)
add_subdirectory (test085_prio)
add_subdirectory (test086_tmqlimit)
add_subdirectory (test087_tmsrv)
add_subdirectory (test088_addlog)
add_subdirectory (test090_tuxmig)

IF (DEFINED EX_USE_EPOLL)
add_subdirectory (test091_tpgotsig)
ENDIF()

add_subdirectory (test092_bflag)
add_subdirectory (test093_killseq)
add_subdirectory (test094_sigchld)

IF (DEFINED EX_USE_SYSVQ)
	add_subdirectory (test095_rqaddrel)
ENDIF()

add_subdirectory (test096_svcrmnet)
add_subdirectory (test097_mtnojump)
add_subdirectory (test098_svqhkeep)
add_subdirectory (test099_callbuflen)
add_subdirectory (test100_svstdout)
add_subdirectory (test101_dupsrv)
add_subdirectory (test102_procgrp)
add_subdirectory (test103_singlegrp)
add_subdirectory (test104_tmqfailover)
add_subdirectory (test105_respawn)
add_subdirectory (test106_periodcb)

################################################################################
# Master test case drivere
add_executable (atmiunit1 atmiunit1.c)
################################################################################
# Master test case driver
target_link_libraries (atmiunit1 ubf cgreen m nstd ${RT_LIB} pthread)
set_target_properties(atmiunit1 PROPERTIES LINK_FLAGS "$ENV{MYLDFLAGS}")

# vim: set ts=4 sw=4 et smartindent:
