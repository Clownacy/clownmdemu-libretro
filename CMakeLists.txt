cmake_minimum_required(VERSION 3.16.3)

option(BUILD_SHARED_LIBS "Create a shared library instead of a static library." ON)

project(clownmdemu-libretro LANGUAGES C)

# Sources.
add_library(clownmdemu_libretro
	"libretro.c"
	"libretro.h"
	"common/cd-reader.c"
	"common/cd-reader.h"
)

# Only require C90.
set_target_properties(clownmdemu_libretro PROPERTIES
	C_STANDARD 90
	C_STANDARD_REQUIRED NO
	C_EXTENSIONS OFF
)

# Avoid linker errors on Linux.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Provide an 'stdint.h' for older versions of MSVC.
if(MSVC AND MSVC_VERSION LESS 1600)
	target_include_directories(clownmdemu_libretro PRIVATE "libretro-common/include/compat/msvc")
endif()

# Prevent the 'lib' prefix from being added, to match the behaviour of the Makefile.
set_target_properties(clownmdemu_libretro PROPERTIES PREFIX "")

# Add a definition from the Makefile.
target_compile_definitions(clownmdemu_libretro PRIVATE __LIBRETRO__)

# Link clownmdemu-frontend-common.
add_subdirectory("common" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu_libretro clownmdemu-frontend-common)
