#include "WinUI.h"
#include "UIWidgets.h"
#include "resource.h"
#include <windows.h>
#include <windowsx.h>
#include <WinUser.h>
#include <stdio.h>

CHAR* WindowTitle = "GuiLite";                  // The title bar text
CHAR* WindowClass = "GuiLiteWindow";            // the main window class name
ATOM                RegisterWindowClass(HINSTANCE hInstance);

bool WinUI::globalUIInitialized = false;
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int WinUI::InitGlobal()
{
  HINSTANCE hInstance = GetModuleHandle(NULL);
  int error = GetLastError();
  RegisterWindowClass(hInstance);
  globalUIInitialized = true;
  return 0;
}

WinUI::WinUI():running(false),
  ui_block(new CUIBlock()),
  UI_WIDTH(510),
  UI_HEIGHT(500)
{
  if (!globalUIInitialized) {
    int ret = InitGlobal();
  }
}

WinUI::~WinUI()
{
  Stop();
  delete ui_block;
}

bool WinUI::IsRunning() {
  return running;
}

void WinUI::Run()
{
  running = true;
  if (!msgThread.joinable()) {
    msgThread = std::thread(&WinUI::MsgLoop, this);
  }
}

void WinUI::Stop()
{
  running = false;
  if (msgThread.joinable()) {
    msgThread.join();
  }
}

void WinUI::MsgLoop()
{
  HINSTANCE hInstance = GetModuleHandle(NULL);
  int nCmdShow = SW_SHOW;
  InitInstance(hInstance, nCmdShow);
  const unsigned COLOR_BYTES = 4;
  ui_block->CreateUI(UI_WIDTH, UI_HEIGHT, COLOR_BYTES);
  
  MSG msg;
  while (running) {

    BOOL ret = GetMessage(&msg, nullptr, 0, 0);
    if (ret) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else {
      //user closed window;
      running = false;
      break;
    }

    HDC hdc = GetDC(hWnd);
    RECT rect;
    GetClientRect(hWnd, &rect);
    ui_block->renderUI(rect, hdc);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
}

ATOM RegisterWindowClass(HINSTANCE hInstance)
{
  WNDCLASSEX wc;

  wc.cbSize = sizeof(WNDCLASSEX);

  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HelloWidgets));
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName = MAKEINTRESOURCE(IDC_HelloWidgets);
  wc.lpszClassName = WindowClass;
  wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_SMALL));

  return RegisterClassEx(&wc);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  WinUI* ui = (WinUI *) GetWindowLongPtr(hWnd, GWLP_USERDATA);
  switch (message)
  {
  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    // TODO: Add any drawing code that uses hdc here....
    EndPaint(hWnd, &ps);
  }
  break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  case WM_LBUTTONDOWN:
    ui->GetUIBlock()->OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    break;
  case WM_LBUTTONUP:
    ui->GetUIBlock()->OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    break;
  case WM_MOUSEMOVE:
    ui->GetUIBlock()->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    break;
  case WM_KEYUP:
    ui->GetUIBlock()->OnKeyUp(wParam);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

BOOL WinUI::InitInstance(HINSTANCE hInstance, int nCmdShow)
{
  hInst = hInstance;

  hWnd = CreateWindowEx(0, WindowClass, WindowTitle, WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, 0, 500, 500, nullptr, nullptr, hInstance, nullptr);
  int error = GetLastError();
  if (!hWnd)
  {
    return FALSE;
  }
  SetWindowLongPtr(hWnd, GWLP_USERDATA, LONG_PTR(this));
  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);
  
  return TRUE;
}

void WinUI::AddButton(const std::string& label, ButtonCallback cb)
{
  ui_block->AddButton(label, cb);
}