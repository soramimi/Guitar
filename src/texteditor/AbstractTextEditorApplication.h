#ifndef ABSTRACTTEXTEDITORAPPLICATION_H
#define ABSTRACTTEXTEDITORAPPLICATION_H

#include <QByteArray>
#include <QColor>
#include <QKeyEvent>
#include <QRect>
#include <QString>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>
#include <mutex>
#include <QFont>
#include <QPixmap>
#include <QPainter>
#include <QFontMetrics>

#include "LineIndexMap/LineIndexMap.h"

namespace EscapeCode {
enum EscapeCode {
	Up = 0x1b5b4100,
	Down = 0x1b5b4200,
	Right = 0x1b5b4300,
	Left = 0x1b5b4400,
	Home = 0x1b4f4800,
	End = 0x1b4f4600,
	Insert = 0x1b5b327e,
	Delete = 0x1b5b337e,
	PageUp = 0x1b5b357e,
	PageDown = 0x1b5b367e,
};
}

struct CharAttr {
	enum Index {
		Normal,
		Invert,
		Hilite,
	};
	uint16_t index = 0; // テーマ側で解釈する属性番号
	QColor color;       // 属性番号では表せない明示色（未指定なら無効色）
	CharAttr(int index = Normal)
		: index(index)
	{
	}
	bool operator == (CharAttr const &r) const
	{
		return index == r.index && color == r.color;
	}
	bool operator != (CharAttr const &r) const
	{
		return !operator == (r);
	}
};

struct CharFlags {
	enum DiffMarker {
		Undefined,
		Del,
		Add,
		_Reserved
	};
	union {
		struct {
			bool selected : 1;       // 選択範囲内の文字
			bool current_line : 1;   // カーソルが属する論理行の文字
			uint8_t diff_marker : 2; // 文字単位diffの種別
			
		};
		uint16_t all = 0;
	};
};

struct Character {
	char32_t unicode = 0; // Unicodeコードポイント。UTF-16のcode unitではない
	int left_x = 0;       // 行頭を0とする文字左端のピクセル座標
	int right_x = 0;      // 行頭を0とする文字右端のピクセル座標
	CharAttr attr;        // 描画属性
	Character() = default;
	Character(char32_t unicode)
		: unicode(unicode)
	{
	}
	operator char32_t () const
	{
		return unicode;
	}
};
class CharBuffer {
public:
	// コピー時にCharacter列を複製せず共有する。編集目的でコピーした場合も同じ
	// vectorを指すため、呼び出し側は値型の深いコピーと誤認しないこと。
	std::shared_ptr<std::vector<Character>> vec;
	CharBuffer()
		: vec(std::make_shared<std::vector<Character>>())
	{
	}
	CharBuffer(std::vector<Character> const &v)
		: vec(std::make_shared<std::vector<Character>>(v))
	{
	}
	void reserve(size_t n)
	{
		vec->reserve(n);
	}
	void clear()
	{
		vec->clear();
	}
	void resize(size_t n)
	{
		vec->resize(n);
	}
	size_t size() const
	{
		return vec->size();
	}
	bool empty() const
	{
		return vec->empty();
	}
	Character const &back() const
	{
		return vec->back();
	}
	Character &operator [] (size_t i)
	{
		return (*vec)[i];
	}
	Character const &operator [] (size_t i) const
	{
		return (*vec)[i];
	}
	void push_back(Character const &c)
	{
		vec->push_back(c);
	}
	void emplace_back(Character const &c)
	{
		vec->emplace_back(c);
	}
	std::vector<Character>::iterator begin()
	{
		return vec->begin();
	}
	std::vector<Character> ::iterator end()
	{
		return vec->end();
	}
	std::vector<Character>::const_iterator begin() const
	{
		return vec->begin();
	}
	std::vector<Character> ::const_iterator end() const
	{
		return vec->end();
	}
	void insert(std::vector<Character>::iterator pos, Character const &first)
	{
		vec->insert(pos, first);
	}
	void insert(std::vector<Character>::iterator pos, Character const *first, Character const *last)
	{
		vec->insert(pos, first, last);
	}
	void insert(std::vector<Character>::iterator pos, std::vector<Character>::const_iterator first, std::vector<Character>::const_iterator last)
	{
		vec->insert(pos, first, last);
	}
	void erase(std::vector<Character>::iterator pos)
	{
		vec->erase(pos);
	}
	void erase(std::vector<Character>::iterator first, std::vector<Character>::iterator last)
	{
		vec->erase(first, last);
	}
};

