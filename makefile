#

all: ctetris_win.exe

ctetris_win.exe: ctetris_win.c
	cl /O1 /Os /GS- ctetris_win.c /link /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /NODEFAULTLIB kernel32.lib Rpcrt4.lib