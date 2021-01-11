#pragma once

#include <vector>
#include <windows.h>

#include "WinUI.h"

typedef struct tagMYBITMAPINFO {
	BITMAPINFOHEADER    bmiHeader;
	unsigned				biRedMask;
	unsigned				biGreenMask;
	unsigned				biBlueMask;
} MYBITMAPINFO;

class ui_widgets;

///wrapper of ui_widgets so that
///CUIBlock can be used without including
///GuiLite.h
class CUIBlock
{
public:
	CUIBlock();
	~CUIBlock();

	struct ButtonSpec {
		std::string label;
		ButtonCallback cb;
		int uiIndex;
		int width;
		int height;
		ButtonSpec() :uiIndex(0), width(150), height(50) {}
	};

	///ui has to be created before rendering.
	void AddButton(const std::string & label, ButtonCallback& cb);

	void renderUI(RECT& rect, HDC hDC);
	void OnLButtonDown(int x, int y);
	void OnLButtonUp(int x, int y);
	void OnMouseMove(int x, int y);
	void OnKeyUp(unsigned int key);
	void CreateUI(int screen_width, int screen_height, int color_bytes);
	ui_widgets* ui;
	///not actual buttons.
	std::vector< ButtonSpec> buttonSpecs;
private:
	
	void pointMFC2GuiLite(int& x, int& y);

	int m_color_bytes;
	RECT m_block_rect;
	int m_ui_width;
	int m_ui_height;

	MYBITMAPINFO m_ui_bm_info;
	HDC			m_memDC;
	HBITMAP		m_blockBmp;

	bool m_is_mouse_down;
};
