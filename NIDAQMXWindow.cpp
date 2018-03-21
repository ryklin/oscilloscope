// NIDAQMXWindow.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "NIDAQMXWindow.h"
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;

#include "C:\\Program Files (x86)\\National Instruments\\Shared\\ExternalCompilerSupport\\C\\include\\NIDAQmx.h"
#pragma comment( lib, "C:\\Program Files (x86)\\National Instruments\\Shared\\ExternalCompilerSupport\\C\\lib32\\msvc\\NIDAQmx.lib" ) 	// notice this is 32 bit only

TaskHandle taskHandle = 0;
const int arraySizeInSamps = 4;
float64     readArray[arraySizeInSamps]; //memset(readArray, 0, arraySizeInSamps * sizeof(float64));

float widthWindow = 1800;
float heightWindow = 1200;
HDC hdcBackGround = NULL;
HBITMAP screenMain = NULL; 

#define BUFFER_SIZE 1600

float pixX[BUFFER_SIZE];
float pixY[BUFFER_SIZE];
int pixIndex = 0;

void InitDAQ() {

	int32 status = 0;
	char errorString[MAX_PATH];

	/*********************************************/
	// DAQmx Configure Code
	/*********************************************/
	status = DAQmxCreateTask("", &taskHandle);

	if (status != 0) {
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		OutputDebugStringA(errorString);
		return;
	}

	/* terminalConfig Options:
	DAQmx_Val_Cfg_Default
	DAQmx_Val_RSE
	DAQmx_Val_NRSE
	DAQmx_Val_Diff
	DAQmx_Val_PseudoDiff
	*/
	int32 terminalConfig = DAQmx_Val_RSE;

	int32 units = DAQmx_Val_Volts;
	char nameToAssignToChannel[] = "Dev1/ai0:3";
	status = DAQmxCreateAIVoltageChan(taskHandle, nameToAssignToChannel, "", terminalConfig, -10.0, 10.0, units, NULL);
	if (status != 0) {
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		OutputDebugStringA(errorString);
		return;
	}
	/* activeEdge Options:
	DAQmx_Val_Rising // Acquire or generate samples on the rising edges of the Sample Clock.
	DAQmx_Val_Falling // Acquire or generate samples on the falling edges of the Sample Clock.
	*/
	int32 activeEdge = DAQmx_Val_Rising;

	/* sampleMode Options:
	DAQmx_Val_FiniteSamps //Acquire or generate a finite number of samples.
	DAQmx_Val_ContSamps // Acquire or generate samples until you stop the task.
	DAQmx_Val_HWTimedSinglePoint //Acquire or generate samples continuously using hardware timing without a buffer. Hardware timed single point sample mode is supported only for the sample clock and change detection timing types. (http://zone.ni.com/reference/en-XX/help/370466AC-01/mxcncpts/hwtspsamplemode/)
	*/
	int32 sampleMode = DAQmx_Val_HWTimedSinglePoint;

	/*
	One of the most important parameters of an analog input or output system is the rate at which the measurement device samples an incoming signal or generates the output signal.
	The sampling rate, which is called the scan rate in Traditional NI-DAQ (Legacy), is the speed at which a device acquires or generates a sample on each channel.
	A fast input sampling rate acquires more points in a given time and can form a better representation of the original signal than a slow sampling rate.
	Generating a 1 Hz signal using 1000 points per cycle at 1000 S/s produces a much finer representation than using 10 points per cycle at a sample rate of 10 S/s.
	Sampling too slowly results in a poor representation of the analog signal. Undersampling causes the signal to appear as if it has a different frequency than it actually does.
	This misrepresentation of a signal is called aliasing.
	*/

	float64 sampleRate = 1000; //The sampling rate in samples per second per channel. If you use an external source for the Sample Clock, set this value to the maximum expected rate of that clock.

	/*
	The number of samples to acquire or generate for each channel in the task if sampleMode is DAQmx_Val_FiniteSamps.
	If sampleMode is DAQmx_Val_ContSamps, NIDAQmx uses this value to determine the buffer size.
	*/
	uInt64 sampsPerChanToAcquire = 4;

	/* DAQmxCfgSampClkTiming
	Sets the source of the Sample Clock, the rate of the Sample Clock, and the number of samples to acquire or generate.
	*/
	status = DAQmxCfgSampClkTiming(taskHandle, "", sampleRate, activeEdge, sampleMode, sampsPerChanToAcquire);
	if (status != 0) {
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		OutputDebugStringA(errorString);
		return;
	}

	/*********************************************/
	// start the data acquisition
	/*********************************************/
	status = DAQmxStartTask(taskHandle);
	if (status != 0) {
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		OutputDebugStringA(errorString);
		return;
	}

}

