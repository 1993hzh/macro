// Macro.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <time.h>
#include <conio.h>
typedef unsigned char BYTE;
#define EXIT_SIGNAL 26
#define LOG_ERROR "ERROR"
#define LOG_INFO "INFO"
#define CONFIG_FILE_PATH ".\\configMacro.ini"
#define SPLIT_SYMBOL ","

LPCTSTR filePath = _T(CONFIG_FILE_PATH);
LPCTSTR appName = _T("macro");
LPCTSTR key_delay = _T("delay");
LPCTSTR hot_key = _T("hotkey");

HHOOK hook;
HANDLE listenForExitThread;
HANDLE simulateKeyPressThread;
BYTE ENABLED_HOTKEY = 0;
BOOL isRun = true;

struct Config {
	unsigned short delay;
	BYTE* hotKeys;
};

Config* config;
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

static void simulateKeyEvent(const unsigned short key, unsigned short delay) {
	if (key == -1) {
		// user did not bind this key
		return;
	}

	INPUT input;
	input.type = INPUT_KEYBOARD;
	input.ki.time = 0;
	input.ki.dwExtraInfo = 0;
	input.ki.wVk = 0;// use hardware scan code instead

	input.ki.wScan = key;
	input.ki.dwFlags = KEYEVENTF_SCANCODE;
	SendInput(1, &input, sizeof(INPUT));
	// key release
	input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
	SendInput(1, &input, sizeof(INPUT));

	// set timer
	Sleep(delay);
}

static BYTE getBindedKey(char* str) {
	const char value = str[0];// currently only support numpad
	if (value < 0x30 || value > 0x39) {// ascii code
		log(LOG_ERROR, "currently only support Number: 0-9 to bind the key.");
		exitProgram();
	}
	ENABLED_HOTKEY++;
	return value == 0x30 ? 11 : value - 47;
}

static BYTE* split(char* str) {
	BYTE pos = 0;
	BYTE* array = (BYTE*)calloc(10, sizeof(BYTE));

	char *current = NULL, *next = NULL;
	current = strtok_s(str, SPLIT_SYMBOL, &next);
	while (current != NULL) {
		pos = ENABLED_HOTKEY;
		array[pos] = getBindedKey(current);
		current = strtok_s(NULL, SPLIT_SYMBOL, &next);
	}
	return array;
}

static void readConfig(Config* config) {
	// by default the delay is 200ms
	config->delay = GetPrivateProfileInt(appName, key_delay, 200, filePath);

	// by default the key bind is 6,7,8
	wchar_t* buffer = (wchar_t*)calloc(20, sizeof(wchar_t));
	char* keys = (char*)calloc(20, sizeof(char));
	GetPrivateProfileString(appName, hot_key, _T("6,7,8"), buffer, 20, filePath); // suppose 0-9 is all used,then the max_size should be 2*10-1+1 ('\0')

	size_t charsConverted = 0;
	wcstombs_s(&charsConverted, keys, 20, buffer, 20);
	config->hotKeys = split(keys);

	free(buffer);
	free(keys);
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

	BOOL result = true;
	result &= WritePrivateProfileString(appName, key_delay, _T("200"), filePath);
	result &= WritePrivateProfileString(appName, hot_key, _T("6,7,8"), filePath);

	if (!result) {
		log(LOG_ERROR, "configMacro.ini write failed.");
	}
	else {
		log("configMacro.ini has been successfully written.");
	}
}

DWORD WINAPI KeySimulateThread(void* data) {
	log("Simulate keypress thread has started.");

	Config* config = (Config*)data;
	while (isRun) {
		for (int i = 0; i < ENABLED_HOTKEY; i++) {
			simulateKeyEvent(config->hotKeys[i], config->delay);
		}
	}
	return 0;
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

static void startMacro() {
	if (!simulateKeyPressThread) {
		isRun = true;
		simulateKeyPressThread = CreateThread(NULL, 0, KeySimulateThread, (void*)config, 0, NULL);
	}
	//ResumeThread(simulateKeyPressThread);
}

static void endMacro() {
	isRun = false;
	simulateKeyPressThread = NULL;
	/*
	if (!simulateKeyPressThread) {
		log(LOG_ERROR, "No simulate key press thread found!");
		return;
	}
	CloseHandle(simulateKeyPressThread);
	*/
	//SuspendThread(simulateKeyPressThread);
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

	terminateThread(simulateKeyPressThread, "SimulateKeyInputThread");
	free(config);
	terminateThread(listenForExitThread, "ListenThread");

	log("Process has been terminated successfully.");
	log("Press any key to close console.");
	char c = _getch();
	exit(0);
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

	// read the config file
	log("reading the config file...");
	config = (Config*)malloc(sizeof(Config));
	readConfig(config);
	log("read the config file successfully.");
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