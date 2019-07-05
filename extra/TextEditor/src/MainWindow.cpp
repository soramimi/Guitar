
#include "MainWindow.h"
#include "MySettings.h"
#include "ui_MainWindow.h"

#include <QDebug>
#include <QKeyEvent>
#include <QPainter>
#include <QTextFormat>
#include <QFileDialog>
#include <QTimer>


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

	ui->widget->setTheme(TextEditorTheme::Dark());

//	setFont(ui->widget->font());

	m->engine = TextEditorEnginePtr(new TextEditorEngine);
	ui->widget->setTextEditorEngine(m->engine);

	if (1) {
		ui->widget->setNormalTextEditorMode(true);
	} else {
		ui->widget->setTerminalMode(true);
	}

	ui->widget->bindScrollBar(ui->verticalScrollBar, ui->horizontalScrollBar);
//	ui->widget->setReadOnly(true);

	ui->widget->setWriteMode(AbstractCharacterBasedApplication::WriteMode::Insert);

	connect(&m->tm, SIGNAL(timeout()), this, SLOT(updateIm()));
}

MainWindow::~MainWindow()
{
	delete m;
	delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
	ui->widget->write(e);

	if (ui->widget->state() == TextEditorWidget::State::Exit) {
		close();
	}
}

bool MainWindow::event(QEvent *e)
{
	bool r = QMainWindow::event(e);
	if (e->type() == QEvent::WindowActivate) {
		if (m->need_to_layout) {
			m->need_to_layout = false;
			int w = ui->widget->latin1Width() * ui->widget->screenWidth();
			int h = ui->widget->lineHeight() * ui->widget->screenHeight();
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
	ui->widget->moveCursorUp();
}

void MainWindow::downArrow()
{
	ui->widget->moveCursorDown();
}

void MainWindow::leftArrow()
{
	ui->widget->moveCursorLeft();
}

void MainWindow::rightArrow()
{
	ui->widget->moveCursorRight();
}

void MainWindow::on_verticalScrollBar_valueChanged(int /*value*/)
{
	ui->widget->refrectScrollBar();
}

void MainWindow::on_horizontalScrollBar_valueChanged(int /*value*/)
{
	ui->widget->refrectScrollBar();
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
		ui->widget->openFile(path);
	}
}

void MainWindow::on_action_file_save_triggered()
{
	ui->widget->saveFile("/tmp/test.txt");
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
	ui->widget->write(cmd);

}