void daqRead() {
	/*********************************************/
	// DAQmx Read Code
	/*********************************************/
	const int numSampsPerChan = 1;

	int32 sampsPerChanRead = -1;
	float64 timeOut = 0;

	stringstream message;

	int32 status = DAQmxReadAnalogF64(taskHandle, numSampsPerChan, timeOut, DAQmx_Val_GroupByChannel, readArray, arraySizeInSamps, &sampsPerChanRead, NULL);

	if (status != 0) {
		char errorString[MAX_PATH];
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		message << errorString;
	}
	//if (DAQmxErrorSamplesNotYetAvailable == status || DAQmxErrorSamplesNoLongerAvailable == status) {
	//	message << "status:" << status << " continue" << std::endl;
	//	OutputDebugStringA(message.str().c_str());

	//}
	//else if (status < 0) {
	//	message << "status:" << status << "breaking" << std::endl;
	//}
	else {
		message << std::fixed << std::setprecision(2);
		pixIndex = (pixIndex + 1) % BUFFER_SIZE;
		message << pixIndex << " ";

		for (int i = 0; i < arraySizeInSamps; i++) {
			message << readArray[i] << ", ";
		}

		pixX[pixIndex] = readArray[0];
		pixY[pixIndex] = readArray[1];

	}
	message << endl;
	OutputDebugStringA(message.str().c_str());
}

void StopDAQ() {
	DAQmxStopTask(taskHandle);
	DAQmxClearTask(taskHandle);
}

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_NIDAQMXWINDOW, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NIDAQMXWINDOW));


    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NIDAQMXWINDOW));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_NIDAQMXWINDOW);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      0, 0, widthWindow, heightWindow, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HDC hdcBack;
	HDC hdcTemp;
    switch (message)
    {
	case WM_CREATE:
		SetTimer(hWnd, WM_TIMER, 0, (TIMERPROC)NULL);
		InitDAQ();

		break;
	case WM_SIZE:
		widthWindow = LOWORD(lParam);
		heightWindow = HIWORD(lParam);

		hdcTemp = GetDC(hWnd);

		// create DC for back buffer
		if (hdcBack) {
			DeleteDC(hdcBack); hdcBack = NULL;
		}
		hdcBack = CreateCompatibleDC(hdcTemp);

		// create DC for the background
		if (hdcBackGround) {
			DeleteDC(hdcBackGround); hdcBackGround = NULL;
		}
		hdcBackGround = CreateCompatibleDC(hdcTemp);

		if (screenMain) {
			DeleteObject(screenMain); screenMain = NULL;
		}
		screenMain = CreateCompatibleBitmap(hdcTemp, widthWindow, heightWindow);  // must be created with hdcTemp, same that is used to create hdcBack

		SelectObject(hdcBack, screenMain);

		// set the text drawing properties into the DC
		SetTextColor(hdcBack, RGB(255, 255, 255));
		SetBkMode(hdcBack, TRANSPARENT);

		// finished creating everything, release temporary DC
		ReleaseDC(hWnd, hdcTemp);
		break;
	case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
	case WM_ERASEBKGND:                // APPENDED FLICKER FREE
		return TRUE;
	case WM_TIMER:
		daqRead();
		{
			HDC hdc = GetDC(hWnd);

			HBRUSH backgroundBrush = CreateSolidBrush(RGB(128, 128, 128));
			RECT rect = { 0,0,widthWindow, heightWindow};
			FillRect(hdcBack, &rect, backgroundBrush);
			DeleteObject(backgroundBrush);

			int xP = pixIndex;

			float y = (pixX[xP] + 10) / 20 * heightWindow;
			MoveToEx(hdcBack, 0, y, NULL);
			SelectObject(hdcBack, GetStockObject(WHITE_PEN));
			for (int x = 0; x < BUFFER_SIZE; x++) {
				xP = (xP + 1) % BUFFER_SIZE;
				y = (pixX[xP] + 10) / 20 * heightWindow;
				LineTo(hdcBack, x/(float) BUFFER_SIZE*widthWindow, y);
				MoveToEx(hdcBack, x / (float)BUFFER_SIZE*widthWindow, y, NULL);
			}


			xP = pixIndex;

			y = (pixY[xP] + 10) / 20 * heightWindow;
			MoveToEx(hdcBack, 0, y, NULL);

			SelectObject(hdcBack, GetStockObject(BLACK_PEN));
			for (int x = 0; x < BUFFER_SIZE; x++) {
				xP = (xP + 1) % BUFFER_SIZE;
				y = (pixY[xP] + 10) / 20 * heightWindow;
				LineTo(hdcBack, x / (float)BUFFER_SIZE*widthWindow, y);
				MoveToEx(hdcBack, x / (float)BUFFER_SIZE*widthWindow, y, NULL);
			}

		
			BitBlt(hdc, 0, 0, widthWindow, heightWindow, hdcBack, 0, 0, SRCCOPY);

			ReleaseDC(hWnd, hdc);
		}
		break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

			// TODO: Add any drawing code that uses hdc here...
			
			EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
		StopDAQ();
		KillTimer(hWnd, 0);

		if (hdcBack) {
			DeleteDC(hdcBack); hdcBack = NULL;
		}

		if (hdcBackGround) {
			DeleteDC(hdcBackGround); hdcBackGround = NULL;
		}

		if (screenMain) {
			DeleteObject(screenMain); screenMain = NULL;
		}
		
		PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