typedef int32_t row_index_t;
typedef int32_t col_index_t;

class Document {
public:
	// vector<char>は編集可能な所有データ、string_viewはDocument::allなどを参照する
	// 読み取り専用ビュー。編集時はLine::to_vector()で所有データへ昇格する。
	typedef std::variant<std::vector<char>, std::string_view> varline_t;
	
	// UTF-8をデコードし、フォントメトリクスで位置計算した結果。
	// 論理行と折り返し断片の双方で使うが、それぞれのLineが個別に所有する。
	struct LineProperty {
		CharBuffer chars;             // デコード済み文字と各文字のX座標
		std::vector<CharFlags> flags; // charsと同じ添字で参照する描画フラグ
		bool char_diff = false;       // 文字単位diff情報を保持しているか
		// charsを再利用できる入力テキストとフォントメトリクスの世代。
		uint64_t text_revision = 0;
		uint64_t metrics_revision = 0;
	};
	
	enum LineType {
		Invalid,
		Normal,
		Add,
		Del,
	};
	struct Line {
		struct Meta {
			LineType type = Normal;              // 通常行、diff追加行、削除行などの種別
			uint64_t text_revision = 1;          // text変更時に増加する解析キャッシュ世代
			col_index_t logical_col_pos = 0;     // 折り返し断片の論理行内開始列
			col_index_t logical_col_len = 0;     // 折り返し断片が受け持つコードポイント数
			int32_t line_number_override = -1;   // 0以上なら通常の論理行番号より優先
			mutable std::shared_ptr<LineProperty> detail; // デコード・文字位置計算キャッシュ
			// 論理行だけが使用する折り返し結果。各要素は1表示行に対応する。
			// 表示行の平坦なコピーは持たず、LineIndexMapのwrap_indexで参照する。
			mutable std::vector<Document::Line> visual_lines;
		};
		struct D {
			varline_t text = std::string_view(); // 改行コードを含み得るUTF-8列
			Meta meta;                           // textから導出される情報と表示属性
		};
		// LineのコピーはDを共有する浅いコピー。折り返し断片のvector自体は
		// 別Lineだが、Lineをコンテナ間でコピーすると同じDを参照する。
		std::shared_ptr<D> sp;
		
		Line()
			: sp(std::make_shared<D>())
		{
		}
		
		explicit Line(std::vector<char> const &ba, LineType type = Normal)
			: sp(std::make_shared<D>())
		{
			sp->text = ba;
			sp->meta.type = type;
		}
		
		explicit Line(QByteArray const &ba, LineType type = Normal)
			: sp(std::make_shared<D>())
		{
			sp->text = std::vector<char>(ba.data(), ba.data() + ba.size());
			sp->meta.type = type;
		}
	
		static Line InvalidLine()
		{
			Line line;
			line.sp->meta.type = Invalid;
			return line;
		}
		
		static Line NormalEmptyLine()
		{
			Line line;
			line.sp->meta.type = Normal;
			return line;
		}
		
		static Line None()
		{
			Line line;
			line.sp->meta.type = Invalid;
			return line;
		}
		
		static Line View(std::string_view v, LineType type)
		{
			Line line;
			line.sp->text = v;
			line.sp->meta.type = type;
			return line;
		}
		
		static Line View(std::string_view v, Meta const &meta)
		{
			Line line;
			line.sp->text = v;
			line.sp->meta = meta;
			return line;
		}
		
		static Line View(std::string_view v)
		{
			return View(v, {});
		}
		
