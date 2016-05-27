// Macro.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <time.h>
#include <conio.h>
#define EXIT_SIGNAL 26
HHOOK hook;
HANDLE listenForExitThread;
HANDLE macroThread;

static void log(const char* msg) {
	time_t now;
	char buffer[26];
	struct tm tm_info;

	time(&now);
	localtime_s(&tm_info, &now);

	strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", &tm_info);
	printf("[%s] %s\n", buffer, msg);
}

static void logKeyEvent(char* pressedKey) {
	std::string msg("Key: ");
	msg.append(pressedKey).append(" is pressed.");
	log(msg.c_str());
}

static void simulateKeyEvent(char key) {
	INPUT input;
	WORD vkey = key;
	input.type = INPUT_KEYBOARD;
	input.ki.wScan = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
	input.ki.time = 0;
	input.ki.dwExtraInfo = 0;
	input.ki.wVk = vkey;

	input.ki.dwFlags = 0;// key down
	SendInput(1, &input, sizeof(INPUT));
	input.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &input, sizeof(INPUT));

	// set timer
	// TODO bind delay by user
	Sleep(300);
}

DWORD WINAPI KeySimulateThread(void* data) {
	log("Macro thread has been created.");
	while (true) {
		// TODO bind custom key
		simulateKeyEvent(VK_NUMPAD7);
		simulateKeyEvent(VK_NUMPAD8);
		simulateKeyEvent(VK_NUMPAD9);
	}
	return 0;
}

static void startMacro() {
	if (macroThread == NULL) {
		log("No Macro thread found, try to create one.");
		macroThread = CreateThread(NULL, 0, KeySimulateThread, NULL, 0, NULL);
		return;
	}
	ResumeThread(macroThread);
}

static void endMacro() {
	if (!macroThread) {
		log("No Macro thread found.");
		return;
	}
	SuspendThread(macroThread);
}

static void handle(char pressedKey) {
	switch (pressedKey) {
	case VK_HOME:
		// start the macrod
		logKeyEvent("HOME");
		startMacro();
		break;
	case VK_END:
		// end the macro
		logKeyEvent("END");
		endMacro();
		break;
	default:
		// useless keyboard event
		break;
	}
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HC_ACTION) {
		if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
			PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
			char code = (char)p->vkCode;
			handle(code);
		}
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static void terminateThread(HANDLE thread, char* threadName) {
	std::string msg("");
	if (!thread) {
		log(msg.append("No ").append(threadName).append(" thread found.").c_str());
	}
	else {
		// terminate the thread
		CloseHandle(thread);
		log(msg.append("Thread: ").append(threadName).append(" has been terminated.").c_str());
	}
}

static void exitMacro() {
	// unregister hook
	UnhookWindowsHookEx(hook);
	log("Hook has been unregistered.");

	terminateThread(macroThread, "SendKeyInputThread");
	terminateThread(listenForExitThread, "ListenThread");

	log("Process has been terminated successfully.");
	log("Press any key to close console.");
	char c = _getch();
	exit(0);
}

DWORD WINAPI HookThread(void* data) {
	log("Exit thread has been created and running.");

	char c;
	while ((c = _getch()) != EXIT_SIGNAL) {
		// loop to wait for exit signal
	}

	exitMacro();

	return 0;
}

static void init() {
	// register hook
	hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
	log("Hook has been registered, Macro process is started.");
	log("Press CTRL-Z to exit the process.");

	// create thread to listen to exit signal
	listenForExitThread = CreateThread(NULL, 0, HookThread, NULL, 0, NULL);
}

int main() {
	init();

	MSG msg;
	while (!GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	return 0;
}