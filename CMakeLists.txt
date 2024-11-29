cmake_minimum_required(VERSION 3.18)

option(BUILD_SHARED_LIBS "Create a shared library instead of a static library." ON)

project(clownmdemu-libretro LANGUAGES C)

# Sources.
add_library(clownmdemu-libretro
	"libretro.c"
)

# Only require C90.
set_target_properties(clownmdemu-libretro PROPERTIES
	C_STANDARD 90
	C_STANDARD_REQUIRED NO
	C_EXTENSIONS OFF
)

# Add a definition from the Makefile.
target_compile_definitions(clownmdemu-libretro PRIVATE __LIBRETRO__)

# Link clownmdemu core.
add_subdirectory("common/clownmdemu" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu-libretro clownmdemu)
