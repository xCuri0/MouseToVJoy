#include <windows.h>
#include <iostream>
#include <stdio.h>
#include <time.h>
#include "fileRead.h"
#include "vjoy.h"
#include "mousetovjoy.h"
#include "public.h"
#include "vjoyinterface.h"
#include "input.h"
#include "stopwatch.h"
#include "forcefeedback.h"

int wheelPos = 0;
using namespace std;
using win32::Stopwatch;
//Instantiate classes
const char g_szClassName[] = "myWindowClass";
HWND hwnd;
WNDCLASSEX wc;
MSG msgWindow; //Intantiating a MSG class, which recives messages from "dummy" window 
VJoy vJ;
MouseToVjoy mTV;//Class that gets data, then processes it and modifies axises.
CInputDevices rInput;//Class that helps with determining what key was pressed.
FileRead fR;//Class used for reading and writing to config.txt file.
ForceFeedBack fFB;//Used to recive and interpret ForceFeedback calls from game window.
Stopwatch sw;//Measuring time in nanoseconds
INT axisX, axisY, axisZ, axisRX, ffbStrength; //Local variables that stores all axis values and forcefeedback strength we need.
BOOL isButton1Clicked, isButton2Clicked, isButton3Clicked, ctrlDown; //Bools that stores information if button was pressed.
void CALLBACK FFBCALLBACK(PVOID data, PVOID userData) {//Creating local callback which just executes callback from ForceFeedBack class.
	fFB.ffbToVJoy(data, userData);
}
LRESULT CALLBACK keyboardHook(int nCode, WPARAM wParam, LPARAM lParam) {
	switch (wParam)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)(lParam);
		USHORT vkCode = MapVirtualKey(p->scanCode, MAPVK_VSC_TO_VK);

		if (vkCode == 17 && p->dwExtraInfo != 6969) {
			if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
				ctrlDown = true;
			else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
				ctrlDown = false;
			break;
		}
		for (int i = 7; i <= 14; i++)
			if ((int)fR.result(i) == vkCode && (int)fR.result(i) != 17 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && ctrlDown) {
				// Set the CTRL key to up and set dwExtraInfo to le funni number so we can ignore it
				keybd_event(17, 0, KEYEVENTF_KEYUP, 6969);
				break;
			}
		break;
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}
BOOL exitHandler(DWORD event) {
	if (event == CTRL_CLOSE_EVENT) {
		mTV.inputLogic(rInput, axisX, axisY, axisZ, axisRX, isButton1Clicked, isButton2Clicked, isButton3Clicked, fR.result(1), fR.result(2), fR.result(3), fR.result(4), fR.result(5), fR.result(6), (int)fR.result(7), (int)fR.result(8), (int)fR.result(9), (int)fR.result(10), (int)fR.result(11), (int)fR.result(12), (int)fR.result(13), (int)fR.result(14), (int)fR.result(15), fR.result(17), fR.result(18), fR.result(19), (int)fR.result(22), sw.elapsedMilliseconds(), NULL);
		return true;
	}
	return false;
}
//Code that is run once application is initialized, test virtual joystick and accuires it, also it reads config.txt file and prints out menu and variables.
void initializationCode() {
	//Code that is run only once, tests vjoy device, reads config file and prints basic out accuired vars.
	UINT DEV_ID = 1;
	vJ.testDriver();//Test if driver is installed and compatible.
	vJ.testVirtualDevices(DEV_ID);//Test if virtually created joystick is up and running.
	vJ.accuireDevice(DEV_ID);//Accuire virtual joystick of index number DEV_ID
	vJ.enableFFB(DEV_ID);//Enable virtual joystick of index number DEV_ID to accept forcefeedback calls
	FfbRegisterGenCB(FFBCALLBACK, &DEV_ID);//Registed what function to run on forcefeedback call.
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)(exitHandler), TRUE);//Set the exit handler
	string configFileName = "config.txt";
	//what strings to look for in config file.
	string checkArray[23] = { "Sensitivity", "AttackTimeThrottle", "ReleaseTimeThrottle", "AttackTimeBreak", "ReleaseTimeBreak", "AttackTimeClutch", "ReleaseTimeClutch", "ThrottleKey", "BreakKey", "ClutchKey", "GearShiftUpKey", "GearShiftDownKey", "HandBrakeKey", "MouseLockKey", "MouseCenterKey", "UseMouse","UseCenterReduction" , "AccelerationThrottle", "AccelerationBreak", "AccelerationClutch", "CenterMultiplier", "UseForceFeedback", "UseWheelAsShifter" };
	fR.newFile(configFileName, checkArray);//read configFileName and look for checkArray
	for (int i = 7; i <= 14; i++)
		if ((int)fR.result(i) == 17) {
			printf("Using global keyboard hook to disable Assetto Corsa keyboard shortcuts\n");
			SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHook, wc.hInstance, 0);
			break;
		}
	//Printing basic menu with assigned values
	printf("==================================\n");
	printf("Sensitivity = %.2f \n", fR.result(0));
	printf("Throttle Attack Time = %.0f \n", fR.result(1));
	printf("Throttle Release Time = %.0f \n", fR.result(2));
	printf("Break Attack Time = %.0f \n", fR.result(3));
	printf("Break Release Time = %.0f \n", fR.result(4));
	printf("Clutch Attack Time = %.0f \n", fR.result(5));
	printf("Clutch Release Time = %.0f \n", fR.result(6));
	printf("Throttle key = %d \n", (int)fR.result(7));
	printf("Break key = %d \n", (int)fR.result(8));
	printf("Clutch key = %d \n", (int)fR.result(9));
	printf("Gear Shift Up key = %d \n", (int)fR.result(10));
	printf("Gear Shift Down key = %d \n", (int)fR.result(11));
	printf("Hand Brake Key = %d \n", (int)fR.result(12));
	printf("Mouse Lock key = %d \n", (int)fR.result(13));
	printf("Mouse Center key = %d \n", (int)fR.result(14));
	printf("Use Mouse = %d \n", (int)fR.result(15));
	printf("Use Center Reduction = %d \n", (int)fR.result(16));
	printf("Use Force Feedback = %d \n", (int)fR.result(21));
	printf("Use Mouse Wheel As Shifter = %d \n", (int)fR.result(22));
	printf("Acceleration Throttle = %.2f \n", fR.result(17));
	printf("Acceleration Break = %.2f \n", fR.result(18));
	printf("Acceleration Clutch = %.2f \n", fR.result(19));
	printf("Center Multiplier = %.2f \n", fR.result(20));
	printf("==================================\n");
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}
//Code that is run every time program gets an message from enviroment(mouse movement, mouse click etc.), manages input logic and feeding device.
//Update code is sleeping for 1 miliseconds to make is less cpu demanding
void updateCode() {
	Sleep(10);
	if (fFB.getFfbSize().getEffectType() == "Constant") {
		if (fFB.getFfbSize().getDirection() > 100)
			ffbStrength = (int)((fFB.getFfbSize().getMagnitude()) * (sw.elapsedMilliseconds() * 0.001));
		else
			ffbStrength = (int)(-(fFB.getFfbSize().getMagnitude()) * (sw.elapsedMilliseconds() * 0.001));
	}
	if (fFB.getFfbSize().getEffectType() == "Period")
		ffbStrength = (int)((fFB.getFfbSize().getOffset() * 0.5) * (sw.elapsedMilliseconds() * 0.001));
	if (fR.result(21) == 1) {
		axisX = axisX + ffbStrength;
		ffbStrength = 0;
	}
	mTV.inputLogic(rInput, axisX, axisY, axisZ, axisRX, isButton1Clicked, isButton2Clicked, isButton3Clicked, fR.result(1), fR.result(2), fR.result(3), fR.result(4), fR.result(5), fR.result(6), (int)fR.result(7), (int)fR.result(8), (int)fR.result(9), (int)fR.result(10), (int)fR.result(11), (int)fR.result(12), (int)fR.result(13), (int)fR.result(14), (int)fR.result(15), fR.result(17), fR.result(18), fR.result(19), (int)fR.result(22), sw.elapsedMilliseconds(), wc.hInstance);
	vJ.feedDevice(1, axisX, axisY, axisZ, axisRX, isButton1Clicked, isButton2Clicked, isButton3Clicked);
	isButton1Clicked = false;
	isButton2Clicked = false;
}
//Creates callback on window, registers raw input devices and processes mouse and keyboard input
LRESULT CALLBACK WndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_CREATE:
		//Creating new raw input devices
			RAWINPUTDEVICE m_Rid[2];
			//Keyboard
			m_Rid[0].usUsagePage = 1;
			m_Rid[0].usUsage = 6;
			m_Rid[0].dwFlags = RIDEV_INPUTSINK;
			m_Rid[0].hwndTarget = hwnd;
			//Mouse
			m_Rid[1].usUsagePage = 1;
			m_Rid[1].usUsage = 2;
			m_Rid[1].dwFlags = RIDEV_INPUTSINK;
			m_Rid[1].hwndTarget = hwnd;
			RegisterRawInputDevices(m_Rid, 2, sizeof(RAWINPUTDEVICE));

		break;
	case WM_INPUT:
		//When window recives input message get data for rinput device and run mouse logic function.
			rInput.getData(lParam);
			if (rInput.isMouseWheelUp())isButton1Clicked = true;
			if (rInput.isMouseWheelDown())isButton2Clicked = true;
			mTV.mouseLogic(rInput, axisX, fR.result(0), fR.result(20), (int)fR.result(16), isButton1Clicked, isButton2Clicked, (int)fR.result(22));
		break;
	case WM_CLOSE:
		PostQuitMessage(0);
	case WM_DESTROY:
		DestroyWindow(hwnd);
	default:
		return DefWindowProc(hwnd, Msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	//invisible window initialization to be able to recive raw input even if the window is not focused.
	static const char* class_name = "DUMMY_CLASS";
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WndProc;        // function which will handle messages
	wc.hInstance = hInstance;
	wc.lpszClassName = class_name;
	if (RegisterClassEx(&wc))
		CreateWindowEx(0, class_name, "dummy_name", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

	//Allocating console to process and redirect every stdout, stdin to it.
	string cmdLine = lpCmdLine;
	if (cmdLine != "-noconsole") {
		AllocConsole();
		#pragma warning(disable:4996)
		freopen("CONOUT$", "w", stdout);
		freopen("CONIN$", "r", stdin);
		ios::sync_with_stdio();
	}
	//Show invisible window, update it, then do initialization code.
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	initializationCode();
	

	//Loop on PeekMessage instead of GetMessage to avoid overflow.
	while (true) {
		sw.stop();
		sw.start();
		while (PeekMessage(&msgWindow, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msgWindow);
			DispatchMessage(&msgWindow);
		}
		//To optimalize cpu usade wait 1 milisecond before running update code.
		updateCode();
		//If Message is equal to quit or destroy, break loop and end program.
		if (msgWindow.message == WM_QUIT || msgWindow.message == WM_DESTROY)
			break;
	}
		
}


