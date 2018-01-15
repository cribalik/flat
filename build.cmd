@echo off

IF NOT EXIST .\build mkdir .\build
IF NOT EXIST .\build\x86 mkdir .\build\x86

pushd .\build\x86

cl /Zi ..\..\src\flat.cpp -LD /link -EXPORT:main_loop -EXPORT:init -incremental:no -opt:ref /debug
cl /Zi ..\..\src\flat_sdl.cpp -I..\..\include /link opengl32.lib ..\..\SDL2.lib -incremental:no /debug
REM cl .\src\flat_sdl.cpp

popd
