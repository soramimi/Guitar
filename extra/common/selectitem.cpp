
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#endif

// ターミナルの元の設定を保存しておくグローバル変数
#ifdef _WIN32
static DWORD g_orig_input_mode;
static DWORD g_orig_output_mode;
static bool g_input_mode_saved = false;
static bool g_output_mode_saved = false;
#else
static struct termios g_orig_termios;
static bool g_termios_saved = false;
#endif

static bool g_raw_mode_enabled = false;

/**
 * @brief ターミナルをrawモードに切り替える
 * @details rawモードではキー入力がバッファリングされず、1文字ずつ即座に読み取れる
 */
static bool enableRawMode()
{
#ifdef _WIN32
	// Windowsではコンソールモードを変更してrawモードにする
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdin == INVALID_HANDLE_VALUE || !GetConsoleMode(hStdin, &g_orig_input_mode)) {
		return false;
	}
	g_input_mode_saved = true;
	DWORD mode = g_orig_input_mode;
	mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT); // エコーと行バッファを無効化
	if (!SetConsoleMode(hStdin, mode)) {
		return false;
	}

	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdout != INVALID_HANDLE_VALUE && GetConsoleMode(hStdout, &g_orig_output_mode)) {
		g_output_mode_saved = true;
		SetConsoleMode(hStdout, g_orig_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
#else
	if (tcgetattr(STDIN_FILENO, &g_orig_termios) != 0) {
		return false;
	}
	g_termios_saved = true;
	struct termios raw = g_orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON); // エコーと行バッファを無効化
	raw.c_cc[VMIN] = 1;              // 最低1文字読み取るまでブロック
	raw.c_cc[VTIME] = 0;             // タイムアウトなし
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
		return false;
	}
#endif
	g_raw_mode_enabled = true;
	return true;
}

/**
 * @brief ターミナルの設定を元に戻す
 */
static void disableRawMode()
{
#ifdef _WIN32
	if (!g_raw_mode_enabled) {
		return;
	}
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (g_input_mode_saved && hStdin != INVALID_HANDLE_VALUE) {
		SetConsoleMode(hStdin, g_orig_input_mode); // 元の設定に戻す
	}
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (g_output_mode_saved && hStdout != INVALID_HANDLE_VALUE) {
		SetConsoleMode(hStdout, g_orig_output_mode);
	}
#else
	if (!g_raw_mode_enabled) {
		return;
	}
	if (g_termios_saved) {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
	}
#endif
	g_raw_mode_enabled = false;
}

struct TerminalModeGuard {
	explicit TerminalModeGuard(bool enabled) : enabled_(enabled) {}
	~TerminalModeGuard()
	{
		if (enabled_) {
			disableRawMode();
		}
	}

	TerminalModeGuard(const TerminalModeGuard &) = delete;
	TerminalModeGuard &operator=(const TerminalModeGuard &) = delete;

private:
	bool enabled_;
};

/**
 * @brief 端末の横幅（列数）を取得する
 * @return 端末の列数。取得できない場合は80を返す
 */
static int getTerminalWidth()
{
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (GetConsoleScreenBufferInfo(hStdout, &csbi)) {
		return csbi.srWindow.Right - csbi.srWindow.Left + 1;
	}
#else
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
		return ws.ws_col;
	}
#endif
	return 80; // デフォルト値
}

#ifdef _WIN32
/**
 * @brief ワイド文字の表示幅を計算する（Windows用の簡易実装）
 * @param wc ワイド文字
 * @return 表示幅（全角文字は2、その他は1とする）
 */
int wcwidth(wchar_t wc)
{
	// Windowsではwcwidthがないため、簡易的に全角文字を2、その他を1とする
	if ((wc >= 0x1100 && wc <= 0x115F) || // Hangul Jamo
		(wc >= 0x2E80 && wc <= 0xA4CF) || // CJK統合漢字、CJK互換漢字など
		(wc >= 0xAC00 && wc <= 0xD7A3) || // Hangul Syllables
		(wc >= 0xF900 && wc <= 0xFAFF) || // CJK互換漢字
		(wc >= 0xFE10 && wc <= 0xFE19) || // CJK互換形式
		(wc >= 0xFE30 && wc <= 0xFE6F) || // CJK互換形式
		(wc >= 0xFF00 && wc <= 0xFF60) || // 全角ASCII、全角スペースなど
		(wc >= 0xFFE0 && wc <= 0xFFE6)) { // 全角記号など
		return 2;
	}
	return 1; // その他は幅1とする（制御文字は別途処理する必要あり）
}

#endif

/**
 * @brief UTF-8文字列を端末の表示幅に収まるよう切り捨てる
 * @details マルチバイト文字の途中で切断せず、全角文字の表示幅も考慮する
 * @param src     元の文字列（UTF-8）
 * @param maxCols 最大表示列数
 * @return 切り捨て後の文字列
 */
