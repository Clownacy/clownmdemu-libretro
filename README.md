# Overview

This is ClownMDEmu, a Sega Mega Drive (a.k.a. Sega Genesis) emulator.

Some standard features of the Mega Drive are currently unemulated (see
`common/core/TODO.md` for more information).

This repository contains a frontend that exposes ClownMDEmu as a libretro core.
It is written in C89 and should provide all of the same features as the
standalone frontend aside from the debug menus.


# Compiling

As well as a CMake script, a standard libretro Makefile is provided too.

Be aware that this repo uses Git submodules: use
`git submodule update --init --recursive` to pull in these submodules before
compiling.


# Licence

ClownMDEmu is free software, licensed under the AGPLv3 (or any later version).
See `LICENCE.txt` for more information.
