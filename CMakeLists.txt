cmake_minimum_required(VERSION 3.0...3.16.3)

option(BUILD_SHARED_LIBS "Create a shared library instead of a static library." ON)
option(UNITY_BUILD "Perform a unity-build, to produce a single self-contained library file." OFF)

project(clownmdemu_libretro LANGUAGES C)

############################################
# Standard libretro core boilerplate code. #
############################################

# Allow only the libretro API functions to be exported, improving optimization.
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Avoid some relocation-related linker errors when building a shared library that depends on static libraries.
get_property(SUPPORTS_SHARED GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS)
if(SUPPORTS_SHARED AND BUILD_SHARED_LIBS)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

###################################################
# End of standard libretro core boilerplate code. #
###################################################

if(UNITY_BUILD)
	# A unity build must be used so that static builds produce a single static library,
	# as libretro's CI does not account for more than one static library being produced.
	add_library(clownmdemu_libretro
		"unity.c"
	)
else()
	add_library(clownmdemu_libretro
		"source/clowncd-callbacks.c"
		"source/clowncd-callbacks.h"
		"source/file-io.c"
		"source/file-io.h"
		"source/libretro-interface.c"
		"source/libretro-interface.h"
		"source/options.h"
	)

	add_subdirectory("common" EXCLUDE_FROM_ALL)
	target_link_libraries(clownmdemu_libretro PRIVATE clownmdemu-frontend-common)
endif()

# Only require C90.
# Do not prevent extensions, since some may optionally
# be used for a performance boost (as Clown68000 does).
set_target_properties(clownmdemu_libretro PROPERTIES
	C_STANDARD 90
	C_STANDARD_REQUIRED NO
)

############################################
# Standard libretro core boilerplate code. #
############################################

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

# Handle file extensions.
if(LIBRETRO_STATIC AND EMSCRIPTEN)
	# Emscripten builds are expected to use the '.bc' file extension instead of '.a'.
	set_target_properties(clownmdemu_libretro PROPERTIES SUFFIX ".bc")
elseif(NINTENDO_SWITCH2)
	# Switch 2 builds need to be in the '.nro' format.
	set_target_properties(clownmdemu_libretro PROPERTIES SUFFIX ".nro")
	ounce_create_library_nro(clownmdemu_libretro)
endif()

# Obtain the Git revision so that it can be displayed in the core's version string.
find_package(Git)
if(GIT_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
		OUTPUT_VARIABLE GIT_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(GIT_VERSION)
		target_compile_definitions(clownmdemu_libretro PRIVATE GIT_VERSION=" ${GIT_VERSION}")
	endif()
endif()

###################################################
# End of standard libretro core boilerplate code. #
###################################################
