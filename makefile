#

all: ctetris_win.exe utetris_win.exe

ctetris_win.exe: ctetris.c ctetris_win.c
    cl /O1 /Os /GS- /Oy /FAscu ctetris_win.c /link /MAP /RELEASE /FIXED /STUB:stub.bin /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /NODEFAULTLIB kernel32.lib Rpcrt4.lib

utetris_win.exe: utetris.c utetris.h utetfunc.h utetris_win.c
    cl /O1 /Os /GS- /Oy /FAscu /arch:IA32 /Feutetris_win.exe utetris.c utetris_win.c /link /MAP /RELEASE /FIXED /STUB:stub.bin /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /NODEFAULTLIB kernel32.lib Rpcrt4.lib