		static Line View(Line const &line)
		{
			if (std::holds_alternative<std::string_view>(line.sp->text)) {
				return line;
			}
			std::vector<char> const *v = std::get_if<std::vector<char>>(&line.sp->text);
			assert(v);
			return View(std::string_view(v->data(), v->size()), line.sp->meta);
		}

		// テキストと表示属性を複製し、解析・折り返しキャッシュは引き継がない。
		// 外部Documentの取り込みや、同じDを参照する論理行の並列更新前に使う。
		Line detached_copy() const
		{
			std::vector<char> owned_text;
			std::string_view source_text = text();
			if (!source_text.empty()) {
				owned_text.assign(source_text.begin(), source_text.end());
			}
			Line line(owned_text, sp->meta.type);
			line.sp->meta.text_revision = sp->meta.text_revision;
			line.sp->meta.logical_col_pos = sp->meta.logical_col_pos;
			line.sp->meta.logical_col_len = sp->meta.logical_col_len;
			line.sp->meta.line_number_override = sp->meta.line_number_override;
			return line;
		}

		void detach_if_shared()
		{
			if (!sp.unique()) {
				*this = detached_copy();
			}
		}
		
		LineType type() const
		{
			return sp->meta.type;
		}
		
		void set_line_number_override(int32_t num)
		{
			sp->meta.line_number_override = num;
		}
		
		LineProperty *detail() const
		{
			return sp->meta.detail.get();
		}
		
		LineProperty *new_detail()
		{
			sp->meta.detail = std::make_shared<LineProperty>();
			return detail();
		}
		
		void clear_detail()
		{
			sp->meta.detail.reset();
		}
		
		bool ends_with_new_line() const
		{
			int c = text().empty() ? 0 : text().back();
			return c == '\n' || c == '\r';
		}
		
		std::string_view text() const
		{
			if (std::holds_alternative<std::string_view>(sp->text)) {
				return std::get<std::string_view>(sp->text);
			}
			std::vector<char> const *v = std::get_if<std::vector<char>>(&sp->text);
			assert(v);
			return std::string_view(v->data(), v->size());
		}
		
		void set_text(std::vector<char> const &text)
		{
			sp->text = text;
			sp->meta.text_revision++;
			sp->meta.detail.reset();
		}
		
		std::vector<char> *to_vector()
		{
			if (std::string_view *sv = std::get_if<std::string_view>(&sp->text)) {
				sp->text = std::vector<char>(sv->data(), sv->data() + sv->size());
			}
			std::vector<char> *v = std::get_if<std::vector<char>>(&sp->text);
			assert(v);
			return v;
		}
		
		void append_text(std::string_view new_text)
		{
			if (!new_text.empty()) {
				std::vector<char> *v = to_vector();
				v->insert(v->end(), new_text.data(), new_text.data() + new_text.size());
				sp->meta.text_revision++;
				sp->meta.detail.reset();
				sp->meta.visual_lines.clear();
			}
		}
		
		void append_text(const std::vector<char> &new_text)
		{
			if (!new_text.empty()) {
				append_text(std::string_view(new_text.data(), new_text.size()));
			}
		}
		
		void append_text(char c)
		{
			append_text(std::string_view(&c, 1));
		}
		
		void clear_visual_lines()
		{
			sp->meta.visual_lines.clear();
		}
	};
	
	
	QByteArray all;                    // openFile()で読み込んだファイル全体の所有領域
	std::vector<varline_t> raw_lines;  // allを行単位に分けたビュー（互換・保持用）

	// 文書の唯一の正本。折り返し断片やLineIndexMapはここから再生成できる。
	std::vector<Line> logical_lines;
};

class TextEditorEngine {
public:
	Document document;
};

struct LogicalRowInfo {
	row_index_t visual_row = 0;
};

struct VisualRowInfo {
	row_index_t lrow = 0;
	col_index_t lcol = 0;
};

