#include "mousetovjoy.h"
#include <iostream>
#include <math.h>
#define STEERING_MAX 16384
#define STEERING_MIN -16384
// Mouse disable hook
LRESULT CALLBACK mouseHook(int nCode, WPARAM wParam, LPARAM lParam)
{
	return 1;
}
//Function responsible for getting and modifying vars for throttle, break, clutch.
void MouseToVjoy::inputLogic(CInputDevices input, INT &axisX, INT &axisY, INT &axisZ, INT &axisRX, BOOL &isButton1Clicked, BOOL &isButton2Clicked, BOOL &isButton3Clicked, DOUBLE attackTimeThrottle, DOUBLE releaseTimeThrottle, DOUBLE attackTimeBreak, DOUBLE releaseTimeBreak, DOUBLE attackTimeClutch, DOUBLE releaseTimeClutch, INT throttleKey, INT breakKey, INT clutchKey, INT gearShiftUpKey, INT gearShiftDownKey, INT handBrakeKey, INT mouseLockKey, INT mouseCenterKey, INT useMouse, DOUBLE accelerationThrottle, DOUBLE accelerationBreak, DOUBLE accelerationClutch, BOOL useWheelAsShifter, BOOL touchpad, DOUBLE deltaTime, HINSTANCE hInstance) {
	if (useMouse == 1) {
		if (input.isLeftMouseButtonDown() && axisY < 32767 && useWheelAsShifter)
			axisY = (int)((axisY + (attackTimeThrottle == 0 ? 32767 : (attackTimeThrottle * deltaTime))) * accelerationThrottle);
		if (!input.isLeftMouseButtonDown() && axisY > 1 && useWheelAsShifter)
			axisY = (int)((axisY - (releaseTimeThrottle == 0 ? 32767 : (releaseTimeThrottle * deltaTime))) / accelerationThrottle);
		if (!touchpad) {
			if (input.isRightMouseButtonDown() && axisZ < 32767)
				axisZ = (int)((axisZ + (attackTimeBreak == 0 ? 32767 : (attackTimeBreak * deltaTime))) * accelerationBreak);
			if (!input.isRightMouseButtonDown() && axisZ > 1)
				axisZ = (int)((axisZ - (releaseTimeBreak == 0 ? 32767 : (releaseTimeBreak * deltaTime))) / accelerationBreak);
		}
	}
	else {
		if (input.isAlphabeticKeyDown(throttleKey) && axisY < 32767 && useWheelAsShifter)
			axisY = (int)((axisY + (attackTimeThrottle == 0 ? 32767 : (attackTimeThrottle * deltaTime))) * accelerationThrottle);
		if (!input.isAlphabeticKeyDown(throttleKey) && axisY > 1 && useWheelAsShifter)
			axisY = (int)((axisY - (releaseTimeThrottle == 0 ? 32767 : (releaseTimeThrottle * deltaTime))) / accelerationThrottle);
		if (!touchpad) {
			if (input.isAlphabeticKeyDown(breakKey) && axisZ < 32767)
				axisZ = (int)((axisZ + (attackTimeBreak == 0 ? 32767 : (attackTimeBreak * deltaTime))) * accelerationBreak);
			if (!input.isAlphabeticKeyDown(breakKey) && axisZ > 1)
				axisZ = (int)((axisZ - (releaseTimeBreak == 0 ? 32767 : (releaseTimeBreak * deltaTime))) / accelerationBreak);
		}
	}
	if (!touchpad) {
		if (input.isAlphabeticKeyDown(clutchKey) && axisRX < 32767)
			axisRX = (int)((axisRX + (attackTimeClutch == 0 ? 32767 : (attackTimeClutch * deltaTime))) * accelerationClutch);
		if (!input.isAlphabeticKeyDown(clutchKey) && axisRX > 1)
			axisRX = (int)((axisRX - (releaseTimeClutch == 0 ? 32767 : (releaseTimeClutch * deltaTime))) / accelerationClutch);
	}
	if (input.isAlphabeticKeyDown(mouseLockKey) || hInstance == NULL) {
		if (!hMouseHook && !lastDown && hInstance != NULL) {
			// Set cursor to blank but first save the current one so we can restore it.
			BYTE cura[] = { 0xFF };
			BYTE curx[] = { 0x00 };
			HCURSOR blankCursor = CreateCursor(hInstance, 0, 0, 1, 1, cura, curx);
			origCursor = CopyCursor(LoadCursor(0, IDC_ARROW));
			GetCursorPos(&cursorPos);
			SetSystemCursor(blankCursor, 32512);
			DestroyCursor(blankCursor);
			hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseHook, hInstance, 0);
		}
		else if ((!lastDown || hInstance == NULL) && hMouseHook) {
			UnhookWindowsHookEx(hMouseHook);
			SetSystemCursor(origCursor, 32512);
			SetCursorPos(cursorPos.x, cursorPos.y);
			hMouseHook = 0;
		}
		lastDown = true;
	}
	else
		lastDown = false;
	if (input.isAlphabeticKeyDown(mouseCenterKey)) {
		SleepEx(250, !(input.isAlphabeticKeyDown(mouseCenterKey)));
		axisX = (32766 / 2);
	}
	isButton1Clicked = input.isAlphabeticKeyDown(gearShiftUpKey) || isButton1Clicked;
	isButton2Clicked = input.isAlphabeticKeyDown(gearShiftDownKey) || isButton2Clicked;
	isButton3Clicked = input.isAlphabeticKeyDown(handBrakeKey);
}
//Function responsible for getting and modifying vars for steering wheel.
void MouseToVjoy::mouseLogic(CInputDevices input, INT &X, DOUBLE sensitivity, DOUBLE sensitivityCenterReduction, INT useCenterReduction, BOOL &isButton1Clicked, BOOL &isButton2Clicked, INT useWheelAsShifter){
	//vjoy max value is 0-32767 to make it easier to scale linear reduction/acceleration I subtract half of it so 16384 to make it -16384 to 16384.
	X = X - 16384;
	if (X > 0)
		_centerMultiplier = pow(sensitivityCenterReduction, (1 - (double)((double)X / (double)STEERING_MAX)));
	else if(X < 0)
		_centerMultiplier = pow(sensitivityCenterReduction, (1 - (double)((double)X / (double)STEERING_MIN)));
	if (useCenterReduction == 1)
		X = (int)(X + ((input.getMouseChangeX() * sensitivity) / _centerMultiplier));
	else
		X = (int)(X + (input.getMouseChangeX() * sensitivity));

	if (X > 16384)
		X = 16384;
	else if (X < -16384)
		X = -16384;
	X += 16384;
};