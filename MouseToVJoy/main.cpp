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
#include <vector>
#include <unordered_map>
#include <hidsdi.h>
#include <hidusage.h>
#include <optional>

#define HID_USAGE_DIGITIZER_CONTACT_ID 0x51
#define HID_USAGE_DIGITIZER_CONTACT_COUNT 0x54

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
INT axisX, axisY, axisZ, axisRX, gear, pgear, ffbStrength; //Local variables that stores all axis values and forcefeedback strength we need.
BOOL isButton1Clicked, isButton2Clicked, isButton3Clicked, lisButton1Clicked, lisButton2Clicked, ctrlDown, touchpad; //Bools that stores information if button was pressed.
RECT bounds = { -1, -1, -1, -1 };
double xpmul, ypmul;
//Gets if the Cursor is locked then, sets cursor in cords 0,0 every input.
BOOL isCursorLocked;

// Contact information parsed from the HID report descriptor.
struct contact_info
{
	USHORT link;
};

// The data for a touch event.
struct contact
{
	contact_info info;
	ULONG id;
	POINT point;
};

// Wrapper for malloc with unique_ptr semantics, to allow
// for variable-sized structures.
struct free_deleter { void operator()(void* ptr) { free(ptr); } };
template<typename T> using malloc_ptr = std::unique_ptr<T, free_deleter>;

// Device information, such as touch area bounds and HID offsets.
// This can be reused across HID events, so we only have to parse
// this info once.
struct device_info
{
	malloc_ptr<_HIDP_PREPARSED_DATA> preparsedData; // HID internal data
	USHORT linkContactCount = 0; // Link collection for number of contacts present
	std::vector<contact_info> contactInfo; // Link collection and touch area for each contact
};

// Caches per-device info for better performance
static std::unordered_map<HANDLE, device_info> g_devices;

// Holds the current primary touch point ID
static thread_local ULONG t_primaryContactID;

