#define _CRT_SECURE_NO_WARNINGS

#include "UIWidgets.h"
#define GUILITE_ON
#include "GuiLite.h"
#define PRIMARY_COLOR 0xb3e5fc
#define PRIMARY_LIGHT_COLOR 0xe6ffff

#define SECONDARY_COLOR 0xd65100
#define SECONDARY_DARK 0x9c1900
#define TEXT_COLOR 0x000000

enum WND_ID
{
	ID_ROOT = 1,
	ID_LABEL_1,
	ID_LABEL_2,
	ID_LABEL_3,
	ID_BUTTON
};

extern const FONT_INFO Consolas_24B;

class ui_widgets : public c_wnd
{
public:
	ui_widgets() :width(0), height(0),
		s_display(NULL), frame_buf(NULL){}
	virtual ~ui_widgets();
	virtual void on_init_children();
	virtual void on_paint(void);
	void on_button_clicked(int ctrl_id, int param);
	void on_spinbox_change(int ctrl_id, int value);
	void on_listbox_confirm(int ctrl_id, int value);
	void create_ui(int screen_width, int screen_height, int color_bytes);
	void sendTouch(int x, int y, bool is_down);
	void sendKey(unsigned int key);
	void* getUi(int* width, int* height, bool force_update);

	unsigned width, height;

	c_display* s_display;
	void* frame_buf;
	std::vector<WND_TREE> tree;
	std::vector <c_label> labels;
	std::vector <c_button> buttons;
};

ui_widgets::~ui_widgets()
{
	if (s_display != NULL) {
		delete s_display;
	}

	if (frame_buf != NULL) {
		delete frame_buf;
	}
}

void load_resource()
{
	c_theme::add_font(FONT_DEFAULT, &Consolas_24B);
	//for button
	c_theme::add_color(COLOR_WND_FONT, TEXT_COLOR);
	c_theme::add_color(COLOR_WND_NORMAL, SECONDARY_COLOR);
	c_theme::add_color(COLOR_WND_PUSHED, SECONDARY_DARK);
	c_theme::add_color(COLOR_WND_FOCUS, GL_RGB(78, 198, 76));
	c_theme::add_color(COLOR_WND_BORDER, GL_RGB(50, 50, 50));
}

void ui_widgets::create_ui(int screen_width, int screen_height, int color_bytes) {
	load_resource();
	width = screen_width;
	height = screen_height;
	tree.resize(5);
	labels.resize(3);
	buttons.resize(1);

	tree[0] = { &labels[0],	ID_LABEL_1,	"label 1",	50, 30, 150, 50 };
	tree[1] = { &labels[1],	ID_LABEL_2,	"label 2",	50, 100, 150, 50 };
	tree[2] = { &labels[2],	ID_LABEL_3,	"label 3",	50, 170, 150, 50 };
	tree[3] =	{ &buttons[0], ID_BUTTON, "Button", 300, 30, 150, 50},
	tree[4] = { NULL, 0 , 0, 0, 0, 0, 0 };

  frame_buf = calloc(width * height, color_bytes);
	c_surface* s_surface = new c_surface(width, height, color_bytes, Z_ORDER_LEVEL_1);
	s_display = new c_display(frame_buf, width, height, s_surface);
	set_surface(s_surface);
	set_bg_color(PRIMARY_COLOR);
	connect(NULL, ID_ROOT, 0, 0, 0, width, height, tree.data());
	show_window();
}

void ui_widgets::on_init_children()
{
  c_button* button = (c_button*)get_wnd_ptr(ID_BUTTON);
  button->set_on_click((WND_CALLBACK)&ui_widgets::on_button_clicked);
}

void ui_widgets::on_paint(void)
{
  m_surface->fill_rect(0, 0, width - 1, height - 1, PRIMARY_COLOR, Z_ORDER_LEVEL_0);
}
void ui_widgets::on_button_clicked(int ctrl_id, int param)
{
  static int s_cnt;
  char str[16];
  sprintf(str, "%d click", ++s_cnt);
  c_label* label = (c_label*)get_wnd_ptr(ID_LABEL_1);
  label->set_str(str);
  label->show_window();
}
void ui_widgets::on_spinbox_change(int ctrl_id, int value)
{
  c_label* label = (c_label*)get_wnd_ptr(ID_LABEL_2);
  label->set_str("change");
  label->show_window();
}
void ui_widgets::on_listbox_confirm(int ctrl_id, int value)
{
  char str[16];
  sprintf(str, "choose %d", value);
  c_label* label = (c_label*)get_wnd_ptr(ID_LABEL_3);
  label->set_str(str);
  label->show_window();
}

