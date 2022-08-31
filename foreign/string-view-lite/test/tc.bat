@echo off & setlocal enableextensions enabledelayedexpansion
::
:: tc.bat - compile & run tests (clang).
::

set      unit=string_view
set unit_file=string-view

:: if no std is given, use c++14

set std=%1
if "%std%"=="" set std=c++14

set  clang=clang

call :CompilerVersion version
echo %clang% %version%: %std%

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
set warnflags=-Wall -Wextra -Wpedantic -Weverything -Wshadow -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-missing-noreturn -Wno-documentation-unknown-command -Wno-documentation-deprecated-sync -Wno-documentation -Wno-weak-vtables -Wno-missing-prototypes -Wno-missing-variable-declarations -Wno-exit-time-destructors -Wno-global-constructors

"%clang%" -m32 -std=%std% %optflags% %warnflags% %unit_select% %unit_config% -fms-compatibility-version=19.00 -isystem "%VCInstallDir%include" -isystem "%WindowsSdkDir_71A%include" -isystem lest -I../include -I. -o %unit_file%-main.t.exe %unit_file%-main.t.cpp %unit_file%.t.cpp && %unit_file%-main.t.exe
endlocal & goto :EOF

:: subroutines:

:CompilerVersion  version
echo off & setlocal enableextensions
set tmpprogram=_getcompilerversion.tmp
set tmpsource=%tmpprogram%.c

echo #include ^<stdio.h^>     > %tmpsource%
echo int main(){printf("%%d.%%d.%%d\n",__clang_major__,__clang_minor__,__clang_patchlevel__);} >> %tmpsource%

"%clang%" -m32 -o %tmpprogram% %tmpsource% >nul
for /f %%x in ('%tmpprogram%') do set version=%%x
del %tmpprogram%.* >nul
endlocal & set %1=%version%& goto :EOF

:: toupper; makes use of the fact that string
:: replacement (via SET) is not case sensitive
:toupper
for %%L IN (A B C D E F G H I J K L M N O P Q R S T U V W X Y Z) DO SET %1=!%1:%%L=%%L!
goto :EOF
