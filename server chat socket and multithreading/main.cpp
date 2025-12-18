#include <windows.h>
#include <winsock2.h>
#include <vector>
#include <thread>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")
#include "resource.h"

/*
========================================================
TCP CHAT SERVER (GUI)
--------------------------------------------------------
- Uses WinSock (blocking sockets)
- Accepts multiple clients via threads
- Broadcasts messages to all connected clients
- Thread-safe client list using std::mutex
- Light blue GUI with scrollable log window
- Custom icon for taskbar/title
========================================================
*/

HWND hPortInput, hStartBtn, hLogBox;
SOCKET serverSocket;
bool running = false;

std::vector<SOCKET> clients;
std::mutex clientsMtx;

// -------------------- Colors --------------------
COLORREF winBgColor   = RGB(225, 240, 255); // window background
COLORREF inputBgColor = RGB(240, 248, 255); // edit boxes
COLORREF btnColor     = RGB(70, 130, 180);  // buttons
COLORREF btnTextColor = RGB(255, 255, 255); // button text
COLORREF logBgColor   = RGB(245, 255, 255); // log box background

// -------------------- Logging --------------------
void Log(const char* text) {
    int len = GetWindowTextLength(hLogBox);
    SendMessage(hLogBox, EM_SETSEL, len, len);
    SendMessage(hLogBox, EM_REPLACESEL, 0, (LPARAM)text);
    SendMessage(hLogBox, EM_REPLACESEL, 0, (LPARAM)"\r\n");
}

// -------------------- Broadcast --------------------
void Broadcast(const char* msg, SOCKET exclude) {
    std::lock_guard<std::mutex> lock(clientsMtx);
    for (SOCKET c : clients)
        if (c != exclude)
            send(c, msg, strlen(msg), 0);
}

// -------------------- Client Thread --------------------
void ClientThread(SOCKET client) {
    char buffer[512];
    Log("Client connected.");

    while (true) {
        int bytes = recv(client, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = 0;
        Broadcast(buffer, client);
        Log(buffer);
    }

    closesocket(client);
    Log("Client disconnected.");
}

// -------------------- Server Accept Thread --------------------
void ServerThread() {
    while (running) {
        SOCKET client = accept(serverSocket, NULL, NULL);
        if (client != INVALID_SOCKET) {
            clients.push_back(client);
            std::thread(ClientThread, client).detach();
        }
    }
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
        // Port input
        hPortInput = CreateWindow("EDIT", "8080",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            50, 20, 100, 25, hwnd, NULL, NULL, NULL);

        // Start server button
        hStartBtn = CreateWindow("BUTTON", "Start Server",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            180, 20, 150, 25, hwnd, (HMENU)1, NULL, NULL);

        // Scrollable log box
        hLogBox = CreateWindow("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
            50, 70, 400, 360, hwnd, NULL, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1 && !running) {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2,2), &wsa);

            serverSocket = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(8080);
            addr.sin_addr.s_addr = INADDR_ANY;

            bind(serverSocket, (sockaddr*)&addr, sizeof(addr));
            listen(serverSocket, SOMAXCONN);

            running = true;
            std::thread(ServerThread).detach();
            Log("Server started.");
        }
        break;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT d = (LPDRAWITEMSTRUCT)lParam;
        if (d->CtlID == 1) DrawButton(d->hDC, d->rcItem, "Start Server");
        break;
    }

    case WM_ERASEBKGND: {
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
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, inputBgColor);
        SetTextColor(hdc, RGB(0,0,0));
        static HBRUSH hBrush = CreateSolidBrush(inputBgColor);
        return (LRESULT)hBrush;
    }

    case WM_DESTROY:
        running = false;
        closesocket(serverSocket);
        WSACleanup();
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// -------------------- WinMain --------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "TCPServer";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));

    RegisterClass(&wc);

    HWND hwnd = CreateWindow("TCPServer", "TCP Chat Server",
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
