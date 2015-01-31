rem This scripts updates the environment (that needs to be previously set up for building with vcvarsall)
rem so that the boost will build with xp compatibility in VS2013.
rem Execute this script in the boost directory.

set INCLUDE=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Include;%INCLUDE%
set PATH=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Bin;%PATH%
set CL=/D_USING_V110_SDK71_;%CL%
set LIB=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Lib;%LIB%
set LINK=/SUBSYSTEM:CONSOLE,5.01 %LINK%

bjam --toolset=msvc-12.0 --link=shared --threading=multi --runtime-link=shared address-model=32 define=_BIND_TO_CURRENT_VCLIBS_VERSION --build-type=complete --stagedir=lib-12.0\x32 stage
