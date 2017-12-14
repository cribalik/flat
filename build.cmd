@echo off

IF NOT EXIST .\build mkdir .\build

cl .\src\flat.c -LD /link opengl32.lib -EXPORT:main_loop -EXPORT:init -incremental:no
REM cl .\src\flat_sdl.c