struct SelectionAnchor {
	bool enabled = false; // falseならlrow/lcolは選択端点として無効
	row_index_t lrow = 0; // 論理行番号
	col_index_t lcol = 0; // 論理行内のコードポイント位置
	explicit operator bool () const
	{
		return enabled;
	}
	int compare(SelectionAnchor const &a) const
	{
		if (enabled && a.enabled) {
			if (lrow < a.lrow) return -1;
			if (lrow > a.lrow) return 1;
			if (lcol < a.lcol) return -1;
			if (lcol > a.lcol) return 1;
		} else {
			if (a.enabled) return -1;
			if (enabled) return 1;
		}
		return 0;
	}
};
static inline bool operator == (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) == 0; }
static inline bool operator != (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) != 0; }
static inline bool operator <= (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) <= 0; }
static inline bool operator >= (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) >= 0; }
static inline bool operator < (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) < 0; }
static inline bool operator > (SelectionAnchor const &a, SelectionAnchor const &b) { return a.compare(b) > 0; }

using TextEditorEngine_sp = std::shared_ptr<TextEditorEngine>;

struct TextEditorContext {
	QRect cursor_rect; // IMEへ通知する、ウィジェット座標系のカーソル矩形
	row_index_t current_visual_row = 0; // 表示行（物理行）
	col_index_t current_visual_col = 0; // 表示列（物理列）
	int current_visual_col_hint = 0; // 上下移動時に維持したい表示列
	int current_absolute_x_px = 0; // 桁ピクセル座標（行頭基準）
	int current_visual_x_px = 0; // 桁ピクセル座標（クライアント領域基準）
	int current_visual_y_px = 0; // 行ピクセル座標
	row_index_t saved_row = 0; // terminal modeなどで一時退避する表示行
	col_index_t saved_col = 0; // 同上の表示列
	int saved_col_hint = 0;    // 同上の列ヒント
	int current_char_span = 1; // 現在文字が占める表示セル数
	int scroll_horz_pos_px = 0; // 水平スクロール量（ピクセル）
	int scroll_vert_pos_px = 0; // 垂直スクロール量（実態は表示行数）
	col_index_t viewport_org_x_cols = 0; // テキスト領域の原点（桁位置）（行番号表示領域の幅の文字数）
	row_index_t viewport_org_y_rows = 0; // ビューポート上端の行オフセット
	int viewport_width_px = 640;         // ビューポート幅（ピクセル）
	int viewport_height_rows = 25;       // ビューポートに入る表示行数
	int tab_indent_size = 4;             // タブストップ間隔
	int bottom_line_y = -1;              // 最終描画行のY位置（未設定は-1）
	TextEditorEngine_sp engine;           // Documentを共有するエンジン
	// 論理行ごとの折り返し数・各断片長を保持し、論理座標と表示座標を変換する。
	// wrapping時のvisual_lines参照は、この索引と常に同じ世代でなければならない。
	LineIndexMap line_index_map;
	struct Cache {
		std::optional<row_index_t> nlines; // 総表示行数。nulloptならLineIndexMapから再取得
		row_index_t current_logical_row = 0; // 現在の表示座標を変換した一時結果
		col_index_t current_logical_col = 0; // 同上の論理列
		bool scroll_bar_update_needed = true; // 次回の表示更新でrangeを再設定する
	};
	mutable Cache cache;
};

struct RowCol {
	row_index_t row = 0;
	col_index_t col = 0;
	RowCol(row_index_t row = 0, col_index_t col = 0)
		: row(row)
		, col(col)
	{
	}
};

class AbstractTextEditorApplication {
public:
	class Font {
	public:
		struct TextWidthCache {
			// 同じ文字列のhorizontalAdvance()呼び出しを抑える。
			// フォント変更時には必ず全消去する。
			std::unordered_map<QString, int> map;
		};
		QFont text_font_;                         // 計測対象フォント
		std::unique_ptr<QFontMetrics> fm_;         // text_font_に対応するメトリクス
		int ascent_ = 0;                           // ベースラインより上の高さ
		int descent_ = 0;                          // ベースラインより下の高さ
		QSize basic_character_size_;               // 基準文字"0"の幅とフォントの高さ
		mutable TextWidthCache text_width_cache_;   // 文字列単位の幅キャッシュ

