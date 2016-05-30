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
#define LOG_ERROR "ERROR"
#define LOG_INFO "INFO"
#define CONFIG_FILE_PATH ".\\configMacro.ini"

LPCTSTR filePath = _T(CONFIG_FILE_PATH);
LPCTSTR appName = _T("macro");
LPCTSTR key_delay = _T("delay");
LPCTSTR key_first = _T("first hotkey");
LPCTSTR key_second = _T("second hotkey");
LPCTSTR key_third = _T("thrid hotkey");
LPCTSTR key_fourth = _T("fourth hotkey");

HHOOK hook;
HANDLE listenForExitThread;
HANDLE macroThread;

struct Config {
	short delay;
	short first;
	short second;
	short third;
	short fourth;
};

void exitProgram();

static void log(const char* severity, const char* msg) {
	time_t now;
	char buffer[26];
	struct tm tm_info;

	time(&now);
	localtime_s(&tm_info, &now);

	strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", &tm_info);
	printf("[%s] %s: %s\n", buffer, severity, msg);
}

static void log(const char* msg) {
	log(LOG_INFO, msg);
}

static void logKeyEvent(char* pressedKey) {
	std::string msg("Key: ");
	msg.append(pressedKey).append(" is pressed.");
	log(msg.c_str());
}

static void simulateKeyEvent(char key, unsigned short delay) {
	if (key == 0x59) {
		// if user did not bind this key, the value will be 60 - 1 = 59
		return;
	}

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
	Sleep(delay);
}

static void readConfig(Config* config) {
	// by default the delay is 200ms
	config->delay = GetPrivateProfileInt(appName, key_delay, 200, filePath);
	// by default the key bind is NULL
	config->first = GetPrivateProfileInt(appName, key_first, -1, filePath);
	config->second = GetPrivateProfileInt(appName, key_second, -1, filePath);
	config->third = GetPrivateProfileInt(appName, key_third, -1, filePath);
	config->fourth = GetPrivateProfileInt(appName, key_fourth, -1, filePath);
}

static BOOL isFileExists(const char* path) {
	DWORD ftyp = GetFileAttributesA(path);
	if (ftyp == INVALID_FILE_ATTRIBUTES)
		return false;
	return true;
}

static void writeConfig() {
	if (isFileExists(CONFIG_FILE_PATH)) {
		log("configMacro.ini already exists.");
		return;
	}

	// this code section smells bad
	BOOL result = true;
	result &= WritePrivateProfileString(appName, key_delay, _T("200"), filePath);
	result &= WritePrivateProfileString(appName, key_first, _T("6"), filePath);
	result &= WritePrivateProfileString(appName, key_second, _T("7"), filePath);
	result &= WritePrivateProfileString(appName, key_third, _T("8"), filePath);
	result &= WritePrivateProfileString(appName, key_fourth, _T("9"), filePath);

	if (!result) {
		log(LOG_ERROR, "configMacro.ini write failed.");
	}
	else {
		log("configMacro.ini has been successfully written.");
	}
}

static short getBindedKey(const short value) {
	if ((value < 0 || value > 9) && value != -1) {
		log(LOG_ERROR, "currently only support Number: 0-9 to bind the key.");
		exitProgram();
	}

	return 0x60 + value;
}

DWORD WINAPI KeySimulateThread(void* data) {
	log("Macro thread has been created.");

	log("reading the config...");
	Config* config = (Config*)malloc(sizeof(Config));
	readConfig(config);
	short first = getBindedKey(config->first), second = getBindedKey(config->second), third = getBindedKey(config->third), fourth = getBindedKey(config->fourth), delay = config->delay;
	free(config);
	log("read the config successfully.");

	while (true) {
		simulateKeyEvent(first, delay);
		simulateKeyEvent(second, delay);
		simulateKeyEvent(third, delay);
		simulateKeyEvent(fourth, delay);
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
		// start the macro
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

static void exitProgram() {
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

	exitProgram();

	return 0;
}

static void init() {
	// write the config file if needed
	writeConfig();

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