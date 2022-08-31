@echo off & setlocal enableextensions enabledelayedexpansion
::
:: tg.bat - compile & run tests (GNUC).
::

set      unit=string_view
set unit_file=string-view

:: if no std is given, use c++11

set std=%1
set args=%2 %3 %4 %5 %6 %7 %8 %9
if "%1" == "" set std=c++11

set gpp=g++

call :CompilerVersion version
echo %gpp% %version%: %std% %args%

set UCAP=%unit%
call :toupper UCAP

set unit_select=-D%unit_prfx%_CONFIG_SELECT_%UCAP%=%unit_prfx%_%UCAP%_DEFAULT
::set unit_select=-D%unit_prfx%_CONFIG_SELECT_%UCAP%=%unit_prfx%_%UCAP%_NONSTD
::set unit_select=-D%unit_prfx%_CONFIG_SELECT_%UCAP%=%unit_prfx%_%UCAP%_STD

set unit_config=^
    -Dnssv_CONFIG_STD_SV_OPERATOR=1 ^
    -Dnssv_CONFIG_USR_SV_OPERATOR=1 ^
    -Dnssv_CONFIG_CONFIRMS_COMPILATION_ERRORS=0 ^
    -Dnssv_STRING_VIEW_HEADER=\"nonstd/string_view.hpp\"

rem -flto / -fwhole-program
set  optflags=-O2
set warnflags=-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wno-padded -Wno-missing-noreturn

%gpp% -std=%std% %optflags% %warnflags% %unit_select% %unit_config% -o %unit_file%-main.t.exe -isystem lest -I../include -I. %unit_file%-main.t.cpp %unit_file%.t.cpp && %unit_file%-main.t.exe

endlocal & goto :EOF

:: subroutines:

:CompilerVersion  version
echo off & setlocal enableextensions
set tmpprogram=_getcompilerversion.tmp
set tmpsource=%tmpprogram%.c

echo #include ^<stdio.h^>     > %tmpsource%
echo int main(){printf("%%d.%%d.%%d\n",__GNUC__,__GNUC_MINOR__,__GNUC_PATCHLEVEL__);} >> %tmpsource%

%gpp% -o %tmpprogram% %tmpsource% >nul
for /f %%x in ('%tmpprogram%') do set version=%%x
del %tmpprogram%.* >nul
endlocal & set %1=%version%& goto :EOF

:: toupper; makes use of the fact that string
:: replacement (via SET) is not case sensitive
:toupper
for %%L IN (A B C D E F G H I J K L M N O P Q R S T U V W X Y Z) DO SET %1=!%1:%%L=%%L!
goto :EOF