void ui_widgets::sendTouch(int x, int y, bool is_down)
{
  is_down ? on_touch(x, y, TOUCH_DOWN) : on_touch(x, y, TOUCH_UP);
}

void ui_widgets::sendKey(unsigned int key)
{
  on_navigate(NAVIGATION_KEY(key));
}

void* ui_widgets::getUi(int* width, int* height, bool force_update)
{
  return s_display->get_updated_fb(width, height, force_update);
}

CUIBlock::CUIBlock():ui(new ui_widgets())
{
	m_color_bytes = 4;
	m_is_mouse_down = false;
	m_ui_width = m_ui_height = 0;
}

CUIBlock::~CUIBlock() {
	delete ui;
}

void CUIBlock::CreateUI(int screen_width, int screen_height, int color_bytes)
{
	ui->create_ui(screen_width, screen_height, color_bytes);
}

void CUIBlock::renderUI(RECT& rect, HDC hDC)
{
	void* data = NULL;
	if (rect.right == m_block_rect.right && rect.bottom == m_block_rect.bottom &&
		m_ui_width != 0)
	{
		data = ui->getUi(NULL, NULL, false);
		goto RENDER;
	}

	m_block_rect = rect;
	data = ui->getUi(&m_ui_width, &m_ui_height, true);

	memset(&m_ui_bm_info, 0, sizeof(m_ui_bm_info));
	m_ui_bm_info.bmiHeader.biSize = sizeof(MYBITMAPINFO);
	m_ui_bm_info.bmiHeader.biWidth = m_ui_width;
	m_ui_bm_info.bmiHeader.biHeight = (0 - m_ui_height);//fix upside down. 
	m_ui_bm_info.bmiHeader.biBitCount = m_color_bytes * 8;
	m_ui_bm_info.bmiHeader.biPlanes = 1;
	m_ui_bm_info.bmiHeader.biSizeImage = m_ui_width * m_ui_height * m_color_bytes;
	m_ui_bm_info.bmiHeader.biCompression = BI_RGB;

	DeleteDC(m_memDC);
	DeleteObject(m_blockBmp);

	m_memDC = CreateCompatibleDC(hDC);
	m_blockBmp = CreateCompatibleBitmap(hDC, (m_block_rect.right - m_block_rect.left), (m_block_rect.bottom - m_block_rect.top));
	SelectObject(m_memDC, m_blockBmp);
	::SetStretchBltMode(m_memDC, STRETCH_HALFTONE);

RENDER:
	if (!data)
	{
		return;
	}
	int block_width = (m_block_rect.right - m_block_rect.left);
	int block_height = (m_block_rect.bottom - m_block_rect.top);
	StretchDIBits(m_memDC, 0, 0, block_width, block_height,
		0, 0, m_ui_width, m_ui_height, data, (BITMAPINFO*)&m_ui_bm_info, DIB_RGB_COLORS, SRCCOPY);
	BitBlt(hDC, m_block_rect.left, m_block_rect.top, block_width, block_height, m_memDC, 0, 0, SRCCOPY);
}

void CUIBlock::OnLButtonDown(int x, int y)
{
	m_is_mouse_down = TRUE;
	pointMFC2GuiLite(x, y);
	if (x < 0 || y < 0)
	{
		return;
	}
	ui->sendTouch(x, y, true);
}

void CUIBlock::OnLButtonUp(int x, int y)
{
	m_is_mouse_down = FALSE;
	pointMFC2GuiLite(x, y);
	if (x < 0 || y < 0)
	{
		return;
	}
	ui->sendTouch(x, y, false);
}

void CUIBlock::OnMouseMove(int x, int y)
{
	if (m_is_mouse_down == FALSE)
	{
		return;
	}
	OnLButtonDown(x, y);
}

void CUIBlock::pointMFC2GuiLite(int& x, int& y)
{
	if (x > m_block_rect.right || y > m_block_rect.bottom ||
		x < m_block_rect.left || y < m_block_rect.top)
	{
		x = y = 0;
		return;
	}
	x = m_ui_width * (x - m_block_rect.left) / (m_block_rect.right - m_block_rect.left);
	y = m_ui_height * (y - m_block_rect.top) / (m_block_rect.bottom - m_block_rect.top);
}

void CUIBlock::OnKeyUp(unsigned int nChar)
{
	unsigned int key = 2;
	switch (nChar)
	{
	case 68:
		key = 0;
		break;
	case 65:
		key = 1;
		break;
	}
	ui->sendKey(key);
}
