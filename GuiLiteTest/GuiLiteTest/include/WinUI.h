#pragma once

#include <vector>
#include <thread>
#include <windows.h>

class CUIBlock;
class ui_widgets;

class WinUI {
public:

  //initialize global ui variables.
  static int InitGlobal();
  
  static bool globalUIInitialized;

  WinUI();
  ~WinUI();

  void Run();
  void Stop();
  bool IsRunning();
  void MsgLoop();
  void RenderLoop();
  BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);

  CUIBlock* GetUIBlock() { return ui_block; }
private:

  bool running;
  CUIBlock* ui_block;
  HINSTANCE hInst;
  HWND hWnd;

  unsigned UI_WIDTH;
  unsigned UI_HEIGHT;

  std::thread renderThread;
  std::thread msgThread;
};
