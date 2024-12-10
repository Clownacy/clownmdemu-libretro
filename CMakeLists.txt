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

# Prevent the 'lib' prefix from being added, to match the behaviour of the Makefile.
set_target_properties(clownmdemu_libretro PROPERTIES PREFIX "")

# Add a definition from the Makefile.
target_compile_definitions(clownmdemu_libretro PRIVATE __LIBRETRO__)

# Link clowncd.
add_subdirectory("common/clowncd" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu_libretro clowncd)

# Link clownmdemu core.
add_subdirectory("common/core" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu_libretro clownmdemu-core)
