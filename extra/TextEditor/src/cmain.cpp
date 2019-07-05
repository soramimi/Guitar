
#ifndef _WIN32

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <ncurses.h>
#include <locale.h>
#include <string>
#include <vector>
#include <AbstractCharacterBasedApplication.h>
#include <string.h>
#include <QtGlobal>
#include <QDebug>
#include "cmain.h"

void siginthandler(int param)
{
}

struct CursesOreApplication::Private {
	int width;
	int height;
};

CursesOreApplication::CursesOreApplication()
	: m(new Private)
{
}

CursesOreApplication::~CursesOreApplication()
{
	delete m;
}

int CursesOreApplication::screenWidth() const
{
	return m->width;
}

int CursesOreApplication::screenHeight() const
{
	return m->height;
}



void CursesOreApplication::clearLine(int y)
{
	std::vector<char> arr(y + 1);
	std::fill(arr.begin(), arr.begin() + y, ' ');
	arr[y] = 0;
	mvprintw(y, 0, &arr[0]);
}

void CursesOreApplication::paintScreen()
{
	auto SetAttribute = [](CharAttr const &attr){
		attr_t a = A_NORMAL;
		if (attr.index == CharAttr::Invert) {
			a = A_REVERSE;
		}
		attrset(a);
	};
	for (int y = 0; y < m->height; y++) {
		if (line_flags()->at(y) & LineChanged) {
			Character const *line = &screen()->at(m->width * y);
			std::vector<char> vec;
			vec.reserve(m->width * 3);
			int x = 0;
			int erase_x = 0;
			int erase = 0;
			auto Erase = [&](){
				if (erase > 0) {
					SetAttribute(CharAttr());
					std::vector<char> vec;
					vec.reserve(erase + 1);
					vec.assign(erase, ' ');
					vec.push_back(0);
					move(y, erase_x);
					waddnstr(stdscr, &vec[0], erase);
				}
				erase = 0;
			};
			while (x < m->width) {
				CharAttr attr;
				int n = 0;
				QString text;
				while (x + n < m->width) {
					uint32_t c = line[x + n].c;
					uint32_t d = 0;
					if (c == 0 || c == 0xffff) {
						break;
					}
					if ((c & 0xfc00) == 0xdc00) {
						// surrogate 2nd
						break;
					}
					uint32_t unicode = c;
					if ((c & 0xfc00) == 0xd800) {
						// surrogate 1st
						if (x + n + 1 < m->width) {
							uint16_t t = line[x + n + 1].c;
							if ((t & 0xfc00) == 0xdc00) {
								d = t;
								unicode = (((c & 0x03c0) + 0x0040) << 10) | ((c & 0x003f) << 10) | (d & 0x03ff);
							} else {
								break;
							}
						} else {
							break;
						}
					}
					int cw = charWidth(unicode);
					if (cw < 1) break;
					if (n == 0) {
						attr = line[x + n].a;
					} else {
						if (attr != line[x + n].a) {
							break;
						}
					}
					if (d == 0) {
						text.push_back(c);
					} else {
						text.push_back(c);
						text.push_back(d);
					}
					n += cw;
				}
				if (n > 0) {
					Erase();
					if (!text.isEmpty()) {
						SetAttribute(attr);
						std::string s = text.toStdString();
						move(y, x);
						waddnstr(stdscr, s.c_str(), s.size());
					}
					x += n;
					erase_x = x;
					attr = CharAttr();
				} else {
					erase++;
					x++;
				}
			}
			Erase();
			line_flags()->at(y) &= ~LineChanged;
		}
	}
}

void CursesOreApplication::updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll)
{
	(void)auto_scroll;

	if (ensure_current_line_visible) {
		ensureCurrentLineVisible();
	}

//	clearParsedLine();
	updateCursorPos(true);

	int x = cx()->viewport_org_x + cursorX();
	int y = cx()->viewport_org_y + cursorY();

	if (change_col) {
		cx()->current_col_hint = cx()->current_col;
	}

	if (isPaintingSuppressed()) {
		return;
	}

	preparePaintScreen();

	paintScreen();

	move(y, x);
}



void CursesOreApplication::print_invert_text(int y, const char *text)
{
	attron(A_REVERSE);
	int n = strlen(text);
	std::vector<char> buf(m->width + 1);
	if (n > m->width) n = m->width;
	memcpy(&buf[0], text, n);
	while (n < m->width) {
		buf[n] = ' ';
		n++;
	}
	buf[n] = 0;
	AbstractCharacterBasedApplication::Option opt;
	print(0, y, &buf[0], opt);
}

void CursesOreApplication::print_status_text(const char *text)
{
	print_invert_text(m->height - 1, text);
}

int CursesOreApplication::main2()
{
	signal(SIGINT, siginthandler);

	initEditor();
	cx()->engine = TextEditorEnginePtr(new TextEditorEngine);

	setlocale(LC_ALL, "");
	initscr();
	if (0) {
		start_color();
		init_pair(1, COLOR_BLACK, COLOR_WHITE);
	}
	raw();
	nonl();
	cbreak();
	noecho();
	idlok(stdscr, false);
	scrollok(stdscr, false);
	keypad(stdscr, false);
	m->width = getmaxx(stdscr);
	m->height = getmaxy(stdscr);

	makeBuffer();
	loadExampleFile();
	updateVisibility(true, true, false);

	std::vector<uint8_t> esc;
	while (1) {
		refresh();
		timeout(-1);
		int c = wgetch(stdscr);
		if (c == 0x1b) {
			timeout(5);
			while (1) {
				int d = wgetch(stdscr);
				if (d == ERR) break;
				if (esc.empty()) {
					esc.push_back(c);
				}
				esc.push_back(d);
			}
		}
		if (!esc.empty()) {
			c = 0;
			if (esc.size() == 3) {
				int e = (esc[0] << 24) | (esc[1] << 16) | (esc[2] << 8);
				switch (e) {
				case EscapeCode::Up:
				case EscapeCode::Down:
				case EscapeCode::Right:
				case EscapeCode::Left:
				case EscapeCode::Home:
				case EscapeCode::End:
					c = e;
					break;
				}
			} else if (esc.size() == 4) {
				int e = (esc[0] << 24) | (esc[1] << 16) | (esc[2] << 8) | esc[3];
				switch (e) {
				case EscapeCode::Insert:
				case EscapeCode::Delete:
				case EscapeCode::PageUp:
				case EscapeCode::PageDown:
					c = e;
					break;
				}
			}
			esc.clear();
		} else {
			{
				if (c == 0x7f) {
					doBackspace();
					c = 0;
				} else if (c == 0x0d) {
					c = '\n';
				}
			}
		}
		if (c) {
			write(c, true);
		}
		if (state() == State::Exit) {
			break;
		}
	}
	endwin();
	return 0;
}

int main2()
{
	CursesOreApplication app;
	app.main2();
	return 0;
}

#else

int main2()
{
	return 0;
}

#endif
