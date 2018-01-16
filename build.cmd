@echo off

set compiler_flags=-Od -MT -nologo -fp:fast -fp:except- -Gm- -GR- -Zo -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4127 -FC -Z7
set linker_flags=-incremental:no -opt:ref

IF NOT EXIST .\build mkdir .\build
IF NOT EXIST .\build\x86 mkdir .\build\x86

del flat_*.pdb >NUL 2>NUL

cl %compiler_flags% ..\..\src\flat.cpp     -LD -link -PDB:flat_%random%.pdb -EXPORT:main_loop -EXPORT:init %linker_flags% -debug
cl %compiler_flags% ..\..\src\flat_sdl.cpp -I..\..\include -link %linker_flags% opengl32.lib ..\..\SDL2.lib -debug
