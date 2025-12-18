#include <windows.h>
#include <string>
#include <thread>

#define SHM_NAME   "Global\\MyChatMemory"
#define MUTEX_NAME "Global\\MyChatMutex"
#define EVENT_NAME "Global\\MyChatEvent"

#define MAX_MESSAGES 32
#define MSG_SIZE 256
#include "resource.h"

/*
==================== Developer Notes ====================
Shared-memory chat server.
Shows all messages sent through shared memory in the GUI.
========================================================
*/

struct ShmData {
    unsigned long long seq;
    char msgs[MAX_MESSAGES][MSG_SIZE];
};

HWND hInput, hSendBtn, hListBox;
HANDLE hMap, hMutex, hEvent;
ShmData* shm = nullptr;

COLORREF btnColor   = RGB(70, 130, 180);
COLORREF btnText    = RGB(255, 255, 255);
COLORREF logBgColor = RGB(225, 240, 255);

// -------------------- Button Drawing --------------------
void DrawButton(HDC hdc, RECT rc, const char* text) {
    HBRUSH brush = CreateSolidBrush(btnColor);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, btnText);
    DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// -------------------- Add message to listbox --------------------
void AddMessage(const char* msg) {
    SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)msg);
}

// -------------------- Broadcast message from server --------------------
void BroadcastMessage() {
    char text[MSG_SIZE];
    GetWindowTextA(hInput, text, MSG_SIZE);
    if (!strlen(text)) return;

    WaitForSingleObject(hMutex, INFINITE);

    shm->seq++;
    unsigned idx = shm->seq % MAX_MESSAGES;
    std::string msg = "Server: " + std::string(text);

    strncpy(shm->msgs[idx], msg.c_str(), MSG_SIZE - 1);
    shm->msgs[idx][MSG_SIZE - 1] = '\0';

    ReleaseMutex(hMutex);
    SetEvent(hEvent);

    AddMessage(msg.c_str());
    SetWindowTextA(hInput, "");
}

// -------------------- Monitor shared memory for new messages --------------------
DWORD WINAPI MonitorShm(LPVOID) {
    unsigned long long lastSeq = shm->seq;

    while (true) {
        WaitForSingleObject(hEvent, INFINITE);

        WaitForSingleObject(hMutex, INFINITE);

        // Add all new messages
        while (lastSeq < shm->seq) {
            lastSeq++;
            unsigned idx = lastSeq % MAX_MESSAGES;
            AddMessage(shm->msgs[idx]);
        }

        ReleaseMutex(hMutex);

        // Reset the event for next message
        ResetEvent(hEvent);
    }
    return 0;
}

// -------------------- Window Procedure --------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE:
        hInput = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            50, 30, 300, 30, hwnd, NULL, NULL, NULL);

        hSendBtn = CreateWindow("BUTTON", "Broadcast",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            370, 30, 80, 30, hwnd, (HMENU)1, NULL, NULL);

        hListBox = CreateWindow("LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            50, 80, 400, 350, hwnd, NULL, NULL, NULL);
        break;

    case WM_DRAWITEM:
        DrawButton(((LPDRAWITEMSTRUCT)lParam)->hDC,
                   ((LPDRAWITEMSTRUCT)lParam)->rcItem,
                   "Broadcast");
        break;

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, logBgColor);
        return (LRESULT)CreateSolidBrush(logBgColor);
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) BroadcastMessage();
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// -------------------- WinMain --------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // Create shared memory, mutex, and event
    hMap   = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(ShmData), SHM_NAME);
    shm    = (ShmData*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ShmData));
    shm->seq = 0;

    hMutex = CreateMutexA(NULL, FALSE, MUTEX_NAME);
    hEvent = CreateEventA(NULL, TRUE, FALSE, EVENT_NAME);

    // Start monitoring thread
    std::thread(MonitorShm, nullptr).detach();

    // Window class
    WNDCLASS wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "ShmServer";
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(104));

    RegisterClass(&wc);

    HWND hwnd = CreateWindow("ShmServer", "Shared Memory Chat Server",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        500, 500, NULL, NULL, hInst, NULL);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)));

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
