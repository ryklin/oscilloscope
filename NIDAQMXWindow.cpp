///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2022  Neuro Software Developers, Inc.
//
// Contact Information:
//
// Email:
// info@neurosoftware.com
//
// Website:
// www.neurosoftware.com
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NIDAQMXWindow.h"
#include <iostream>
#include <sstream>
#include <iomanip> //setprecision
#include <vector>
#include <algorithm>
#include "NIDAQmx.h"

using namespace std;

#define NUM_CHANNELS 8 // some cards have 16 channels, and depends also if wired differential or single ended

TaskHandle taskHandle = 0;
int daqDeviceIndexChosen = 1;
vector<string>daqDevices;
const int arraySizeInSamps = NUM_CHANNELS;
float64 readArray[NUM_CHANNELS];

#define BUFFER_SIZE 1024  // arbitrary number of bytes that I want to buffer in my computer's RAM

float widthWindow = 1024;
float heightWindow = 768;
HDC hdcBackGround = NULL;
HBITMAP screenMain = NULL; 
HPEN color[NUM_CHANNELS];
HPEN colorGray;
HPEN colorGrayDashed;
HPEN colorGrayDot;
HBRUSH backgroundBrush;

string daqMessage[10];
int daqMessageIndex = 0;

float pix[NUM_CHANNELS][BUFFER_SIZE];
int sampleNum = 0;

int show2D = 1;
int pauseScreen = -1;
int showSampleValues = -1;
int hideGrid = -1;

void StopDAQ();

void InitDAQ() {

	StopDAQ();

	int32 status = 0;
	char errorString[MAX_PATH];

	/*********************************************/
	// DAQmx Configure Code
	/*********************************************/
	status = DAQmxCreateTask("", &taskHandle);

	if (status != 0) {
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		MessageBoxA(0, errorString, "Oscilloscope-NIDAQmx", MB_ICONERROR);
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

	char channelStr[] = "ai0:7";
	char nameToAssignToChannel[255] = {""};

	sprintf_s(nameToAssignToChannel, "%s/%s", daqDevices[daqDeviceIndexChosen].c_str(), channelStr);

	status = DAQmxCreateAIVoltageChan(taskHandle, nameToAssignToChannel, "", terminalConfig, -10.0, 10.0, units, NULL);
	if (status != 0) {
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		MessageBoxA(0, errorString, "Oscilloscope-NIDAQmx", MB_ICONERROR);
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
//	int32 sampleMode = DAQmx_Val_HWTimedSinglePoint;
	int32 sampleMode = DAQmx_Val_ContSamps;

	/*
	One of the most important parameters of an analog input or output system is the rate at which the measurement device samples an incoming signal or generates the output signal.
	The sampling rate, which is called the scan rate in Traditional NI-DAQ (Legacy), is the speed at which a device acquires or generates a sample on each channel.
	A fast input sampling rate acquires more points in a given time and can form a better representation of the original signal than a slow sampling rate.
	Generating a 1 Hz signal using 1000 points per cycle at 1000 S/s produces a much finer representation than using 10 points per cycle at a sample rate of 10 S/s.
	Sampling too slowly results in a poor representation of the analog signal. Undersampling causes the signal to appear as if it has a different frequency than it actually does.
	This misrepresentation of a signal is called aliasing.
	*/

	float64 sampleRate = 50; //The sampling rate in samples per second per channel. If you use an external source for the Sample Clock, set this value to the maximum expected rate of that clock.

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
		MessageBoxA(0, errorString, "Oscilloscope-NIDAQmx", MB_ICONERROR);
		return;
	}

	/*********************************************/
	// start the data acquisition
	/*********************************************/
	status = DAQmxStartTask(taskHandle);
	if (status != 0) {
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		MessageBoxA(0, errorString, "Oscilloscope-NIDAQmx", MB_ICONERROR);
		return;
	}

}

void clearData() {
	for (int channel = 0; channel < NUM_CHANNELS; channel++) {
		for (int x = 0; x < BUFFER_SIZE; x++) {
			pix[channel][x] = readArray[channel];
//			pix[channel][x] = 0;
		}
	}
}

string daqRead() {
	static int firstSample = 0;
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
		message << "(" + to_string(sampleNum) + ")" + " DAQmxReadAnalogF64() status:" << status << " " << errorString;
	}
	//if (DAQmxErrorSamplesNotYetAvailable == status || DAQmxErrorSamplesNoLongerAvailable == status) {
	//	message << "status:" << status << " continue" << std::endl;
	//	OutputDebugStringA(message.str().c_str());

	//}
	//else if (status < 0) {
	//	message << "status:" << status << "breaking" << std::endl;
	//}
	else {
		if (firstSample == 0)  // do this only on startup 
		{
			firstSample = 1;

			clearData();
		}
		else {  // do this on every subsequent sample after the initial one
			int pixIndex = (sampleNum++) % BUFFER_SIZE;
			message << std::fixed << std::setprecision(2);
			message << "(" << to_string(sampleNum) << ")";
			for (int channel = 0; channel < arraySizeInSamps; channel++) {
				message << readArray[channel];
				if (channel < arraySizeInSamps - 1)
					message << ", ";
				pix[channel][pixIndex] = readArray[channel];
			}
		}
	}
	message << endl;
	//OutputDebugStringA(message.str().c_str());
	return message.str();
}