		void set_font(QFont const &font)
		{
			text_font_ = font;
			text_width_cache_.map.clear();

			QPixmap pm(1, 1);
			QPainter pr(&pm);
			pr.setFont(text_font_);
			fm_ = std::make_unique<QFontMetrics>(pr.fontMetrics());
			ascent_ = fm_->ascent();
			descent_ = fm_->descent();
			basic_character_size_ = QSize(fm_->horizontalAdvance("0"), fm_->height());
		}
		QFont font() const
		{
			return text_font_;
		}
		int basis_char_width() const
		{
			int w = basic_character_size_.width();
			return w > 0 ? w : 1;
		}
		int basis_char_height() const
		{
			int h = basic_character_size_.height();
			return h > 0 ? h : 1;
		}
		int text_width(QString const &text) const
		{
			int ret = 0;
			auto it = text_width_cache_.map.find(text);
			if (it != text_width_cache_.map.end()) {
				ret = it->second;
			} else {
				ret = fm_->horizontalAdvance(text);
				text_width_cache_.map[text] = ret;
			}
			return ret;
		}
	};

	static const int LINE_NUMBER_AREA_WIDTH = 8;
	
	enum class WriteMode {
		Insert,
		Overwrite,
	};
	
	enum class State {
		Normal,
		Exit,
	};
	
	enum class WrappingMode {
		NoWrap,
		CharWrap,
		WordWrap,
	};
	
	struct Option {
		CharAttr char_attr = {};
		CharFlags char_flag = {};
		QRect clip;
	};
	
	struct Char16 {
		uint16_t c = 0;
		CharAttr a;
	};
	
	static int charWidth(uint32_t c);
	
	class FormattedLine {
	public:
		QString text;
		enum Attr {
			StyleID = 0x00ffffff,
			Selected = 0x01000000,
		};
		uint32_t atts;
		FormattedLine(QString const &text, int atts)
			: text(text)
			, atts(atts)
		{
		}
		bool isSelected() const
		{
			return atts & Selected;
		}
	};
	
private:
	// 実装詳細と状態を隠すPimpl。所有権は本クラスにありデストラクタで破棄する。
	struct Private;
	Private *m;
protected:
	SelectionAnchor const &selection_start() const;
	SelectionAnchor const &selection_end() const;
	void set_selection_start(SelectionAnchor const &anchor);
	void set_selection_end(SelectionAnchor const &anchor);
	void set_selection_start_enabled(bool enabled);
	void set_selection_end_enabled(bool enabled);
	void sync_selection();
	void clear_selection();
protected:
	// 総表示行数。NoWrapでは論理行数、wrapping時はLineIndexMapの値の総和。
	row_index_t visual_nlines() const;
	void invalidate_nlines_cache();

	// 表示行をLineIndexMapで(logical row, wrap index)へ変換して取得する。
	// 戻り値はlogical_lines内または論理行のvisual_lines内を指す非所有ポインタ。
	Document::Line *visual_line(row_index_t vrow);
	
	Document::Line const *visual_line(row_index_t vrow) const
	{
		return const_cast<AbstractTextEditorApplication *>(this)->visual_line(vrow);
	}
	
	void initEditor();
protected:
	const Document::Line *currentLine() const;
	
	void set_current_visual_row(row_index_t row);
	void set_current_visual_col(col_index_t col);
	row_index_t current_visual_row() const;
	int current_visual_col() const;
	int current_visual_x_px() const;
	
	int scroll_vert_pos_px() const;
	int scroll_horz_pos_px() const;

	int cursor_col_px() const;
	int cursor_row_px() const;

	void set_scroll_vert_pos_px(int row);
	void set_scroll_horz_pos_px(int col);
	
	int editor_viewport_width_px() const;
	int editor_viewport_height() const;
	
	// カーソル、スクロール、Document、LineIndexMapをまとめた実行時コンテキスト。
	// TextEditorViewなど派生クラスも描画時に参照する。
	std::shared_ptr<TextEditorContext> editor_cx;
	
