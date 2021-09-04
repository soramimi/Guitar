
#include "MainWindow.h"
#include "MySettings.h"
#include "ui_MainWindow.h"

#include <QDebug>
#include <QKeyEvent>
#include <QPainter>
#include <QTextFormat>
#include <QFileDialog>
#include <QTimer>
#include <QStyle>


struct MainWindow::Private {
	bool need_to_layout;
	QRect cursor_rect;
	TextEditorEnginePtr engine;
	QString last_used_file;
	QTimer tm;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);
	auto flags = windowFlags();
	flags &= ~Qt::WindowMaximizeButtonHint;
	flags |= Qt::MSWindowsFixedSizeDialogHint;
	setWindowFlags(flags);
	m->need_to_layout = true;

	texteditor()->setTheme(TextEditorTheme::Dark());

//	setFont(texteditor()->font());

	m->engine = TextEditorEnginePtr(new TextEditorEngine);
	texteditor()->setTextEditorEngine(m->engine);

	if (1) {
		texteditor()->setNormalTextEditorMode(true);
	} else {
		texteditor()->setTerminalMode(true);
	}

//	texteditor()->setReadOnly(true);

//	texteditor()->setRenderingMode(TextEditorView::CharacterMode);

	texteditor()->setWriteMode(AbstractCharacterBasedApplication::WriteMode::Insert);

	texteditor()->loadExampleFile();

	connect(&m->tm, SIGNAL(timeout()), this, SLOT(updateIm()));
}

MainWindow::~MainWindow()
{
	delete m;
	delete ui;
}

TextEditorView *MainWindow::texteditor()
{
	return ui->widget->view();
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
	texteditor()->write(e);

	if (texteditor()->state() == TextEditorView::State::Exit) {
		close();
	}
}

bool MainWindow::event(QEvent *e)
{
	bool r = QMainWindow::event(e);
	if (e->type() == QEvent::WindowActivate) {
		if (m->need_to_layout) {
			m->need_to_layout = false;
			int w = texteditor()->basisCharWidth() * texteditor()->screenWidth();
			int h = texteditor()->lineHeight() * texteditor()->screenHeight();
			int sb = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
			w += sb;
			h += sb;
			h += ui->menuBar->height();
			setFixedSize(w, h);
		}
	}
	return r;
}

TextEditorEnginePtr MainWindow::engine()
{
	return m->engine;
}

Document *MainWindow::document()
{
	return &engine()->document;
}

void MainWindow::upArrow()
{
	texteditor()->moveCursorUp();
}

void MainWindow::downArrow()
{
	texteditor()->moveCursorDown();
}

void MainWindow::leftArrow()
{
	texteditor()->moveCursorLeft();
}

void MainWindow::rightArrow()
{
	texteditor()->moveCursorRight();
}

void MainWindow::on_verticalScrollBar_valueChanged(int /*value*/)
{
	texteditor()->refrectScrollBar();
}

void MainWindow::on_horizontalScrollBar_valueChanged(int /*value*/)
{
	texteditor()->refrectScrollBar();
}

void MainWindow::on_action_file_open_triggered()
{
	QString path;
	{
		MySettings s;
		s.beginGroup("File");
		path = s.value("LastUsedFile").toString();
	}
	path = QFileDialog::getOpenFileName(this, tr("Open"), path);
	if (!path.isEmpty()) {
		{
			MySettings s;
			s.beginGroup("File");
			s.setValue("LastUsedFile", path);
		}
		texteditor()->openFile(path);
	}
}

void MainWindow::on_action_file_save_triggered()
{
	texteditor()->saveFile("/tmp/test.txt");
}

void MainWindow::updateIm()
{
	QApplication::inputMethod()->update(Qt::ImCursorRectangle);
}

void MainWindow::moveEvent(QMoveEvent *)
{
	m->tm.stop();
	m->tm.setSingleShot(true);
	m->tm.start(100);
}

void MainWindow::on_action_test_triggered()
{
	std::string cmd = "\x1b[36mHello, \x1b[34mworld\n";
	texteditor()->write(cmd);

}
