
#include "MainWindow.h"
#include "MySettings.h"
#include "PtyProcessController.h"
#include "ui_MainWindow.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QTextFormat>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QStyle>
#include <memory>
#include <QShortcut>

struct MainWindow::Private {
	bool need_to_layout;
	QRect cursor_rect;
	TextEditorEngine_sp engine;
	TextEditorEngine_sp engine2;
	QString last_used_file;
	QTimer tm;
	PtyProcessController neofetch;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);
	m->need_to_layout = true;

	ui->splitter->setSizes({500, 500});
	
	// connect(new QShortcut(QKeySequence("Ctrl+T"), this), &QShortcut::activated, this, &MainWindow::debug);

	// setup text editor
	
	texteditor()->setTheme(TextEditorTheme::Light());

	m->engine = std::make_shared<TextEditorEngine>();
	texteditor()->setTextEditorEngine(m->engine);

	if (1) {
		texteditor()->setNormalTextEditorMode(true);
	} else {
		texteditor()->setTerminalMode(true);
	}

	texteditor()->setWriteMode(AbstractTextEditorApplication::WriteMode::Insert);

	texteditor()->set_auto_layout(true);
	texteditor()->setWrappingMode(TextEditorView::WrappingMode::NoWrap);
	// texteditor()->setWrappingMode(TextEditorView::WrappingMode::WordWrap);
	// texteditor()->setWrappingMode(TextEditorView::WrappingMode::CharWrap);

	// setup terminal
	
	terminal()->setTheme(TextEditorTheme::Light());
	m->engine2 = std::make_shared<TextEditorEngine>();
	terminal()->setTextEditorEngine(m->engine2);
	terminal()->set_auto_layout(true);
	terminal()->showLineNumber(false);
	terminal()->setTerminalMode(true);

	connect(&m->neofetch, &PtyProcessController::outputReady, this,
		[this](QByteArray const &output){
			writeToTerminal(std::string_view(
				output.constData(), static_cast<size_t>(output.size())));
		});
	connect(&m->neofetch, &PtyProcessController::completed, this,
		[this](int exit_code, QString const &error_message){
			if (exit_code == 0 && error_message.isEmpty()) return;
			QString text = error_message.isEmpty()
				? tr("[neofetchが異常終了しました: exit code %1]\n").arg(exit_code)
				: tr("[neofetchを起動できませんでした: %1]\n").arg(error_message);
			QByteArray const message = text.toUtf8();
			writeToTerminal(std::string_view(
				message.constData(), static_cast<size_t>(message.size())));
		});
	connect(terminal(), &TextEditorView::terminalSizeChanged,
		&m->neofetch, &PtyProcessController::resizeTerminal);
	
	connect(&m->tm, SIGNAL(timeout()), this, SLOT(updateIm()));
}

MainWindow::~MainWindow()
{
	delete m;
	delete ui;
}

TextEditorView *MainWindow::texteditor()
{
	return ui->widget_editor->view();
}

TextEditorView *MainWindow::terminal()
{
	return ui->widget_terminal->view();
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
			// int w = texteditor()->basisCharWidth() * texteditor()->screenWidth();
			// int h = texteditor()->lineHeight() * texteditor()->screenHeight();
			// int sb = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
			// w += sb;
			// h += sb;
			// h += ui->menuBar->height();
		}
	}
	return r;
}

TextEditorEngine_sp MainWindow::engine()
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
	texteditor()->reflectScrollBar();
}

void MainWindow::on_horizontalScrollBar_valueChanged(int /*value*/)
{
	texteditor()->reflectScrollBar();
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
		QString error_message;
		if (texteditor()->openFile(path, &error_message)) {
			MySettings s;
			s.beginGroup("File");
			s.setValue("LastUsedFile", path);
		} else {
			QMessageBox::warning(this, tr("Open"),
				tr("Failed to open file:\n%1\n\n%2").arg(path, error_message));
		}
	}
}

void MainWindow::on_action_file_save_triggered()
{
	saveFile("/tmp/test.txt");
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

bool MainWindow::saveFile(QString const &path)
{
	QString error_message;
	if (!texteditor()->saveFile(path, &error_message)) {
		QMessageBox::warning(this, tr("Save"), tr("Failed to save file:\n%1\n\n%2").arg(path, error_message));
		return false;
	}
	return true;
}

void MainWindow::on_action_file_save_as_triggered()
{
	QString path = texteditor()->recentlyUsedPath();
	path = QFileDialog::getSaveFileName(this, tr("Save as"), path);
	if (!path.isEmpty() && saveFile(path)) {
		{
			MySettings s;
			s.beginGroup("File");
			s.setValue("LastUsedFile", path);
		}
		texteditor()->setRecentlyUsedPath(path);
	}
}
void MainWindow::on_action_edit_cut_triggered()
{
	qDebug() << Q_FUNC_INFO;
	texteditor()->edit_cut();
}


void MainWindow::on_action_edit_copy_triggered()
{
	qDebug() << Q_FUNC_INFO;
	texteditor()->edit_copy();
}


void MainWindow::on_action_edit_paste_triggered()
{
	qDebug() << Q_FUNC_INFO;
	texteditor()->edit_paste();
}

void MainWindow::writeToTerminal(const std::string_view &str)
{
	terminal()->appendBulk(str);
	terminal()->setChanged(false);
	terminal()->moveToBottom();
}




void MainWindow::on_action_debug_triggered()
{
	if (m->neofetch.isRunning()) {
		writeToTerminal("[neofetchはすでに実行中です]\n");
		return;
	}

	QString const executable = QStandardPaths::findExecutable("neofetch");
	if (executable.isEmpty()) {
		writeToTerminal("[neofetchが見つかりません]\n");
		return;
	}

	writeToTerminal("$ neofetch --off --color_blocks off\n");
	if (!m->neofetch.start(executable,
		{"--off", "--color_blocks", "off"}, terminal()->terminalSizeInCells())) {
		writeToTerminal("[neofetchを起動できませんでした]\n");
	}
}

