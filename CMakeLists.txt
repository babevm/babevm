cmake_minimum_required(VERSION 3.5)
project(io_babevm C)

set(CMAKE_C_STANDARD 90)

# pass -DWINDOWS=ON or -DLINUX=ON on command line to set
option(WINDOWS "Windows" OFF)
option(LINUX "Linux" OFF)
option(32BIT "32 Bit" OFF)

# enable the correct one of these for platform (OSX uses linux)
if(LINUX)
    add_definitions(-DBVM_PLATFORM_LINUX)
    include_directories(src/platform/linux/h)
endif()

if(WINDOWS)
    add_definitions(-DBVM_PLATFORM_WINDOWS)
    include_directories(src/platform/winos/h)
endif()

add_executable(babe
        src/platform/ansi/c/pd_console.c
        src/platform/ansi/c/pd_file.c
        src/platform/ansi/c/pd_memory.c
        src/c/debugger/debugger-cmd-01-virtualmachine.c
        src/c/debugger/debugger-cmd-02-referencetype.c
        src/c/debugger/debugger-cmd-03-classtype.c
        src/c/debugger/debugger-cmd-04-arraytype.c
        src/c/debugger/debugger-cmd-06-method.c
        src/c/debugger/debugger-cmd-09-objectreference.c
        src/c/debugger/debugger-cmd-10-stringreference.c
        src/c/debugger/debugger-cmd-11-threadreference.c
        src/c/debugger/debugger-cmd-12-threadgroupreference.c
        src/c/debugger/debugger-cmd-13-arrayreference.c
        src/c/debugger/debugger-cmd-14-classloaderreference.c
        src/c/debugger/debugger-cmd-15-eventrequest.c
        src/c/debugger/debugger-cmd-16-stackframe.c
        src/c/debugger/debugger-cmd-17-classobjectreference.c
        src/c/debugger/debugger-cmd-64-event.c
        src/c/debugger/debugger-id.c
        src/c/debugger/debugger-io-inputstream.c
        src/c/debugger/debugger-io-outputstream.c
        src/c/debugger/debugger-io-transport-sockets.c
        src/c/debugger/debugger-io.c
        src/c/debugger/debugger-roots.c
        src/c/debugger/debugger.c
        src/c/clazz.c
        src/c/collector.c
        src/c/exec.c
        src/c/file.c
        src/c/fp.c
        src/c/frame.c
        src/c/heap.c
        src/c/int64.c
        src/c/babe.c
        src/c/native.c
        src/c/ni.c
        src/c/net.c
        src/c/object.c
        src/c/pool_clazz.c
        src/c/pool_internstring.c
        src/c/pool_nativemethod.c
        src/c/pool_utfstring.c
        src/c/stacktrace.c
        src/c/string.c
        src/c/thread.c
        src/c/trycatch.c
        src/c/utfstring.c
        src/c/vm.c
        src/c/zip.c
        src/h/debugger/debugger-commands.h
        src/h/debugger/debugger-events.h
        src/h/debugger/debugger-id.h
        src/h/debugger/debugger-io-inputstream.h
        src/h/debugger/debugger-io-outputstream.h
        src/h/debugger/debugger-io-transport.h
        src/h/debugger/debugger-io.h
        src/h/debugger/debugger-jdwp.h
        src/h/debugger/debugger-roots.h
        src/h/debugger/debugger.h
        src/h/pd/pd.h
        src/h/pd/pd_console.h
        src/h/pd/pd_file.h
        src/h/pd/pd_memory.h
        src/h/pd/pd_sockets.h
        src/h/pd/pd_system.h
        src/h/clazz.h
        src/h/collector.h
        src/h/define.h
        src/h/exec.h
        src/h/file.h
        src/h/fp.h
        src/h/frame.h
        src/h/heap.h
        src/h/int64.h
        src/h/int64_emulated.h
        src/h/bvm.h
        src/h/native.h
        src/h/net.h
        src/h/ni-types.h
        src/h/ni.h
        src/h/object.h
        src/h/platform-types.h
        src/h/pool_clazz.h
        src/h/pool_internstring.h
        src/h/pool_nativemethod.h
        src/h/pool_utfstring.h
        src/h/stacktrace.h
        src/h/string.h
        src/h/thread.h
        src/h/trycatch.h
        src/h/utfstring.h
        src/h/vm.h
        src/h/zip.h
        src/thirdparty/xp/xp.h
        src/thirdparty/xp/xp.c
        src/thirdparty/tinf-master/src/tinf.h
        src/thirdparty/tinf-master/src/tinflate.c
        )

if(WINDOWS)
    # for mingw to link in winsock
    target_link_libraries(babe wsock32 ws2_32)
    target_sources(babe PRIVATE
            src/platform/winos/h/bvm_pd.h
            src/platform/winos/c/pd_system.c
            src/platform/sockets/c/pd_bsd_win_socket.c
            )
endif()

if(LINUX)
    target_sources(babe PRIVATE
            src/platform/linux/h/bvm_pd.h
            src/platform/linux/c/pd_system.c
            src/platform/sockets/c/pd_bsd_win_socket.c
            )
    target_link_libraries(babe PUBLIC -lm)
endif()

if(32BIT)
    add_definitions(-DBVM_32BIT_ENABLE)
    # -mfpmath=sse to sort out 32 bit strict fp?
    set_target_properties(babe PROPERTIES COMPILE_OPTIONS "-m32 " LINK_FLAGS "-m32")
endif()




