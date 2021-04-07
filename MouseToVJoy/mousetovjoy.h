#pragma once
#ifndef MOUSETOVJOY_H
#define MOUSETOVJOY_H
#include "input.h"
#include "vjoy.h"
#include "stopwatch.h"
/* Basic funtion that gets data, then processes it and modifies inputs.*/

class MouseToVjoy {
public:
	//Function responsible for getting and modifying vars for throttle, break, clutch.
	void MouseToVjoy::inputLogic(CInputDevices input, INT &axisX, INT &axisY, INT &axisZ, INT &axisRX, BOOL &isButton1Clicked, BOOL &isButton2Clicked, BOOL &isButton3Clicked, DOUBLE attackTimeThrottle, DOUBLE releaseTimeThrottle, DOUBLE attackTimeBreak, DOUBLE releaseTimeBreak, DOUBLE attackTimeClutch, DOUBLE releaseTimeClutch, INT throttleKey, INT breakKey, INT clutchKey, INT gearShiftUpKey, INT gearShiftDownKey, INT handBrakeKey, INT mouseLockKey, INT mouseCenterKey, INT useMouse, DOUBLE accelerationThrottle, DOUBLE accelerationBreak, DOUBLE accelerationClutch, BOOL useWheelAsShifter, BOOL touchpad, DOUBLE deltaTime, BOOL requireLocked, BOOL &_isCursorLocked, HINSTANCE hInstance);
	//Function responsible for getting and modifying vars for steering wheel.
	void MouseToVjoy::mouseLogic(CInputDevices input, INT &axisX, DOUBLE sensitivity, DOUBLE sensitivityCenterReduction, INT useCenterReduction, BOOL &isButton1Clicked, BOOL &isButton2Clicked, INT useWheelAsShifter);
private:
	//If cursor lock key was down last time
	bool lastDown;
	//**Gets if the Center Reduction is used, when used function mouselogic() uses linear algorithm to calculate sensitivity, the more the joystick is centered the sloweer the sensitivity(max 1.0 of the sensitivity.), the further the faster the sensitivity(max = CenterReduction value * Sensitivity )
	//bool _useCenterReduction;
	//define center multiplier to be able to modify it during function.
	DOUBLE _centerMultiplier;
	//Original cursor
	HCURSOR origCursor;
	//Original cursor position
	POINT cursorPos;
};
#endif