static std::string truncateToColumns(const std::string &src, int maxCols)
{
	int cols = 0;
	size_t i = 0;
	while (i < src.size()) {
		wchar_t wc;
		int bytes = mbtowc(&wc, src.c_str() + i, src.size() - i);
		if (bytes <= 0) break; // 不正なバイト列またはヌル文字
		int w = wcwidth(wc);
		if (w < 0) w = 0; // 非表示文字
		if (cols + w > maxCols) break; // 幅を超えるなら打ち切り
		cols += w;
		i += bytes;
	}
	return src.substr(0, i);
}

/**
 * @brief アイテム一覧をターミナルに描画する
 * @details 選択中の行は反転表示（ANSIエスケープシーケンス \033[7m）する。
 *          テキストが端末幅を超える場合はマルチバイト文字を考慮して切り捨てる。
 * @param items    表示するアイテムの一覧
 * @param selected 現在選択中のインデックス（0始まり）
 */
static void drawMenu(const std::vector<std::string> &items, int selected)
{
	int width = getTerminalWidth();
	const int prefix = 4; // "%2d: " の表示幅

	for (int i = 0; i < (int)items.size(); i++) {
		// 選択行は末尾に " " が付くため1文字分多く確保が必要
		int avail = width - prefix - (i == selected ? 1 : 0);
		if (avail < 0) avail = 0;
		// UTF-8マルチバイト文字・全角文字を考慮して切り捨て
		std::string text = truncateToColumns(items[i], avail);
		if (i == selected) {
			printf("\033[7m%2d: %s \033[m\033[K\n", i + 1, text.c_str()); // 反転表示、属性リセット後に行末消去
		} else {
			printf("%2d: %s\033[K\n", i + 1, text.c_str()); // 行末まで消去して反転表示の残骸を除去
		}
	}
}

enum InputKey {
	KEY_READ_ERROR = -1,
	KEY_UP = 256,
	KEY_DOWN,
};

/**
 * @brief 1入力を読み取り、必要に応じて特殊キーを正規化する
 * @return 通常キーはその文字コード、矢印キーは InputKey、失敗時は KEY_READ_ERROR
 */
static int readInputKey()
{
#ifdef _WIN32
	int c = _getwch();
	if (c == 0 || c == 0xE0) {
		int extended = _getwch();
		if (extended == 72) return KEY_UP;
		if (extended == 80) return KEY_DOWN;
		return 0;
	}
	return c;
#else
	char c;
	if (read(STDIN_FILENO, &c, 1) != 1) return KEY_READ_ERROR;

	if (c == '\033') {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 50000;
		int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
		if (ready == 0) return '\033';
		if (ready < 0) {
			if (errno == EINTR) return 0;
			return KEY_READ_ERROR;
		}

		char seq[2];
		if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_READ_ERROR;
		if (seq[0] != '[') return 0;
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_READ_ERROR;
		if (seq[0] == '[') {
			if (seq[1] == 'A') return KEY_UP;
			if (seq[1] == 'B') return KEY_DOWN;
		}
		return 0;
	}

	return static_cast<unsigned char>(c);
#endif
}

/** * @brief アイテム選択メニューを表示してユーザーに選択させる
 * @param items 選択肢の文字列のベクター
 * @return 選択されたアイテムのインデックス（0始まり）。キャンセルされた場合は-1。
 */
int selectitem(std::vector<std::string> const &items)
{
	setlocale(LC_ALL, ""); // ロケールを環境変数に合わせる（UTF-8対応に必要）

#if 0
	// 選択対象のアイテム一覧
	std::vector<std::string> items = {
		"apple",
		"banana",
		"cherry",
		"grape",
	};
#endif

	int selected = 0;          // 現在選択中のインデックス（0始まり）
	int n = (int)items.size(); // アイテム数

	bool rawModeEnabled = enableRawMode();
	TerminalModeGuard terminalModeGuard(rawModeEnabled);
	if (!rawModeEnabled) {
		fprintf(stderr, "Failed to configure terminal mode.\n");
		return 1;
	}
	drawMenu(items, selected); // 初期表示

	while (true) {
		int key = readInputKey();
		if (key == KEY_READ_ERROR) break;

		if (key == KEY_UP) {
			if (selected > 0) selected--;
		} else if (key == KEY_DOWN) {
			if (selected < n - 1) selected++;
		} else if (key == '\r' || key == '\n') { // Enter で選択確定
			break;
		} else if (key >= '1' && key <= '9') { // 数字キーで直接選択
			int idx = key - '1';           // '1' → 0 に変換
			if (idx < n) selected = idx;
		} else if (key == '\033') { // Esc でキャンセル終了
			selected = -1;
			break;
		} else if (key == 'q' || key == 'Q') { // q でキャンセル終了
			selected = -1;
			break;
		}

		// カーソルをメニュー先頭行まで戻して再描画
		printf("\033[%dA", n);
		drawMenu(items, selected);
	}

#if 0
	printf("\n");

	if (selected >= 0) {
		printf("Selected item: %s\n", items[selected].c_str());
	} else {
		printf("Canceled\n");
	}
#endif

	return selected;
}
