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
HANDLE thread;

static void log(const char* msg) {
	time_t now;
	char buffer[26];
	struct tm tm_info;

	time(&now);
	localtime_s(&tm_info, &now);

	strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", &tm_info);
	printf("[%s] %s\n", buffer, msg);
}

static void simulateKeyEvent(char key) {
	INPUT input;
	WORD vkey = key;
	input.type = INPUT_KEYBOARD;
	input.ki.wScan = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
	input.ki.time = 1000;
	input.ki.dwExtraInfo = 0;
	input.ki.wVk = vkey;

	input.ki.dwFlags = 0;
	SendInput(1, &input, sizeof(INPUT));

	input.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &input, sizeof(INPUT));
}

DWORD WINAPI ThreadFunc(void* data) {
	log("thread has been created.");
	while (true) {
		simulateKeyEvent(VK_NUMPAD5);
		simulateKeyEvent(VK_NUMPAD6);
	}
	return 0;
}

static void startMacro() {
	if (thread == NULL) {
		log("No thread found, try to create one.");
		thread = CreateThread(NULL, 0, ThreadFunc, NULL, 0, NULL);
		return;
	}
	ResumeThread(thread);
}

static void endMacro() {
	if (!thread) {
		log("No thread found.");
		return;
	}
	SuspendThread(thread);
}

static void logKeyEvent(char* pressedKey) {
	std::string msg("Key: ");
	msg.append(pressedKey).append(" is pressed.");
	log(msg.c_str());
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

static void exitMacro() {
	// unregister hook
	UnhookWindowsHookEx(hook);
	if (!thread) {
		log("No thread found.");
	} else {
		CloseHandle(thread);
		log("thread has been terminated.");
	}

	printf("\nHook has been unregistered, press any key to exit.");
	char c = _getch();
	exit(0);
}

int main() {
	// register hook
	hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
	printf("Hook has been registered, Macro process is started.\n");
	printf("\nPress CTRL-Z to stop the process.\n");

	MSG msg;
	while (!GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// TODO add a exit function here
	return 0;
}