	TextEditorContext *cx();
	TextEditorContext const *cx() const;
	
	Document *document();
	Document const *document() const;
	int logical_nlines() const;
	
	void ensure_current_line_visible();
	
	int leftMargin_() const;
	
	struct UpdateVisibilityOption {
		bool ensure_current_line_visible = true;
		bool change_col = true;
		bool auto_scroll = true;
	};
	virtual void updateVisibility(UpdateVisibilityOption const &arg) = 0;
	
	void insert_line(row_index_t lrow); // DocumentとLineIndexMapへ同じ位置を挿入する
	bool commit_line(row_index_t lrow, const CharBuffer &vec); // 変更行だけを再解析・再折り返しする
	
	void doDelete();
	void doBackspace();
	
	void invalidate_visual_row_info(row_index_t vrow, size_t n = -1);
	void invalidate_logical_row_info(row_index_t vrow);
	void invalidate_visual_line_details(row_index_t vrow, size_t n = -1);

	LineIndexMap::LogicalPosition query_logical_for_visual_row(row_index_t vrow) const;

	virtual void calc_pos_x(CharBuffer *chars) const {}
	
private:
	void internalWrite(const ushort *begin, const ushort *end);
	SelectionAnchor currentAnchor(bool enabled) const;
	enum class EditOperation {
		Cut,
		Copy,
	};
	std::optional<CharBuffer> edit_selection(EditOperation op);
	int calcColumnToIndex(int column);
	void _edit_op(EditOperation op);
	bool is_current_line_writable() const;
	void initEngine(const std::shared_ptr<TextEditorContext>& cx);
	void writeCR();
	bool deleteIfSelected();
	void _set_cursor_col(col_index_t col, bool auto_scroll = true, bool by_mouse = false);
	std::vector<Document::Line> *documentLinesForWrite(bool check_readonly = true);
public:
	row_index_t lrow_to_vrow(row_index_t lrow) const;
	row_index_t vrow_to_lrow(row_index_t vrow) const;
	RowCol visual_position(SelectionAnchor const &a) const;
protected:
	CharBuffer parse_logical_line(const TextEditorContext *cx, row_index_t lrow) const;
	const CharBuffer *parse_current_line() const;
private:
	CharBuffer _parse_line(const TextEditorContext *cx, const Document::Line *line, std::mutex *mutex) const;
protected:
	CharBuffer _parse_line(Document::Line const *line, std::mutex *mutex = nullptr) const;
	CharBuffer *parse_line(row_index_t vrow) const;

	virtual void updateScrollBarRange() {}
	
	virtual void setCursorRow(row_index_t vrow, bool auto_scroll = true, bool by_mouse = false);
	virtual void setCursorCol(col_index_t vcol);
	void setCursorPos(RowCol const &vpos);
	void setCursorPosByMouse(RowCol vpos, QPoint pt);
	
	static int nextTabStop(TextEditorContext const *cx, int x);
	int scrollBottomLimit() const;
	int scrollBottomLimit2() const;
	bool isPaintingSuppressed() const;
	void setPaintingSuppressed(bool f);
	
	void writeNewLine();
	
	void update_horz_scroll();
	
	QString statusLine() const;
	
	
	void paintLineNumbers(std::function<void(int, QString const &, Document::Line const *)> const &draw);
	bool isAutoLayout() const;
	void savePos();
	void restorePos();
public:
	
	AbstractTextEditorApplication();
	virtual ~AbstractTextEditorApplication();

	void setRecentlyUsedPath(QString const &path);
	QString recentlyUsedPath();
	
	virtual void layoutEditor();
	void scrollUp();
	void scrollDown();
	void moveCursorOut();
	void moveCursorHome(bool consider_indent);
	void moveCursorEnd();
	void moveCursorUp();
	virtual void moveCursorDown();
	void moveCursorLeft();
	void moveCursorRight();
	void movePageUp();
	void movePageDown();
	void scrollToTop();
	
