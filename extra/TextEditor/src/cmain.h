#ifndef CMAIN_H
#define CMAIN_H

#include "texteditor/AbstractCharacterBasedApplication.h"

class CursesOreApplication : public AbstractCharacterBasedApplication {
public:

	struct Private;
	Private *m;

	CursesOreApplication();
	~CursesOreApplication();

	void print_invert_text(int y, char const *text);


	void print_status_text(char const *text);

	int main2();

protected:
	int screenWidth() const;
	int screenHeight() const;
private:
	void clearLine(int y);
	void paintScreen();

protected:
	void updateVisibility(bool ensure_current_line_visible, bool change_col, bool auto_scroll);
};

int main2();

#endif // CMAIN_H