void StopDAQ() {
	int32 status;
	char errorString[MAX_PATH];

	if (taskHandle == NULL)
		return;
	status = DAQmxStopTask(taskHandle);
	if (status != 0) {
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		MessageBoxA(0, errorString, "Oscilloscope-NIDAQmx", MB_ICONERROR);
		return;
	}

	status = DAQmxClearTask(taskHandle);
	if (status != 0) {
		DAQmxGetErrorString(status, errorString, MAX_PATH);
		MessageBoxA(0, errorString, "Oscilloscope-NIDAQmx", MB_ICONERROR);
		return;
	}
}

// the rest is mostly boiler plate code except where I call the above functions and graph the data in the WM_TIMER message section of the WndProc

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
CHAR szTitle[MAX_LOADSTRING];                  // The title bar text
CHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    ChoseDAQ(HWND, UINT, WPARAM, LPARAM);
void				EnumerateDAQDevices(HWND hWnd);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    //LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	strcpy_s(szTitle, "Digital Oscilliscope");
	strcpy_s(szWindowClass, "NIDAQMXWINDOW");
    //LoadStringW(hInstance, IDC_NIDAQMXWINDOW, szWindowClass, MAX_LOADSTRING);
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
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NIDAQMXWINDOW));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_NIDAQMXWINDOW);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
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

   HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
	   CW_USEDEFAULT, CW_USEDEFAULT, widthWindow, heightWindow, nullptr, nullptr, hInstance, nullptr);

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
	{
		// set these colors manually because I can't think of a better way
		color[0] = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
		color[1] = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
		color[2] = CreatePen(PS_SOLID, 1, RGB(255, 255, 0));
		color[3] = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
		color[4] = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
		color[5] = CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
		color[6] = CreatePen(PS_SOLID, 1, RGB(125, 255, 255));
		color[7] = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

		colorGray = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
		colorGrayDashed = CreatePen(PS_DASH, 1, RGB(180, 180, 180));
		colorGrayDot = CreatePen(PS_DOT, 1, RGB(180, 180, 180));
		backgroundBrush = CreateSolidBrush(RGB(128, 128, 128));

		memset(pix, 0, sizeof(float)*NUM_CHANNELS*BUFFER_SIZE);  // optional, I do it as a precaution.

		EnumerateDAQDevices(hWnd);
		
		SetTimer(hWnd, WM_TIMER, 0, (TIMERPROC)NULL);
	}
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
			case ID_FILE_CLEARSCREEN:
				clearData();
				break;
			case ID_FILE_SHOW2D:
				show2D *= -1;
				if (show2D) {
					CheckMenuItem(GetMenu(hWnd), ID_FILE_SHOW2D, MF_CHECKED);
				}
				else {
					CheckMenuItem(GetMenu(hWnd), ID_FILE_PAUSE, MF_UNCHECKED);
				}
				break;
			case ID_FILE_PAUSE:
				pauseScreen *= -1;
				if (pauseScreen == 1) {
					CheckMenuItem(GetMenu(hWnd), ID_FILE_PAUSE, MF_CHECKED);
				}
				else if (pauseScreen == -1) {
					CheckMenuItem(GetMenu(hWnd), ID_FILE_PAUSE, MF_UNCHECKED);
				}
				break;
			case ID_FILE_SHOWSAMPLEVALUES:
				showSampleValues *= -1;
				if (showSampleValues == 1) {
					CheckMenuItem(GetMenu(hWnd), ID_FILE_SHOWSAMPLEVALUES, MF_CHECKED);
				}
				else if (showSampleValues == -1) {
					CheckMenuItem(GetMenu(hWnd), ID_FILE_SHOWSAMPLEVALUES, MF_UNCHECKED);
				}
				break;
			case ID_FILE_SHOWGRID:
				hideGrid *= -1;
				if (hideGrid == 1) {
					CheckMenuItem(GetMenu(hWnd), ID_FILE_SHOWGRID, MF_CHECKED);
				}
				else if (hideGrid == -1) {
					CheckMenuItem(GetMenu(hWnd), ID_FILE_SHOWGRID, MF_UNCHECKED);
				}		
				break;
			case IDM_DAQ:
				DialogBox(hInst, MAKEINTRESOURCE(IDD_CHOOSE_DAQ), hWnd, ChoseDAQ);
				break;
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

		if (pauseScreen == 1) {
			break;
		}
		else {
			int sampleIndex = sampleNum%BUFFER_SIZE;
			int messageIndex = (daqMessageIndex++) % 10;
			daqMessage[messageIndex] = daqRead();

			const int edge = 40;

			HDC hdc = GetDC(hWnd);

			RECT rect = { 0,0,widthWindow, heightWindow};
			FillRect(hdcBack, &rect, backgroundBrush);
			float x = 0;
			float y = 0;

			// draw a vertical line demarcating the Y-axis boundary			
			SelectObject(hdcBack, colorGray);
			MoveToEx(hdcBack, edge, y, NULL);
			LineTo(hdcBack, edge, heightWindow);

			// plots the y axis
			SetTextColor(hdcBack, RGB(180, 180, 180));
			for (float yAxis = 0; yAxis < 20; yAxis++) {
				y = yAxis / 20 * heightWindow;
				MoveToEx(hdcBack, edge, y, NULL);

				stringstream buffer;
				int value = ((yAxis - 10)*-1);

				if (value <= 0) {
					buffer << value << "V";
				}
				else {
					buffer << "+" << value << "V";
				}
				TextOutA(hdcBack, 11, y - 8, buffer.str().c_str(), buffer.str().length());

				if (hideGrid == 1) {
					LineTo(hdcBack, edge + 7, y);
				}
				else {
					if (value % 5 == 0) {
						SelectObject(hdcBack, colorGrayDashed);
					}
					else {
						SelectObject(hdcBack, colorGrayDot);
					}
					LineTo(hdcBack, widthWindow, y);
				}
			}

			// render data from all analog input channels
			int xP = 0;
//			for (int channel = 0; channel < NUM_CHANNELS; channel++) {
			for (int channel = 0; channel < 4; channel++) {

				xP = sampleNum%BUFFER_SIZE;
				y = (pix[channel][xP] + 10) / 20 * heightWindow * -1;
				MoveToEx(hdcBack, edge, heightWindow+y, NULL);
				SelectObject(hdcBack, color[channel]);

				for (int x = 1; x < BUFFER_SIZE; x++) {
					xP = (xP + 1) % BUFFER_SIZE;								// increment to next data value INDEX (wrap if necessary)
					y = (pix[channel][xP] + 10) / 20 * heightWindow*-1;			// get data value based on INDEX and scale into window's space. First scale from -10/10V to 0-1 (normalized)
					
					int xPosition = ((float)x / (float)BUFFER_SIZE*(widthWindow-edge));

					LineTo(hdcBack, xPosition + edge, heightWindow + y);
					MoveToEx(hdcBack, xPosition + edge, heightWindow+y, NULL);
				}
			}

			// render XY Plot
			if (show2D == 1) {
				float hyp = sqrt(widthWindow*widthWindow + heightWindow*heightWindow);
				int side2D = hyp * 1 / 5;
				float edge2D = side2D * 1 / 10;
				RECT rect = { widthWindow - edge2D - side2D, edge2D, widthWindow - edge2D, edge2D + side2D };
				int height2D = rect.bottom - rect.top;
				int width2D = rect.right - rect.left;

				// render the background
				SelectObject(hdcBack, backgroundBrush);
				SelectObject(hdcBack, colorGray);
				Rectangle(hdcBack, rect.left, rect.top, rect.right, rect.bottom);
				// render the x axis
				MoveToEx(hdcBack, rect.left, rect.top / 2 + rect.bottom / 2, NULL);
				LineTo(hdcBack, rect.right, rect.top / 2 + rect.bottom / 2);
				// render the y axis
				MoveToEx(hdcBack, rect.left + (width2D) / 2, rect.top, NULL);
				LineTo(hdcBack, rect.left + (width2D) / 2, rect.top + rect.bottom - edge2D);


				// render the data
				char sampleNumStr[255];
				sprintf_s(sampleNumStr, "[%i] ", sampleNum);
				for (int channel = 0; channel < 2; channel++) {

					(hdcBack, GetStockObject(NULL_BRUSH));
					SelectObject(hdcBack, color[channel * 2]);
					MoveToEx(hdcBack, x + rect.left+width2D/2, y+height2D/2+edge2D, NULL);

					float x = pix[channel * 2 + 0][sampleIndex-1];
					float y = pix[channel * 2 + 1][sampleIndex-1];

					sprintf_s(sampleNumStr, "%s(%4.2f, %4.2f)", sampleNumStr, x, y);

					x = (x + 10.0) / 20.0;
					y = (y + 10.0) / 20.0;
					
					x = x * (float)width2D;
					y = y * (float)height2D;
	

					int diameter2D = 5;
					Ellipse(hdcBack, x + rect.left - diameter2D + 1, y + edge2D - diameter2D + 1, x + rect.left + diameter2D + 1, y + edge2D + diameter2D);
				}
				if (showSampleValues == 1) TextOut(hdcBack, rect.left + 1, rect.top - 20, sampleNumStr, strlen(sampleNumStr));
			}

			if (showSampleValues == 1) {

				int x = widthWindow / 2 - 150;
				int y = edge;
				RECT rect = { x, y, x + 350, y + 200 };

				FillRect(hdcBack, &rect, backgroundBrush);
				SetTextColor(hdcBack, RGB(180, 180, 180));
				for (int i = 0; i < 10; i++) {
					string result = string("[" + to_string(daqMessageIndex) + "] " + daqMessage[i]);

					TextOutA(hdcBack, x, y + 20 * i, result.c_str(), result.length());
				}
			}


			// render everything to screen
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
		if (backgroundBrush) {
			DeleteObject(backgroundBrush); backgroundBrush = NULL;
		}
		if (hdcBack) {
			DeleteDC(hdcBack); hdcBack = NULL;
		}

		if (hdcBackGround) {
			DeleteDC(hdcBackGround); hdcBackGround = NULL;
		}

		if (screenMain) {
			DeleteObject(screenMain); screenMain = NULL;
		}
		for (int channel = 0; channel < NUM_CHANNELS; channel++) {
			DeleteObject(color[channel]);
		}
		DeleteObject(colorGray);
		DeleteObject(colorGrayDashed);
		DeleteObject(colorGrayDot);
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

INT_PTR CALLBACK ChoseDAQ(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	
	switch (message)
	{
	case WM_INITDIALOG:

		for (auto deviceName : daqDevices) {
			char value[255];
			strcpy_s(value, deviceName.c_str());

			SendMessage(GetDlgItem(hDlg, IDC_COMBO_DAQ_DEVICES), CB_ADDSTRING, 0, (LPARAM)&value);
		}
		SendMessage(GetDlgItem(hDlg, IDC_COMBO_DAQ_DEVICES), CB_SETCURSEL, daqDeviceIndexChosen, NULL);

		return (INT_PTR)TRUE;

	case WM_COMMAND:
		switch (HIWORD(wParam))
		{
		case CBN_SELCHANGE:		// drop down control changed
			daqDeviceIndexChosen = SendMessage(GetDlgItem(hDlg, IDC_COMBO_DAQ_DEVICES), CB_GETCURSEL, 0, 0);
		default:
			break;
		}

		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			InitDAQ();
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

vector<string> splitString(std::string str, char delimiter) {
	vector<string> v;
	stringstream src(str);
	string buf;

	while (getline(src, buf, delimiter)) {
		v.push_back(buf);
	}
	return v;
}

void EnumerateDAQDevices(HWND hWnd) {

	char deviceNamesStr[255];
	DAQmxGetSystemInfoAttribute(DAQmx_Sys_DevNames, deviceNamesStr, 255); // this will query the nidaq driver to see what cards are detected

	string deviceNames = deviceNamesStr;
	deviceNames.erase(std::remove_if(deviceNames.begin(), deviceNames.end(), isspace), deviceNames.end());

	daqDevices = splitString(deviceNames, ',');

	DialogBox(hInst, MAKEINTRESOURCE(IDD_CHOOSE_DAQ), hWnd, ChoseDAQ);
}
