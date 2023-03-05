##
## @brief Enduro/X Build Compiler configuration
##
## @file ex_comp.cmake
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

cmake_minimum_required (VERSION 3.1) 

#
# Configure C/C++ Compiler for the platform
# 
macro(ex_comp_settings)

    SET (EX_ALIGNMENT_BYTES "4")

    SET ( CMAKE_CXX_FLAGS "-O2 -fno-exceptions -fno-rtti ${CMAKE_CXX_FLAGS}" CACHE STRING "compile flags" FORCE)

    # Support #310 Ubuntu 18.04 prints lot of unneeded warnings...
    if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
    #    SET(CMAKE_C_FLAGS "-Wno-format-truncation -Wstringop-overflow=0 -D_DEFAULT_SOURCE=1 ${CMAKE_C_FLAGS}")
        SET(CMAKE_C_FLAGS "-Wno-stringop-overflow -Wno-format-truncation -Wformat-overflow=0 ${CMAKE_C_FLAGS}")
    endif()

    SET (STACK_PROTECTOR "-fstack-protector")

    if(${CMAKE_OS_NAME} STREQUAL "AIX" OR ${CMAKE_OS_NAME} STREQUAL "SUNOS")
        # not for aix, on gcc 9 does not build
        SET (STACK_PROTECTOR "")
    endif()

    # Enable release only if specified.
    # By default if RELEASE_BUILD is not defined, then we run in debug!
    IF (DEFINE_RELEASEBUILD)
        MESSAGE( RELEASE_BUILD )
    #_cmake_modify_IGNORE     SET(CMAKE_BUILD_TYPE release)
        SET(CMAKE_C_FLAGS "${STACK_PROTECTOR} -O2 ${CMAKE_C_FLAGS}")
        SET(CMAKE_CXX_FLAGS "${STACK_PROTECTOR} -O2 ${CMAKE_CXX_FLAGS}")
    ELSE ()
        MESSAGE( DEBUG_BUILD )
    #_cmake_modify_IGNORE     SET(CMAKE_BUILD_TYPE debug)
    # Memory debugging:
    #       SET(CMAKE_C_FLAGS "-fsanitize=alignment -fsanitize=address -fno-omit-frame-pointer -O1 -ggdb ${CMAKE_C_FLAGS}")

        IF (DEFINE_SANITIZE)
            SET(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -O1 -ggdb ${CMAKE_C_FLAGS}")
            SET(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -O1 -ggdb ${CMAKE_CXX_FLAGS}")
        ELSE ()
            SET(CMAKE_C_FLAGS "${STACK_PROTECTOR} -O2 -ggdb ${CMAKE_C_FLAGS}")
        ENDIF()
    ENDIF ()

    IF (MUTEX_DEBUG)
        SET(CMAKE_C_FLAGS "-D_GNU_SOURCE ${CMAKE_C_FLAGS}")
        SET(CMAKE_CXX_FLAGS "-D_GNU_SOURCE ${CMAKE_CXX_FLAGS}")
    ENDIF()

    # -pthread needed for System V pthread_atfork()
    SET(RT_LIB rt dl)
    if( CMAKE_OS_NAME STREQUAL "LINUX" )

        set(EX_OS_LINUX "1")
        set(NDRX_LD_LIBRARY_PATH "LD_LIBRARY_PATH")

        # gnu pg fixes for rpi
        if(EXISTS "/opt/vc/include/bcm_host.h")
            SET(CMAKE_C_FLAGS "-D_FILE_OFFSET_BITS=64 ${CMAKE_C_FLAGS}")
        endif()

        # Configure backend transport
        IF(DEFINE_SYSVQ)
            message("Linux - forced poll()")
            set(EX_USE_SYSVQ "1")
        ELSEIF(DEFINE_SVAPOLL)
            message("Linux - forced dev mode System V poll()")
            set(EX_USE_SVAPOLL "1")
        ELSEIF(DEFINE_FORCEPOLL)
            message("Linux - forced poll()")
            set(EX_USE_POLL "1")
        ELSEIF(DEFINE_FORCEFDPOLL)
            message("Linux - forced fdpoll()")
            set(EX_USE_FDPOLL "1")
        ELSEIF(DEFINE_FORCEEMQ)
            message("Linux - forced emulated message q")
            set(EX_USE_EMQ "1")
            set(EX_USE_POLL "1")
        ELSE()
            message("Linux - forced epoll()")
            set(EX_USE_EPOLL "1")
        ENDIF()
    elseif(CMAKE_OS_NAME STREQUAL "AIX")
            # Configure the compiler, we support XLC too..
            message("No threads mode for CPM - bit slower child process exit status check...")
            # Seems aix 7.1 does not like threads mixed with fork...
            set(EX_CPM_NO_THREADS "1")
            set(NDRX_LD_LIBRARY_PATH "LD_LIBRARY_PATH")

        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
                message("GNU compiler")
                set ( CMAKE_C_FLAGS "-D_SEM_SEMUN_UNDEFINED -D_MSGQSUPPORT -D_THREAD_SAFE -pthread -maix64 -Wl,-brtl,-bexpall,-berok ${CMAKE_C_FLAGS}")
                set ( CMAKE_CXX_FLAGS "-D_SEM_SEMUN_UNDEFINED -D_MSGQSUPPORT -D_THREAD_SAFE -pthread -maix64 -Wl,-brtl,-bexpall,-berok ${CMAKE_CXX_FLAGS}")
        elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
                message("Clang Open XL compiler")
                # seems having issues with tls libndrxxaqdisk.so, access to G_nstd_tls from the lib causes segmentation fault
                set ( CMAKE_C_FLAGS "-D_SEM_SEMUN_UNDEFINED -D_MSGQSUPPORT -ftls-model=global-dynamic -D_THREAD_SAFE -pthread -brtl -bexpall -berok -Wno-unused-command-line-argument ${CMAKE_C_FLAGS}")
                set ( CMAKE_CXX_FLAGS "-D_SEM_SEMUN_UNDEFINED -D_MSGQSUPPORT -ftls-model=global-dynamic -D_THREAD_SAFE -pthread -brtl -bexpall -berok -Wno-unused-command-line-argument ${CMAKE_CXX_FLAGS}")
                set ( CMAKE_SHARED_LINKER_FLAGS "-shared -Wl,-G")
        elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "XL")
                message("XL compiler")
                SET ( CMAKE_CXX_FLAGS "-D_MSGQSUPPORT -O2 -qtls -q64 -b64 -qsuppress=1500-030" CACHE STRING "compile flags" FORCE)
                set ( CMAKE_C_FLAGS " -D_MSGQSUPPORT -D_SEM_SEMUN_UNDEFINED -D_THREAD_SAFE ")
                set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -qwarn64 -qpic -bexpfull -b64")
                set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -brtl -qtls -q64")
                set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -qsuppress=1506-997 -qprocimported=setjmp")
                set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -qsuppress=1506-747")
                set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -qsuppress=1506-742")
                set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -qsuppress=1506-743")
                set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -qsuppress=1506-744")
                set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -qsuppress=1506-745")
                set ( CMAKE_SHARED_LINKER_FLAGS "-G")

                if (NOT DEFINE_RELEASEBUILD)
                        message("XL compiler: with -g (debug symbols...)")
                        set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
                endif()
        endif()

        IF(DEFINE_SVAPOLL)
            message("AIX - Force svapoll")
            set(EX_USE_SVAPOLL "1")
        ELSE()
            set(EX_USE_SYSVQ "1")
        ENDIF()
        set(EX_OS_AIX "1")
    elseif(CMAKE_OS_NAME STREQUAL "HP-UX")
        set(EX_USE_POLL "1")
        set(EX_OS_HPUX "1")
        set(NDRX_LD_LIBRARY_PATH "SHLIB_PATH")
    elseif(CMAKE_OS_NAME STREQUAL "SUNOS")
        set(EX_CPM_NO_THREADS "1")
        set(EX_OS_SUNOS "1")
        set(EX_USE_SYSVQ "1")
        set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -m64")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -m64")

        set(NDRX_LD_LIBRARY_PATH "LD_LIBRARY_PATH")
        # Bug #219 !!! _REENTRANT makes errno thread safe
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_REENTRANT")

        if (CMAKE_C_COMPILER_ID STREQUAL "SunPro")
            # set new config for SunPro
        SET(CMAKE_C_FLAGS "-D_REENTRANT -m64")
            message("SUN Studio compiler (building for 64bit) ")

            # Bug #219 !!! _REENTRANT makes errno thread safe
            # For GNU I guess
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -errtags -fast -erroff=E_PTR_TO_VOID_IN_ARITHMETIC ")
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -erroff=E_EMPTY_MBR_DECLARATION -erroff=E_NO_IMPLICIT_DECL_ALLOWED")
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -erroff=E_EMPTY_DECLARATION -erroff=E_ARGUEMENT_MISMATCH -erroff=E_ZERO_OR_NEGATIVE_SUBSCRIPT")
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -erroff=E_ARG_INCOMPATIBLE_WITH_ARG_L")
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -erroff=E_BAD_PTR_INT_COMBINATION")
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -erroff=SEC_UNINITIALIZED_MEM_READ")
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -erroff=E_INIT_DOES_NOT_FIT")
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -erroff=SEC_ARR_OUTSIDE_BOUND_READ")

            # Add Sun Studio C runtime
            SET(RT_LIB ${RT_LIB} Crun)
        endif()

    elseif(CMAKE_OS_NAME STREQUAL "FREEBSD")
        set(RT_LIB rt)
        # seem that threading model also does not work here...
        set(EX_CPM_NO_THREADS "1")
        set(EX_OS_FREEBSD "1")
        # Best option...
        set(EX_USE_KQUEUE "1")
        set(NDRX_LD_LIBRARY_PATH "LD_LIBRARY_PATH")
    elseif(CMAKE_OS_NAME STREQUAL "CYGWIN")
        set(EX_OS_CYGWIN "1")
        set(EX_USE_POLL "1")
        set(NDRX_LD_LIBRARY_PATH "LD_LIBRARY_PATH")
    elseif(CMAKE_OS_NAME STREQUAL "DARWIN")
        set(RT_LIB dl)
        set(EX_OS_DARWIN "1")
        set(EX_USE_EMQ "1")
        set(EX_USE_POLL "1")
        set(NDRX_LD_LIBRARY_PATH "DYLD_FALLBACK_LIBRARY_PATH")
    # Unknown OS:
    else()
        message( FATAL_ERROR "Unsupported OS" )
    endif()

    IF(CMAKE_SYSTEM_PROCESSOR MATCHES "^sparc")
        set(EX_ALIGNMENT_BYTES "8")
        set(EX_ALIGNMENT_FORCE "1")
    ENDIF ()

    IF (DEFINE_ALIGNMENT_FORCE)
        set(EX_ALIGNMENT_FORCE "1")
    ENDIF()


endmacro(ex_comp_settings)


# vim: set ts=4 sw=4 et smartindent:
