@echo off & setlocal enableextensions enabledelayedexpansion
::
:: t.bat - compile & run tests (MSVC).
::

set      unit=string_view
set unit_file=string-view
set unit_prfx=nssv

:: if no std is given, use compiler default

set std=%1
if not "%std%"=="" set std=-std:%std%

call :CompilerVersion version
echo VC%version%: %args%

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

set msvc_defines=^
    -D_CRT_SECURE_NO_WARNINGS ^
    -D_SCL_SECURE_NO_WARNINGS

::    -Dnssv_CONFIG_NO_EXCEPTIONS=1
::    -Dnssv_CONFIG_STD_SV_OPERATOR=1
::    -Dnssv_CONFIG_CONVERSION_STD_STRING=1
::    -Dnssv_CONFIG_CONVERSION_STD_STRING_CLASS_METHODS=1
::    -Dnssv_CONFIG_CONVERSION_STD_STRING_FREE_FUNCTIONS=1
::    -Dnssv_CONFIG_NO_STREAM_INSERTION=1

set CppCoreCheckInclude=%VCINSTALLDIR%\Auxiliary\VS\include

cl -nologo -W3 -EHsc %std% %unit_select% %unit_config% %msvc_defines% -I"%CppCoreCheckInclude%" -Ilest -I../include -I. %unit_file%-main.t.cpp %unit_file%.t.cpp && %unit_file%-main.t.exe
endlocal & goto :EOF

:: subroutines:

:CompilerVersion  version
@echo off & setlocal enableextensions
set tmpprogram=_getcompilerversion.tmp
set tmpsource=%tmpprogram%.c

echo #include ^<stdio.h^>                   >%tmpsource%
echo int main(){printf("%%d\n",_MSC_VER);} >>%tmpsource%

cl /nologo %tmpsource% >nul
for /f %%x in ('%tmpprogram%') do set version=%%x
del %tmpprogram%.* >nul
set offset=0
if %version% LSS 1900 set /a offset=1
set /a version="version / 10 - 10 * ( 5 + offset )"
endlocal & set %1=%version%& goto :EOF

:: toupper; makes use of the fact that string
:: replacement (via SET) is not case sensitive
:toupper
for %%L IN (A B C D E F G H I J K L M N O P Q R S T U V W X Y Z) DO SET %1=!%1:%%L=%%L!
goto :EOF
