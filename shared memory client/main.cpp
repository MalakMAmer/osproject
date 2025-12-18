#include <windows.h>
#include <string>
#include <thread>

#define SHM_NAME   "Global\\MyChatMemory"
#define MUTEX_NAME "Global\\MyChatMutex"
#define EVENT_NAME "Global\\MyChatEvent"

#define MAX_MESSAGES 32
#define MSG_SIZE 256

#include "resource.h"

struct ShmData {
    unsigned long long seq;
    char msgs[MAX_MESSAGES][MSG_SIZE];
};

// ===================== Globals =====================
HWND hInput, hSendBtn, hListBox;
HANDLE hMap, hMutex, hEvent;
ShmData* shm = nullptr;

bool running = true;
unsigned long long lastSeq = 0;
std::string username;

// -------- username window --------
HWND hNameEdit;
bool gotUsername = false;

// ===================== Colors (MATCH SOCKET CLIENT) =====================
COLORREF bgColor    = RGB(240, 248, 255);
COLORREF btnColor   = RGB(70, 130, 180);
COLORREF btnText    = RGB(255, 255, 255);
COLORREF logBgColor = RGB(225, 240, 255);

// ===================== Username Window =====================
LRESULT CALLBACK NameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            char buf[64];
            GetWindowTextA(hNameEdit, buf, 64);
            username = buf;
            if (username.empty()) username = "Client";
            gotUsername = true;
            DestroyWindow(hwnd);
        }
        break;

    case WM_CLOSE:
        username = "Client";
        gotUsername = true;
        DestroyWindow(hwnd);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void UsernameWindow(HINSTANCE hInst) {
    WNDCLASSA wc{};
    wc.lpfnWndProc = NameWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "NameWnd";
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("NameWnd", "Enter Username",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        500, 300, 300, 130,
        NULL, NULL, hInst, NULL);

    hNameEdit = CreateWindowA("EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        20, 20, 240, 25,
        hwnd, NULL, hInst, NULL);

    CreateWindowA("BUTTON", "OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        90, 60, 100, 25,
        hwnd, (HMENU)1, hInst, NULL);

    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (!gotUsername) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10);
    }
}

// ===================== UI Helpers =====================
void DrawButton(HDC hdc, RECT rc, const char* text) {
    HBRUSH brush = CreateSolidBrush(btnColor);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, btnText);
    DrawTextA(hdc, text, -1, &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void AddMessage(const char* msg) {
    SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)msg);
}

// ===================== Receiver Thread =====================
DWORD WINAPI ReceiverThread(LPVOID) {
    while (running) {
        WaitForSingleObject(hEvent, INFINITE);
        WaitForSingleObject(hMutex, INFINITE);

        while (lastSeq < shm->seq) {
            lastSeq++;
            AddMessage(shm->msgs[lastSeq % MAX_MESSAGES]);
        }

        ReleaseMutex(hMutex);
        ResetEvent(hEvent);
    }
    return 0;
}

// ===================== Send Message =====================
void SendChatMessage() {
    char text[MSG_SIZE];
    GetWindowTextA(hInput, text, MSG_SIZE);
    if (!strlen(text)) return;

    WaitForSingleObject(hMutex, INFINITE);

    shm->seq++;
    unsigned idx = shm->seq % MAX_MESSAGES;

    std::string msg = username + ": " + text;
    strncpy(shm->msgs[idx], msg.c_str(), MSG_SIZE - 1);
    shm->msgs[idx][MSG_SIZE - 1] = '\0';

    ReleaseMutex(hMutex);
    SetEvent(hEvent);

    SetWindowTextA(hInput, "");
}

// ===================== Main Window =====================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE:
        hInput = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            50, 30, 300, 30, hwnd, NULL, NULL, NULL);

        hSendBtn = CreateWindow("BUTTON", "Send",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            370, 30, 80, 30, hwnd, (HMENU)1, NULL, NULL);

        hListBox = CreateWindow("LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            50, 80, 400, 350, hwnd, NULL, NULL, NULL);
        break;

    case WM_DRAWITEM:
        DrawButton(((LPDRAWITEMSTRUCT)lParam)->hDC,
                   ((LPDRAWITEMSTRUCT)lParam)->rcItem,
                   "Send");
        return TRUE;

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, logBgColor);
        return (LRESULT)CreateSolidBrush(logBgColor);
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) SendChatMessage();
        break;

    case WM_DESTROY:
        running = false;
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ===================== WinMain =====================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {

    // ---- username first (LOGIC RESTORED) ----
    std::thread nameThread(UsernameWindow, hInst);
    while (!gotUsername) Sleep(10);
    nameThread.join();

    // ---- shared memory ----
    hMap   = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                               0, sizeof(ShmData), SHM_NAME);
    shm    = (ShmData*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ShmData));
    hMutex = CreateMutexA(NULL, FALSE, MUTEX_NAME);
    hEvent = CreateEventA(NULL, TRUE, FALSE, EVENT_NAME);

    // ---- main window ----
    WNDCLASS wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "ShmClient";
    wc.hbrBackground = CreateSolidBrush(bgColor);
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));


    RegisterClass(&wc);

    HWND hwnd = CreateWindow("ShmClient", "Shared Memory Chat Client",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 500,
        NULL, NULL, hInst, NULL);
    SendMessage(hwnd, WM_SETICON, ICON_BIG,
    (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)));

    SendMessage(hwnd, WM_SETICON, ICON_SMALL,
    (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)));

    ShowWindow(hwnd, nCmdShow);

    CreateThread(NULL, 0, ReceiverThread, NULL, 0, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