	TextEditorEngine_sp engine() const;
	int client_width_px() const;
	int client_height_px() const;
	void set_client_size(int w, int h, bool update_layout);
	void setContentWidth(int w);
	void setTextEditorEngine(const TextEditorEngine_sp &e);
	bool openFile(QString const &path, QString *error_message = nullptr);
	bool saveFile(QString const &path, QString *error_message = nullptr);
	void loadExampleFile();
	void pressEnter();
	void pressEscape();
	State state() const;
	bool isLineNumberVisible() const;
	void showLineNumber(bool show, int left_margin = LINE_NUMBER_AREA_WIDTH);
	void set_auto_layout(bool f);
	void setDocument(const std::vector<Document::Line> *source);
	void setSelectionAnchor(bool enabled, bool update_anchor, bool auto_scroll);
	void setNormalTextEditorMode(bool f);
	void set_read_only(bool f);
	bool is_read_only() const;
	void edit_paste();
	void edit_copy();
	void edit_cut();
	void setWriteMode(WriteMode wm);
	bool isInsertMode() const;
	bool isOverwriteMode() const;
	void setTerminalMode(bool f);
	bool isTerminalMode() const;
	void moveToTop();
	void moveToBottom();
	void set_line_margin(int n);
	void write(uint32_t c, bool by_keyboard);
	void write(char const *ptr, int len, bool by_keyboard);
	void write(std::string const &text);
	void write(QKeyEvent *e);
	void setCursorVisible(bool show);
	bool isCursorVisible();
	void setModifierKeys(Qt::KeyboardModifiers const &keymod);
	bool isControlModifierPressed() const;
	bool isShiftModifierPressed() const;
	void clearShiftModifier();
	bool isChanged() const;
	void setChanged(bool f);
	void logicalMoveToBottom();
	void appendBulk(std::string_view const &str);
	void clear();
private:
	// 1論理行を現在の幅とWrappingModeで表示行へ分割する。
	// 入力行のLinePropertyが有効ならUTF-8解析と文字幅計測は再利用される。
	std::vector<Document::Line> _wrap_line(Document::Line line, std::mutex *mutex) const;

	void wrap_line(Document::Line *ll, bool force, std::mutex *mutex);
	void update_line_index_map(row_index_t lrow, Document::Line *ll, std::mutex *mutex);

	bool _update_line(row_index_t lrow, std::optional<std::vector<char>> text, bool force, std::mutex *mutex);
protected:
	bool update_visual_line(row_index_t lrow, bool force);
	// 幅・フォント・モード・文書全体の変更時に、全論理行を並列で再折り返しする。
	void update_visual_lines_all();
private:
	void _update_logical_pos_cache() const;
	void delete_line(row_index_t lrow);
protected:
	row_index_t current_logical_row() const;
	col_index_t current_logical_col() const;
protected:
	bool isWidthFixed() const;
protected:
	void write_(char const *ptr, bool by_keyboard);
	void write_(QString const &text, bool by_keyboard);
	
	bool hasSelection() const;
	void updateSelectionAnchor1(bool auto_scroll);
	void updateSelectionAnchor2(bool auto_scroll);
	virtual std::pair<int, int> currentPixelX() const { return {}; }
	
	void set_fixed_font(const QFont &font);
	void set_text_font(const QFont &font);
	Font const &fixed_font() const;
	Font const &text_font() const;
	void set_line_margin_px(int top, int bottom);	
	int line_baseline_px() const;
	void need_to_update_scroll_bar();
	int linenum_area_width_px() const;
	void new_document();
public:
	int line_height_px() const;
	
	void setWrappingMode(WrappingMode mode);
	AbstractTextEditorApplication::WrappingMode wrappingMode() const;

	bool save(std::function<bool (char const *p, size_t n)> callback) const
	{
		std::vector<Document::Line> const &llines = document()->logical_lines;
		for (Document::Line const &line : llines) {
			std::string_view view = line.text();
			if (!callback(view.data(), view.size())) {
				return false;
			}
		}
		return true;
	}
};

#endif // ABSTRACTTEXTEDITORAPPLICATION_H