// Allocates a malloc_ptr with the given size. The size must be
// greater than or equal to sizeof(T).
template<typename T>
static malloc_ptr<T>
make_malloc(size_t size)
{
	T* ptr = (T*)malloc(size);
	if (ptr == nullptr) {
		throw std::bad_alloc();
	}
	return malloc_ptr<T>(ptr);
}

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
        if (((int)fR.result(31) != 0 && isCursorLocked) || (int)fR.result(31) == 0) {
            PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)(lParam);
            USHORT vkCode = MapVirtualKey(p->scanCode, MAPVK_VSC_TO_VK);
            if (vkCode == 17 && p->dwExtraInfo != 6969) {
                ctrlDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
                for (int i = 7; i <= 14; i++) {
                    if ((rInput.isAlphabeticKeyDown((int)fR.result(i)) && (int)fR.result(i) != 17)) {
                        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                            rInput._isKeyboardButtonPressed[17] = true;
                            return 1;
                        }
                        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                            rInput._isKeyboardButtonPressed[17] = false;
                            return 1;
                        }
                        break;
                    }
                }
            }
            for (int i = 7; i <= 14; i++)
                if ((int)fR.result(i) == vkCode && (int)fR.result(i) != 17 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && ctrlDown) {
                    // Set the CTRL key to up and set dwExtraInfo to le funni number so we can ignore it
                    keybd_event(17, 0, KEYEVENTF_KEYUP, 6969);
                    break;
                }
            break;
        }
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}
BOOL exitHandler(DWORD event) {
	if (event == CTRL_CLOSE_EVENT) {
		mTV.inputLogic(rInput, axisX, axisY, axisZ, axisRX, isButton1Clicked, isButton2Clicked, isButton3Clicked, fR.result(1), fR.result(2), fR.result(3), fR.result(4), fR.result(5), fR.result(6), (int)fR.result(7), (int)fR.result(8), (int)fR.result(9), (int)fR.result(10), (int)fR.result(11), (int)fR.result(12), (int)fR.result(13), (int)fR.result(14), (int)fR.result(15), fR.result(17), fR.result(18), fR.result(19), (int)fR.result(22), touchpad, sw.elapsedMilliseconds(), (int)fR.result(31), isCursorLocked, NULL);
		return true;
	}
	return false;
}
// Reads the raw input header for the given raw input handle.
static RAWINPUTHEADER GetRawInputHeader(HRAWINPUT hInput)
{
    RAWINPUTHEADER hdr;
    UINT size = sizeof(hdr);
    if (GetRawInputData(hInput, RID_HEADER, &hdr, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
        throw;
    }
    return hdr;
}

// Reads the raw input data for the given raw input handle.
static malloc_ptr<RAWINPUT> GetRawInput(HRAWINPUT hInput, RAWINPUTHEADER hdr)
{
    malloc_ptr<RAWINPUT> input = make_malloc<RAWINPUT>(hdr.dwSize);
    UINT size = hdr.dwSize;
    if (GetRawInputData(hInput, RID_INPUT, input.get(), &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
        throw;
    }
    return input;
}

// Gets info about a raw input device.
static RID_DEVICE_INFO GetRawInputDeviceInfo(HANDLE hDevice)
{
    RID_DEVICE_INFO info;
    info.cbSize = sizeof(RID_DEVICE_INFO);
    UINT size = sizeof(RID_DEVICE_INFO);
    if (GetRawInputDeviceInfoW(hDevice, RIDI_DEVICEINFO, &info, &size) == (UINT)-1) {
        throw;
    }
    return info;
}

// Reads the preparsed HID report descriptor for the device
// that generated the given raw input.
static malloc_ptr<_HIDP_PREPARSED_DATA> GetHidPreparsedData(HANDLE hDevice)
{
    UINT size = 0;
    if (GetRawInputDeviceInfoW(hDevice, RIDI_PREPARSEDDATA, nullptr, &size) == (UINT)-1) {
        throw;
    }
    malloc_ptr<_HIDP_PREPARSED_DATA> preparsedData = make_malloc<_HIDP_PREPARSED_DATA>(size);
    if (GetRawInputDeviceInfoW(hDevice, RIDI_PREPARSEDDATA, preparsedData.get(), &size) == (UINT)-1) {
        throw;
    }
    return preparsedData;
}

// Returns all input button caps for the given preparsed
// HID report descriptor.
static std::vector<HIDP_BUTTON_CAPS> GetHidInputButtonCaps(PHIDP_PREPARSED_DATA preparsedData)
{
    NTSTATUS status;
    HIDP_CAPS caps;
    status = HidP_GetCaps(preparsedData, &caps);
    if (status != HIDP_STATUS_SUCCESS) {
        throw;
    }
    USHORT numCaps = caps.NumberInputButtonCaps;
    std::vector<HIDP_BUTTON_CAPS> buttonCaps(numCaps);
    status = HidP_GetButtonCaps(HidP_Input, &buttonCaps[0], &numCaps, preparsedData);
    if (status != HIDP_STATUS_SUCCESS) {
        throw;
    }
    buttonCaps.resize(numCaps);
    return buttonCaps;
}

// Returns all input value caps for the given preparsed
// HID report descriptor.
static std::vector<HIDP_VALUE_CAPS> GetHidInputValueCaps(PHIDP_PREPARSED_DATA preparsedData)
{
    NTSTATUS status;
    HIDP_CAPS caps;
    status = HidP_GetCaps(preparsedData, &caps);
    if (status != HIDP_STATUS_SUCCESS) {
        throw;
    }
    USHORT numCaps = caps.NumberInputValueCaps;
    std::vector<HIDP_VALUE_CAPS> valueCaps(numCaps);
    status = HidP_GetValueCaps(HidP_Input, &valueCaps[0], &numCaps, preparsedData);
    if (status != HIDP_STATUS_SUCCESS) {
        throw;
    }
    valueCaps.resize(numCaps);
    return valueCaps;
}

// Reads the pressed status of a single HID report button.
static bool GetHidUsageButton(
    HIDP_REPORT_TYPE reportType,
    USAGE usagePage,
    USHORT linkCollection,
    USAGE usage,
    PHIDP_PREPARSED_DATA preparsedData,
    PBYTE report,
    ULONG reportLen)
{
    ULONG numUsages = HidP_MaxUsageListLength(
        reportType,
        usagePage,
        preparsedData);
    std::vector<USAGE> usages(numUsages);
    NTSTATUS status = HidP_GetUsages(
        reportType,
        usagePage,
        linkCollection,
        &usages[0],
        &numUsages,
        preparsedData,
        (PCHAR)report,
        reportLen);
    if (status != HIDP_STATUS_SUCCESS) {
        throw;
    }
    usages.resize(numUsages);
    return std::find(usages.begin(), usages.end(), usage) != usages.end();
}

// Reads a single HID report value in logical units.
static ULONG GetHidUsageLogicalValue(
    HIDP_REPORT_TYPE reportType,
    USAGE usagePage,
    USHORT linkCollection,
    USAGE usage,
    PHIDP_PREPARSED_DATA preparsedData,
    PBYTE report,
    ULONG reportLen)
{
    ULONG value;
    NTSTATUS status = HidP_GetUsageValue(
        reportType,
        usagePage,
        linkCollection,
        usage,
        &value,
        preparsedData,
        (PCHAR)report,
        reportLen);
    if (status != HIDP_STATUS_SUCCESS) {
        throw;
    }
    return value;
}

// Reads a single HID report value in physical units.
static LONG GetHidUsagePhysicalValue(
    HIDP_REPORT_TYPE reportType,
    USAGE usagePage,
    USHORT linkCollection,
    USAGE usage,
    PHIDP_PREPARSED_DATA preparsedData,
    PBYTE report,
    ULONG reportLen)
{
    LONG value;
    NTSTATUS status = HidP_GetScaledUsageValue(
        reportType,
        usagePage,
        linkCollection,
        usage,
        &value,
        preparsedData,
        (PCHAR)report,
        reportLen);
    if (status != HIDP_STATUS_SUCCESS) {
        return -1;
    }
    return value;
}

// Gets the device info associated with the given raw input. Uses the
// cached info if available; otherwise parses the HID report descriptor
// and stores it into the cache.
static device_info& GetDeviceInfo(HANDLE hDevice)
{
    if (g_devices.count(hDevice)) {
        return g_devices.at(hDevice);
    }

    device_info dev;
    std::optional<USHORT> linkContactCount;
    dev.preparsedData = GetHidPreparsedData(hDevice);

    // Struct to hold our parser state
    struct contact_info_tmp
    {
        bool hasContactID = false;
        bool hasTip = false;
        bool hasX = false;
        bool hasY = false;
    };
    std::unordered_map<USHORT, contact_info_tmp> contacts;

    // Get the touch area for all the contacts. Also make sure that each one
    // is actually a contact, as specified by:
    // https://docs.microsoft.com/en-us/windows-hardware/design/component-guidelines/windows-precision-touchpad-required-hid-top-level-collections
    for (const HIDP_VALUE_CAPS& cap : GetHidInputValueCaps(dev.preparsedData.get())) {
        if (cap.IsRange || !cap.IsAbsolute) {
            continue;
        }

        if (cap.UsagePage == HID_USAGE_PAGE_GENERIC) {
            if (cap.NotRange.Usage == HID_USAGE_GENERIC_X) {
                contacts[cap.LinkCollection].hasX = true;
            }
            else if (cap.NotRange.Usage == HID_USAGE_GENERIC_Y) {
                contacts[cap.LinkCollection].hasY = true;
            }
        }
        else if (cap.UsagePage == HID_USAGE_PAGE_DIGITIZER) {
            if (cap.NotRange.Usage == HID_USAGE_DIGITIZER_CONTACT_COUNT) {
                linkContactCount = cap.LinkCollection;
            }
            else if (cap.NotRange.Usage == HID_USAGE_DIGITIZER_CONTACT_ID) {
                contacts[cap.LinkCollection].hasContactID = true;
            }
        }
    }

    for (const HIDP_BUTTON_CAPS& cap : GetHidInputButtonCaps(dev.preparsedData.get())) {
        if (cap.UsagePage == HID_USAGE_PAGE_DIGITIZER) {
            if (cap.NotRange.Usage == HID_USAGE_DIGITIZER_TIP_SWITCH) {
                contacts[cap.LinkCollection].hasTip = true;
            }
        }
    }

    if (!linkContactCount.has_value()) {
        throw std::runtime_error("No contact count usage found");
    }
    dev.linkContactCount = linkContactCount.value();

    for (const auto& kvp : contacts) {
        USHORT link = kvp.first;
        const contact_info_tmp& info = kvp.second;
        if (info.hasContactID && info.hasTip && info.hasX && info.hasY) {
            dev.contactInfo.push_back({ link });
        }
    }

    return g_devices[hDevice] = std::move(dev);
}

// Reads all touch contact points from a raw input event.
static std::vector<contact> GetContacts(device_info& dev, RAWINPUT* input)
{
    std::vector<contact> contacts;

    DWORD sizeHid = input->data.hid.dwSizeHid;
    DWORD count = input->data.hid.dwCount;
    BYTE* rawData = input->data.hid.bRawData;
    if (count == 0) {
        return contacts;
    }

    ULONG numContacts = GetHidUsageLogicalValue(
        HidP_Input,
        HID_USAGE_PAGE_DIGITIZER,
        dev.linkContactCount,
        HID_USAGE_DIGITIZER_CONTACT_COUNT,
        dev.preparsedData.get(),
        rawData,
        sizeHid);

    if (numContacts > dev.contactInfo.size()) {
        numContacts = (ULONG)dev.contactInfo.size();
    }

    // It's a little ambiguous as to whether contact count includes
    // released contacts. I interpreted the specs as a yes, but this
    // may require additional testing.
    for (ULONG i = 0; i < numContacts; ++i) {
        contact_info& info = dev.contactInfo[i];
        bool tip = GetHidUsageButton(
            HidP_Input,
            HID_USAGE_PAGE_DIGITIZER,
            info.link,
            HID_USAGE_DIGITIZER_TIP_SWITCH,
            dev.preparsedData.get(),
            rawData,
            sizeHid);

        if (!tip) {
            continue;
        }

        ULONG id = GetHidUsageLogicalValue(
            HidP_Input,
            HID_USAGE_PAGE_DIGITIZER,
            info.link,
            HID_USAGE_DIGITIZER_CONTACT_ID,
            dev.preparsedData.get(),
            rawData,
            sizeHid);

        LONG x = GetHidUsagePhysicalValue(
            HidP_Input,
            HID_USAGE_PAGE_GENERIC,
            info.link,
            HID_USAGE_GENERIC_X,
            dev.preparsedData.get(),
            rawData,
            sizeHid);

        LONG y = GetHidUsagePhysicalValue(
            HidP_Input,
            HID_USAGE_PAGE_GENERIC,
            info.link,
            HID_USAGE_GENERIC_Y,
            dev.preparsedData.get(),
            rawData,
            sizeHid);

        if (x != -1 && y != -1)
            contacts.push_back({ info, id, { x, y } });
    }

    return contacts;
}

// Returns the primary contact for a given list of contacts. This is
// necessary since we are mapping potentially many touches to a single
// mouse position. Currently this just stores a global contact ID and
// uses that as the primary contact.
static contact GetPrimaryContact(const std::vector<contact>& contacts)
{
    for (const contact& contact : contacts) {
        if (contact.id == t_primaryContactID) {
            return contact;
        }
    }
    t_primaryContactID = contacts[0].id;
    return contacts[0];
}
BOOL HasPrecisionTouchpad() {
    std::vector<RAWINPUTDEVICELIST> devices(64);

    while (true) {
        UINT numDevices = (UINT)devices.size();
        UINT ret = GetRawInputDeviceList(&devices[0], &numDevices, sizeof(RAWINPUTDEVICELIST));
        if (ret != (UINT)-1) {
            devices.resize(ret);
            break;
        }
        else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            devices.resize(numDevices);
        }
        else {
            return false;
        }
    }

    for (RAWINPUTDEVICELIST dev : devices) {
        RID_DEVICE_INFO info;
        info.cbSize = sizeof(RID_DEVICE_INFO);
        UINT size = sizeof(RID_DEVICE_INFO);
        if (GetRawInputDeviceInfoW(dev.hDevice, RIDI_DEVICEINFO, &info, &size) == (UINT)-1) {
            continue;
        }
        if (info.dwType == RIM_TYPEHID &&
            info.hid.usUsagePage == HID_USAGE_PAGE_DIGITIZER &&
            info.hid.usUsage == HID_USAGE_DIGITIZER_TOUCH_PAD) {
            device_info& info = GetDeviceInfo(dev.hDevice);
            if (!info.contactInfo.empty()) {
                return true;
            }
            else
                break;
        }
    }
    return false;
}
void WriteCalibration() {
    std::ofstream calib;
    calib.open("tpcalib.dat");
    calib << bounds.left << std::endl;
    calib << bounds.right << std::endl;
    calib << bounds.top << std::endl;
    calib << bounds.bottom << std::endl;
    calib.close();
}

void ReadCalibration() {
    int i = 0;
    std::ifstream input("tpcalib.dat");
    if (input.good()) {
        for (std::string line; std::getline(input, line); )
        {
            switch (i) {
            case 0:
                bounds.left = std::stol(line.c_str());
                break;
            case 1:
                bounds.right = std::stol(line.c_str());
                break;
            case 2:
                bounds.top = std::stol(line.c_str());
                break;
            case 3:
                bounds.bottom = std::stol(line.c_str());
                break;
            }
            i++;
        }
        printf("Loaded calibration %d %d %d %d\n", bounds.left, bounds.right, bounds.top, bounds.bottom);
    }
    else {
        printf("Calibrate touchpad by touching each corner\n");
    }
}

void HandleCalibration(LONG x, LONG y) {
    if (x < bounds.left || bounds.left == -1) {
        bounds.left = x;
        WriteCalibration();
    }
    if (x > bounds.right || bounds.right == -1) {
        bounds.right = x;
        WriteCalibration();
    }
    if (y < bounds.top || bounds.top == -1) {
        bounds.top = y;
        WriteCalibration();
    }
    if (y > bounds.bottom || bounds.bottom == -1) {
        bounds.bottom = y;
        WriteCalibration();
    }
}
//Code that is run once application is initialized, test virtual joystick and accuires it, also it reads config.txt file and prints out menu and variables.
void initializationCode() {
	//Code that is run only once, tests vjoy device, reads config file and prints basic out accuired vars.
	UINT DEV_ID = 1;
	vJ.testDriver();//Test if driver is installed and compatible.
	vJ.testVirtualDevices(DEV_ID);//Test if virtually created joystick is up and running.
	vJ.accuireDevice(DEV_ID);//Accuire virtual joystick of index number DEV_ID
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)(exitHandler), TRUE);//Set the exit handler
	string configFileName = "config.txt";
	//what strings to look for in config file.
	string checkArray[33] = { "Sensitivity", "AttackTimeThrottle", "ReleaseTimeThrottle", "AttackTimeBreak", "ReleaseTimeBreak", "AttackTimeClutch", "ReleaseTimeClutch", "ThrottleKey", "BreakKey", "ClutchKey", "GearShiftUpKey", "GearShiftDownKey", "HandBrakeKey", "MouseLockKey", "MouseCenterKey", "UseMouse","UseCenterReduction" , "AccelerationThrottle", "AccelerationBreak", "AccelerationClutch", "CenterMultiplier", "UseForceFeedback", "UseWheelAsShifter", "UseWheelAsThrottle", "Touchpad", "TouchpadXInvert", "TouchpadYInvert", "TouchpadXPercent", "TouchpadYPercent", "TouchpadXStartPercent", "TouchpadYStartPercent", "RequireLocked", "ForceFeedbackKey"};
	fR.newFile(configFileName, checkArray);//read configFileName and look for checkArray

    if ((int)fR.result(21)) {
        vJ.enableFFB(DEV_ID);//Enable virtual joystick of index number DEV_ID to accept forcefeedback calls
        FfbRegisterGenCB(FFBCALLBACK, &DEV_ID);//Registed what function to run on forcefeedback call.
    }
    for (int i = 7; i <= 14; i++)
		if ((int)fR.result(i) == 17) {
			printf("Using global keyboard hook to disable Assetto Corsa keyboard shortcuts\n");
			SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHook, wc.hInstance, 0);
			break;
		}
	if (HasPrecisionTouchpad() && (int)fR.result(24)) {
		printf("Found precision touchpad, using it for axis Z and RX\n");
        ReadCalibration();
		touchpad = true;
        printf("Make sure your touchpad is set to disabled while mouse connected so it doesn't interfere with mouse input\n");
	}
	else if ((int)fR.result(24)) {
		printf("Could not find precision touchpad\n");
	}
    else {
        printf("Touchpad disabled\n");
    }
    xpmul = 1 / ((INT)fR.result(27) == 0 ? 1 : fR.result(27) / 100);
    ypmul = 1 / ((INT)fR.result(28) == 0 ? 1 : fR.result(28) / 100);
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
    printf("Required Locked = %d \n", (int)fR.result(31));
	printf("Use Mouse = %d \n", (int)fR.result(15));
	printf("Use Center Reduction = %d \n", (int)fR.result(16));
	printf("Use Force Feedback = %d \n", (int)fR.result(21));
    printf("Force Feedback Key = %d \n", (int)fR.result(32));
	printf("Use Mouse Wheel As Shifter = %d \n", (int)fR.result(22));
	printf("Use Mouse Wheel As Throttle = %d \n", (int)fR.result(23));
	printf("Acceleration Throttle = %.2f \n", fR.result(17));
	printf("Acceleration Break = %.2f \n", fR.result(18));
	printf("Acceleration Clutch = %.2f \n", fR.result(19));
	printf("Center Multiplier = %.2f \n", fR.result(20));
    printf("Touchpad X Invert = %d \n", (INT)fR.result(25));
    printf("Touchpad Y Invert = %d \n", (INT)fR.result(26));
    printf("Touchpad X Start = %d \n", (INT)fR.result(29));
    printf("Touchpad Y Start = %d \n", (INT)fR.result(30));
    printf("Touchpad X Percent = %.0f \n", (1 / xpmul) * 100);
    printf("Touchpad Y Percent = %.0f \n", (1 / ypmul) * 100);
	printf("==================================\n");
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}
//Code that is run every time program gets an message from enviroment(mouse movement, mouse click etc.), manages input logic and feeding device.
//Update code is sleeping for 1 miliseconds to make is less cpu demanding
void updateCode() {
	Sleep(1);

    if (((int)fR.result(31) != 0 && isCursorLocked) || (int)fR.result(31) == 0) {
        if (((int)fR.result(32) != 0 && rInput.isAlphabeticKeyDown((int)fR.result(32))) || (int)fR.result(32) == 0 && fR.result(21) == 1) {
            if (fFB.getFfbSize().getEffectType() == "Constant") {
                if (fFB.getFfbSize().getDirection() > 100)
                    ffbStrength = (int)((fFB.getFfbSize().getMagnitude()) * (sw.elapsedMilliseconds() * 0.001));
                else
                    ffbStrength = (int)(-(fFB.getFfbSize().getMagnitude()) * (sw.elapsedMilliseconds() * 0.001));
            }
            if (fFB.getFfbSize().getEffectType() == "Period")
                ffbStrength = (int)((fFB.getFfbSize().getOffset() * 0.5) * (sw.elapsedMilliseconds() * 0.001));

            axisX += ffbStrength;
            ffbStrength = 0;
        }
        if (rInput.isAlphabeticKeyDown((int)fR.result(10)) && !lisButton1Clicked && gear < 7) {
            lisButton1Clicked = true;
            gear++;
        }
        else
            lisButton1Clicked = rInput.isAlphabeticKeyDown((int)fR.result(10));
        if (rInput.isAlphabeticKeyDown((int)fR.result(11)) && !lisButton2Clicked && gear > -1) {
            lisButton2Clicked = true;
            gear--;
        }
        else
            lisButton2Clicked = rInput.isAlphabeticKeyDown((int)fR.result(11));
    }
    mTV.inputLogic(rInput, axisX, axisY, axisZ, axisRX, isButton1Clicked, isButton2Clicked, isButton3Clicked, fR.result(1), fR.result(2), fR.result(3), fR.result(4), fR.result(5), fR.result(6), (int)fR.result(7), (int)fR.result(8), (int)fR.result(9), (int)fR.result(10), (int)fR.result(11), (int)fR.result(12), (int)fR.result(13), (int)fR.result(14), (int)fR.result(15), fR.result(17), fR.result(18), fR.result(19), ((int)fR.result(22) && !(int)fR.result(23)) || (!(int)fR.result(22) && !(int)fR.result(23)), touchpad, sw.elapsedMilliseconds(), (int)fR.result(31), isCursorLocked, wc.hInstance);

    if (((int)fR.result(31) != 0 && isCursorLocked) || (int)fR.result(31) == 0) {
        vJ.feedDevice(1, axisX, axisY, axisZ, axisRX, gear, isButton1Clicked, isButton2Clicked, isButton3Clicked);
        isButton1Clicked = false;
        isButton2Clicked = false;
    }

}
BOOL HandleTouchpad(LPARAM* lParam) {
    if (!touchpad)
        return false;

    double _axisZ, _axisRX, x, y, ystart, xstart;
    HRAWINPUT hInput = (HRAWINPUT)*lParam;
    RAWINPUTHEADER hdr = GetRawInputHeader(hInput);
    if (hdr.dwType != RIM_TYPEHID)
        return false;
    device_info& dev = GetDeviceInfo(hdr.hDevice);
    malloc_ptr<RAWINPUT> input = GetRawInput(hInput, hdr);
    std::vector<contact> contacts = GetContacts(dev, input.get());

    if (contacts.empty()) {
        axisZ = 0;
        axisRX = 0;
        return true;
    }

    for (const contact& contact : contacts) {
        HandleCalibration(contact.point.x, contact.point.y);
    }

    axisRX = -1;
    axisZ = -1;
    ystart = (double)fR.result(29) / 100;
    xstart = (double)fR.result(30) / 100;
    for (const contact& contact : contacts) {
        x = ((double)contact.point.x - bounds.left) / ((double)bounds.right - bounds.left);
        y = ((double)contact.point.y - bounds.top) / ((double)bounds.bottom - bounds.top);

        if ((int)fR.result(25))
            _axisZ = (INT)round(((1 - x) * xpmul) * 32767);
        else
            _axisZ = (INT)round((x * xpmul) * 32767);
        if ((int)fR.result(26))
            _axisRX = (INT)round(((1 - y) * ypmul) * 32767);
        else
            _axisRX = (INT)round((y * ypmul) * 32767);

        if (_axisRX > (32767 * xstart) && axisRX == -1)
            axisRX = int((_axisRX - (32767 * xstart)) / (1 - xstart));

        if (_axisZ > (32767 * ystart) && axisZ == -1)
            axisZ = (int)((_axisZ - (32767 * ystart)) / (1 - ystart));

    }

    if (axisRX == -1)
        axisRX = 0;
    if (axisZ == -1)
        axisZ = 0;
    return true;
}
//Creates callback on window, registers raw input devices and processes mouse and keyboard input
LRESULT CALLBACK WndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_CREATE:
		//Creating new raw input devices
		RAWINPUTDEVICE m_Rid[3];
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
        //Touchpad
        m_Rid[2].usUsagePage = HID_USAGE_PAGE_DIGITIZER;
        m_Rid[2].usUsage = HID_USAGE_DIGITIZER_TOUCH_PAD;
        m_Rid[2].dwFlags = RIDEV_INPUTSINK;
        m_Rid[2].hwndTarget = hwnd;
		RegisterRawInputDevices(m_Rid, 3, sizeof(RAWINPUTDEVICE));
		break;
	case WM_INPUT:
		//When window recives input message get data for rinput device and run mouse logic function.
        if (HandleTouchpad(&lParam))
            break;
        rInput.getData(lParam, touchpad);
        if ((int)fR.result(31) != 0 && !isCursorLocked)
            break;
		if ((int)fR.result(22) && !(int)fR.result(23)) {
			if (rInput.isMouseWheelUp())isButton1Clicked = true;
			if (rInput.isMouseWheelDown())isButton2Clicked = true;
			if (rInput.isMiddleMouseButtonDown() && !pgear) {
				pgear = gear;
				gear = 0;
			}
			else if (!rInput.isMiddleMouseButtonDown() && pgear) {
				gear = pgear;
				pgear = 0;
			}
			if (rInput.isMiddleMouseButtonDown()) {
				if (rInput.isMouseWheelUp() && pgear < 7) pgear++;
				if (rInput.isMouseWheelDown() && pgear > -1) pgear--;
			}
			else {
				if (rInput.isMouseWheelUp() && gear < 7) gear++;
				if (rInput.isMouseWheelDown() && gear > -1) gear--;
			}
		}
		else if ((int)fR.result(23)) {
			if (rInput.isMouseWheelUp() && axisY <= 32767) axisY += 32767 / (int)fR.result(23);
			if (rInput.isMouseWheelDown() && axisY > 0) axisY -= 32767 / (int)fR.result(23);
		}
		mTV.mouseLogic(rInput.getMouseChangeX(), axisX, fR.result(0), fR.result(20), (int)fR.result(16));
		break;
	case WM_CLOSE:
		PostQuitMessage(0);
        break;
	case WM_DESTROY:
		DestroyWindow(hwnd);
        break;
	default:
		return DefWindowProc(hwnd, Msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
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
        #pragma warning(disable:6031)
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


