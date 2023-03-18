#
# 86Box    A hypervisor and IBM PC system emulator that specializes in
#          running old operating systems and software designed for IBM
#          PC systems and compatibles from 1981 through fairly recent
#          system designs based on the PCI bus.
#
#          This file is part of the 86Box distribution.
#
#          CMake build script.
#
# Authors: Cacodemon345
#          David Hrdlička, <hrdlickadavid@outlook.com>
#
#          Copyright 2021      Cacodemon345.
#          Copyright 2021      David Hrdlička.
#

add_library(plat OBJECT winsr.c winsr_dynld.c winsr_cdrom.c)
# winsr_keyboard.c win_mouse.c

add_library(ui OBJECT winsr_sdl.c glad.c winsr_opengl_glslp.c winsr_opengl.c)
 
if(NOT CPPTHREADS)
    target_sources(plat PRIVATE winsr_thread.c)
endif()

if(RTMIDI)
    target_compile_definitions(ui PRIVATE USE_RTMIDI)
endif()

# CMake 3.22 messed this up for clang/clang++
# See https://gitlab.kitware.com/cmake/cmake/-/issues/23066
if(MSVC OR (NOT MINGW AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.22))
    # MSVC linker adds its own manifest to the executable, which fails if
    # we include ours in 86Box.rc. We therefore need to pass the manifest
    # directly as as a source file, so the linker can use that instead.
    set_property(SOURCE 86Box.rc PROPERTY COMPILE_DEFINITIONS NO_INCLUDE_MANIFEST)
    target_sources(86Box PRIVATE 86Box.manifest)
endif()

if(NOT MINGW)
    # Append null to resource strings (fixes file dialogs)
    # set_property(SOURCE 86Box.rc PROPERTY COMPILE_FLAGS -n)

    # `opendir` is only included in MinGW, so include an implementation
    # for other builds.
    target_sources(plat PRIVATE winsr_opendir.c)
endif()

#if(DINPUT)
#    target_sources(plat PRIVATE winsr_joystick.cpp)
#    target_link_libraries(86Box dinput8)
#else()
#    target_sources(plat PRIVATE winsr_joystick_rawinput.c)
#endif()

    target_sources(plat PRIVATE winsr_sdl2_joystick.c)
 
target_link_libraries(86Box advapi32 comctl32 comdlg32 gdi32 shell32 iphlpapi
    dxguid imm32 hid setupapi uxtheme version winmm psapi ws2_32) 

