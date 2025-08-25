cmake_minimum_required(VERSION 3.0...3.16.3)

option(BUILD_SHARED_LIBS "Create a shared library instead of a static library." ON)

project(clownmdemu_libretro LANGUAGES C)

# Sources.
# A unity build must be used so that static builds produce a single static library,
# as libretro's CI does not account for more than one static library being produced.
add_library(clownmdemu_libretro
	"unity.c"
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

# Specify the library name, and append the libretro suffix to it.
set_target_properties(clownmdemu_libretro PROPERTIES OUTPUT_NAME "clownmdemu_libretro${LIBRETRO_SUFFIX}")

# libretro's CI expects Emscripten builds to end in '.bc' instead of '.a'.
if(LIBRETRO_STATIC AND EMSCRIPTEN)
	set_target_properties(clownmdemu_libretro PROPERTIES SUFFIX ".bc")
endif()

# Add a definition from the Makefile.
target_compile_definitions(clownmdemu_libretro PRIVATE __LIBRETRO__)
