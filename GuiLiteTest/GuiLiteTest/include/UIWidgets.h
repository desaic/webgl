#pragma once

#include <vector>
#include <windows.h>

typedef struct tagMYBITMAPINFO {
	BITMAPINFOHEADER    bmiHeader;
	unsigned				biRedMask;
	unsigned				biGreenMask;
	unsigned				biBlueMask;
} MYBITMAPINFO;

class ui_widgets;

class CUIBlock
{
public:
	CUIBlock();
	~CUIBlock();
	void renderUI(RECT& rect, HDC hDC);
	void OnLButtonDown(int x, int y);
	void OnLButtonUp(int x, int y);
	void OnMouseMove(int x, int y);
	void OnKeyUp(unsigned int key);
	void CreateUI(int screen_width, int screen_height, int color_bytes);
	ui_widgets* ui;

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
