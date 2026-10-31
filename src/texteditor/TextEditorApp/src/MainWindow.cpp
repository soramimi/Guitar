
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
#include <QStatusBar>
#include <memory>
#include <QShortcut>

struct MainWindow::Private {
	MainWindow::Mode mode = MainWindow::Mode::TextEditor;
	bool need_to_layout;
	QRect cursor_rect;
	TextEditorEngine_sp engine;
	TextEditorEngine_sp engine2;
	QString last_used_file;
	QTimer tm;
	PtyProcessController process_controller;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);
	m->need_to_layout = true;

	ui->splitter->setSizes({600, 400});
	
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

	connect(&m->process_controller, &PtyProcessController::outputReady, this,
		[this](QByteArray const &output){
			writeToTerminal(std::string_view(
				output.constData(), static_cast<size_t>(output.size())));
		});
	connect(&m->process_controller, &PtyProcessController::completed, this,
		[this](int exit_code, QString const &error_message){
			if (exit_code == 0 && error_message.isEmpty()) {
				statusBar()->showMessage(tr("端末プロセスが終了しました"), 3000);
				return;
			}
			QString const message = error_message.isEmpty()
				? tr("端末プロセスが異常終了しました: exit code %1").arg(exit_code)
				: tr("端末プロセスの実行に失敗しました: %1").arg(error_message);
			statusBar()->showMessage(message, 10000);
		});
	connect(terminal(), &TextEditorView::terminalInputRequested,
		&m->process_controller, &PtyProcessController::writeInput);
	connect(terminal(), &TextEditorView::terminalSizeChanged,
		&m->process_controller, &PtyProcessController::resizeTerminal);
	
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

TextEditorView *MainWindow::target_widget()
{
	if (m->mode == Mode::TextEditor) {
		return texteditor();
	} else {
		return terminal();
	}
}

bool MainWindow::event(QEvent *e)
{
	bool r = QMainWindow::event(e);
	if (e->type() == QEvent::WindowActivate) {
		if (m->need_to_layout) {
			m->need_to_layout = false;
			// int w = target_widget()->basisCharWidth() * target_widget()->screenWidth();
			// int h = target_widget()->lineHeight() * target_widget()->screenHeight();
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
	target_widget()->moveCursorUp();
}

void MainWindow::downArrow()
{
	target_widget()->moveCursorDown();
}

void MainWindow::leftArrow()
{
	target_widget()->moveCursorLeft();
}

void MainWindow::rightArrow()
{
	target_widget()->moveCursorRight();
}

void MainWindow::on_verticalScrollBar_valueChanged(int /*value*/)
{
	target_widget()->reflectScrollBar();
}

void MainWindow::on_horizontalScrollBar_valueChanged(int /*value*/)
{
	target_widget()->reflectScrollBar();
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
		if (target_widget()->openFile(path, &error_message)) {
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
	if (!target_widget()->saveFile(path, &error_message)) {
		QMessageBox::warning(this, tr("Save"), tr("Failed to save file:\n%1\n\n%2").arg(path, error_message));
		return false;
	}
	return true;
}

void MainWindow::on_action_file_save_as_triggered()
{
	QString path = target_widget()->recentlyUsedPath();
	path = QFileDialog::getSaveFileName(this, tr("Save as"), path);
	if (!path.isEmpty() && saveFile(path)) {
		{
			MySettings s;
			s.beginGroup("File");
			s.setValue("LastUsedFile", path);
		}
		target_widget()->setRecentlyUsedPath(path);
	}
}
void MainWindow::on_action_edit_cut_triggered()
{
	qDebug() << Q_FUNC_INFO;
	target_widget()->edit_cut();
}


void MainWindow::on_action_edit_copy_triggered()
{
	qDebug() << Q_FUNC_INFO;
	target_widget()->edit_copy();
}


void MainWindow::on_action_edit_paste_triggered()
{
	qDebug() << Q_FUNC_INFO;
	target_widget()->edit_paste();
}

void MainWindow::writeToTerminal(const std::string_view &str)
{
	bool const was_viewing_bottom = terminal()->isViewingBottom();
	terminal()->appendBulk(str);
	terminal()->setChanged(false);
	if (was_viewing_bottom && terminal()->terminalOutputScrolled()
		&& !terminal()->terminalAlternateScreenActive()) {
		terminal()->moveToBottom();
	}
}




void MainWindow::on_action_debug_triggered()
{
	// A deterministic, configuration-free interactive shell for terminal testing.
#ifdef Q_OS_WIN
	QString const program = "C:\\Program Files\\Git\\bin\\bash.exe";
	QStringList const arguments = {"--noprofile", "--norc", "-i"};
#else
	QString const program = "/usr/bin/bash";
	QStringList const arguments = {"--noprofile", "--norc", "-i"};
#endif

	if (m->process_controller.isRunning()) {
		statusBar()->showMessage(tr("端末プロセスはすでに実行中です"), 3000);
		return;
	}

	QString const executable = QStandardPaths::findExecutable(program);
	if (executable.isEmpty()) {
		statusBar()->showMessage(tr("実行ファイルが見つかりません: %1").arg(program), 10000);
		return;
	}

	QSize const terminal_size = terminal()->terminalSizeInCells();
	// Each run owns a fresh terminal screen and parser.  Host status is kept out
	// of the terminal document so it cannot be mistaken for child output.
	terminal()->clear();
	terminal()->setTerminalScreenSize(terminal_size.width(), terminal_size.height());
	if (!m->process_controller.start(executable, arguments, terminal_size)) {
		statusBar()->showMessage(tr("端末プロセスを起動できませんでした: %1").arg(program), 10000);
	}
}
