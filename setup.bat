@echo off

rem Store PATH before importing visual studio environment
if "%BAKE_USERPATH%"=="" (
  set "BAKE_USERPATH=%PATH%"
)

rem If started from visual studio prompt, use current installation
set VSDIR=%VSINSTALLDIR%

rem If not in visual studio prompt, find visual studio installation
if "%VSDIR%"=="" (
    for /l %%v in (2050, -1, 2015) do (
        if "%VSDIR%"=="" (
          if exist "C:\Program Files\Microsoft Visual Studio\%%v\Community\" (
              set VSDIR=C:\Program Files\Microsoft Visual Studio\%%v\Community
          )
        )
    )
)

rem If not found, try in x86 program files
if "%VSDIR%"=="" (
    for /l %%v in (2050, -1, 2015) do (
        if "%VSDIR%"=="" (
          if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%v\Community\" (
              set VSDIR=C:\Program Files (x86^)\Microsoft Visual Studio\%%v\Community
          )
        )
    )
)

rem If not found, find visual studio cli build tools installation
if "%VSDIR%"=="" (
    for /l %%v in (2050, -1, 2019) do (
        if "%VSDIR%"=="" (
          if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%v\BuildTools\" (
              set VSDIR=C:\Program Files (x86^)\Microsoft Visual Studio\%%v\BuildTools
          )
        )
    )
)

if "%VSDIR%"=="" (
    echo No visual studio installation found! Try running again in visual studio prompt.
    goto :eof
)

echo Visual studio installation: %VSDIR%

rem Setup visual studio command line environment
call "%VSDIR%\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64

rem Build & install bake
cd build-Windows
nmake clean all /NOLOGO
cd ..
bake.exe setup

rem Cleanup env
set "PATH=%BAKE_USERPATH%"
set VSDIR=
set LIB=
set LIBPATH=
set INCLUDE=
