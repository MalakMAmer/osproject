#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#include "resource.h"

/*
========================================================
TCP CHAT CLIENT (GUI)
--------------------------------------------------------
Socket and multithreading chat server.
sends messages through socket in the GUI.
========================================================
*/

HWND hIpInput, hPortInput, hMsgInput, hConnectBtn, hSendBtn, hLogBox;
SOCKET clientSocket = INVALID_SOCKET;
bool connected = false;

// -------------------- Colors --------------------
COLORREF winBgColor   = RGB(225, 240, 255);   // window background
COLORREF inputBgColor = RGB(240, 248, 255);   // input boxes
COLORREF btnColor     = RGB(70, 130, 180);    // buttons
COLORREF btnTextColor = RGB(255, 255, 255);   // button text
COLORREF logBgColor   = RGB(245, 255, 255);   // log box

// -------------------- Logging --------------------
void Log(const char* text) {
    int len = GetWindowTextLength(hLogBox);
    SendMessage(hLogBox, EM_SETSEL, len, len);
    SendMessage(hLogBox, EM_REPLACESEL, 0, (LPARAM)text);
    SendMessage(hLogBox, EM_REPLACESEL, 0, (LPARAM)"\r\n");
}

// -------------------- Receiver Thread --------------------
DWORD WINAPI ReceiverThread(LPVOID) {
    char buffer[512];
    while (connected) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            Log("Disconnected from server.");
            connected = false;
            break;
        }
        buffer[bytes] = 0;
        Log(buffer);
    }
    return 0;
}

// -------------------- Owner-drawn button --------------------
void DrawButton(HDC hdc, RECT rect, const char* text) {
    HBRUSH brush = CreateSolidBrush(btnColor);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, btnTextColor);
    DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// -------------------- Window Procedure --------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE:
        // Static labels
        CreateWindow("STATIC", "Server IP", WS_CHILD | WS_VISIBLE, 50, 20, 100, 20, hwnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "Port", WS_CHILD | WS_VISIBLE, 200, 20, 100, 20, hwnd, NULL, NULL, NULL);

        // IP and Port inputs
        hIpInput   = CreateWindow("EDIT", "127.0.0.1", WS_CHILD | WS_VISIBLE | WS_BORDER, 50, 45, 120, 25, hwnd, NULL, NULL, NULL);
        hPortInput = CreateWindow("EDIT", "8080", WS_CHILD | WS_VISIBLE | WS_BORDER, 200, 45, 80, 25, hwnd, NULL, NULL, NULL);

        // Connect button
        hConnectBtn = CreateWindow("BUTTON", "Connect", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            300, 45, 100, 25, hwnd, (HMENU)1, NULL, NULL);

        // Message input
        hMsgInput = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            50, 90, 300, 30, hwnd, NULL, NULL, NULL);

        // Send button
        hSendBtn = CreateWindow("BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            370, 90, 80, 30, hwnd, (HMENU)2, NULL, NULL);

        // Scrollable log box
        hLogBox = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY, 50, 140, 400, 300, hwnd, NULL, NULL, NULL);
        break;

    case WM_COMMAND:
        // Connect button clicked
        if (LOWORD(wParam) == 1 && !connected) {
            char ip[32], portStr[16];
            GetWindowText(hIpInput, ip, sizeof(ip));
            GetWindowText(hPortInput, portStr, sizeof(portStr));

            WSADATA wsa;
            WSAStartup(MAKEWORD(2,2), &wsa);

            clientSocket = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(atoi(portStr));
            inet_pton(AF_INET, ip, &addr.sin_addr);

            Log("Connecting...");

            if (connect(clientSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
                Log("Connection failed.");
                closesocket(clientSocket);
            } else {
                connected = true;
                Log("Connected.");
                CreateThread(NULL, 0, ReceiverThread, NULL, 0, NULL);
            }
        }

        // Send button clicked
        if (LOWORD(wParam) == 2 && connected) {
            char msg[256];
            GetWindowText(hMsgInput, msg, sizeof(msg));
            if (strlen(msg)) {
                send(clientSocket, msg, strlen(msg), 0);
                Log((std::string("You: ") + msg).c_str());
                SetWindowText(hMsgInput, "");
            }
        }
        break;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT d = (LPDRAWITEMSTRUCT)lParam;
        if (d->CtlID == 1) DrawButton(d->hDC, d->rcItem, "Connect");
        if (d->CtlID == 2) DrawButton(d->hDC, d->rcItem, "Send");
        break;
    }

    case WM_ERASEBKGND: {
        // Light blue background for whole window
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH brush = CreateSolidBrush(winBgColor);
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
        return 1;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        // Edit controls: slightly brighter light-blue, black text
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, inputBgColor);
        SetTextColor(hdc, RGB(0,0,0));
        static HBRUSH hBrush = CreateSolidBrush(inputBgColor);
        return (LRESULT)hBrush;
    }

    case WM_DESTROY:
        connected = false;
        closesocket(clientSocket);
        WSACleanup();
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// -------------------- WinMain --------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASS wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "TCPClient";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)); // custom icon

    RegisterClass(&wc);

    HWND hwnd = CreateWindow("TCPClient", "TCP Chat Client",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        500, 500, NULL, NULL, hInst, NULL);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)));

    // Set icons (taskbar/title)
    SendMessage(hwnd, WM_SETICON, ICON_BIG,
        (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL,
        (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)));

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
