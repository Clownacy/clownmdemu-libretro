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

############################################
# Standard libretro core boilerplate code. #
############################################

# Avoid some relocation-related linker errors when building a shared library that depends on static libraries.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include libretro-common, so that 'libretro.h' can be found.
target_include_directories(clownmdemu_libretro PRIVATE "libretro-common/include")

# Provide an 'stdint.h' for older versions of MSVC.
if(MSVC AND MSVC_VERSION LESS 1600)
	target_include_directories(clownmdemu_libretro PRIVATE "libretro-common/include/compat/msvc")
endif()

# Give the code a way of determining that it is targeting libretro.
target_compile_definitions(clownmdemu_libretro PRIVATE __LIBRETRO__)

# Adjust the library's filename. This is relied upon by libretro's CI infrastructure.

# Remove the 'lib' prefix.
set_target_properties(clownmdemu_libretro PROPERTIES PREFIX "")

# Append the given suffix.
set_target_properties(clownmdemu_libretro PROPERTIES OUTPUT_NAME "clownmdemu_libretro${LIBRETRO_SUFFIX}")

# Emscripten builds are expected to use the '.bc' file extension instead of '.a'.
if(LIBRETRO_STATIC AND EMSCRIPTEN)
	set_target_properties(clownmdemu_libretro PROPERTIES SUFFIX ".bc")
endif()
