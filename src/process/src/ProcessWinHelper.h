#ifndef PROCESSWINHELPER_H
#define PROCESSWINHELPER_H

#ifndef _WIN32
#ifndef HANDLE
#error Must include <windows.h> before this file.
#endif
#endif

namespace {

inline bool IS_VALID_HANDLE(HANDLE h)
{
	return h && h != INVALID_HANDLE_VALUE;
}

inline bool IS_VALID_HANDLE(PROCESS_INFORMATION const &h)
{
	return IS_VALID_HANDLE(h.hProcess) || IS_VALID_HANDLE(h.hThread);
}

inline bool write_all(HANDLE handle, char const *data, size_t size)
{
	if (!IS_VALID_HANDLE(handle) || (!data && size != 0)) {
		return false;
	}
	// 匿名パイプへのWriteFileは要求サイズ未満で成功する場合があるため、全量を書き切る。
	while (size > 0) {
		DWORD written = 0;
		DWORD chunk = size > MAXDWORD ? MAXDWORD : static_cast<DWORD>(size);
		if (!WriteFile(handle, data, chunk, &written, nullptr) || written == 0) {
			return false;
		}
		data += written;
		size -= written;
	}
	return true;
}

inline std::wstring convert_str_to_wstr(std::string_view const &str)
{
	std::wstring wstr;
	if (str.empty()) return wstr;
	int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
	if (len > 0) {
		wstr.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], len);
	}
	return wstr;
}

inline std::string convert_wstr_to_str(std::wstring_view const &str)
{
	std::string s;
	if (str.empty()) return s;
	int len = WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0, nullptr, nullptr);
	if (len > 0) {
		s.resize(len);
		WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), &s[0], len, nullptr, nullptr);
	}
	return s;
}

inline std::string_view getProgram(std::string_view cmdline)
{
	char const *begin = cmdline.data();
	char const *end = begin + cmdline.size();
	char const *ptr = begin;
	bool quote = 0;
	while (1) {
		char c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\"') {
			if (quote) {
				quote = false;
			} else {
				quote = true;
			}
			ptr++;
		} else if (quote && c != 0) {
			ptr++;
		} else if (isspace((unsigned char)c) || c == 0) {
			break;
		} else {
			ptr++;
		}
	}
	char const *left = begin;
	char const *right = ptr;
	if (left + 1 < right) {
		if (left[0] == '\"' && right[-1] == '\"') {
			left++;
			right--;
		}
	}
	return std::string_view(left, right - left);
}

template <typename HANDLE> class AbstractAutoHandle {
private:
	HANDLE h_ = { };

protected:
	virtual void close_handle(HANDLE &h) = 0;

public:
	AbstractAutoHandle() = default;
	AbstractAutoHandle(HANDLE h)
		: h_(h)
	{
	}
	AbstractAutoHandle(AbstractAutoHandle const &) = delete;
	AbstractAutoHandle &operator=(AbstractAutoHandle const &) = delete;
	AbstractAutoHandle(AbstractAutoHandle &&r) noexcept
		: h_(r.detach())
	{
	}
	AbstractAutoHandle &operator=(AbstractAutoHandle &&r) noexcept
	{
		if (this != std::addressof(r)) {
			close();
			h_ = r.detach();
		}
		return *this;
	}
	virtual ~AbstractAutoHandle()
	{
		close();
	}
	HANDLE detach()
	{
		HANDLE h = h_;
		h_ = { };
		return h;
	}
	void close()
	{
		HANDLE h = detach();
		if (IS_VALID_HANDLE(h)) {
			close_handle(h);
		}
	}
	HANDLE *operator->()
	{
		return &h_;
	}
	HANDLE *operator&()
	{
		close();
		return &h_;
	}
	HANDLE &operator=(HANDLE h)
	{
		if (h_ == h) return h_;
		close();
		h_ = h;
		return h_;
	}
	operator HANDLE &()
	{
		return h_;
	}
};

class AutoHandle : public AbstractAutoHandle<HANDLE> {
protected:
	void close_handle(HANDLE &h)
	{
		CloseHandle(h);
		h = nullptr;
	}

public:
	using AbstractAutoHandle<HANDLE>::AbstractAutoHandle;
	using AbstractAutoHandle<HANDLE>::operator=;
};

class AutoProcessInformation : public AbstractAutoHandle<PROCESS_INFORMATION> {
private:
	PROCESS_INFORMATION pi = { };

protected:
	void close_handle(PROCESS_INFORMATION &pi)
	{
		if (IS_VALID_HANDLE(pi.hProcess)) {
			CloseHandle(pi.hProcess);
		}
		if (IS_VALID_HANDLE(pi.hThread)) {
			CloseHandle(pi.hThread);
		}
		pi = { };
	}

public:
	using AbstractAutoHandle<PROCESS_INFORMATION>::AbstractAutoHandle;
	using AbstractAutoHandle<PROCESS_INFORMATION>::operator=;
};

class AutoPseudoConsole {
private:
	HPCON hPC = nullptr;

public:
	AutoPseudoConsole() = default;
	AutoPseudoConsole(AutoPseudoConsole const &) = delete;
	AutoPseudoConsole &operator=(AutoPseudoConsole const &) = delete;
	AutoPseudoConsole(AutoPseudoConsole &&r) noexcept
		: hPC(r.hPC)
	{
		r.hPC = nullptr;
	}
	AutoPseudoConsole &operator=(AutoPseudoConsole &&r) noexcept
	{
		if (this != std::addressof(r)) {
			close();
			hPC = r.hPC;
			r.hPC = nullptr;
		}
		return *this;
	}
	~AutoPseudoConsole()
	{
		close();
	}
	void close()
	{
		if (IS_VALID_HANDLE(hPC)) {
			ClosePseudoConsole(hPC);
			hPC = nullptr;
		}
	}
	operator HPCON &()
	{
		return hPC;
	}
	HPCON *operator&()
	{
		return &hPC;
	}
};

// ReadFileの境界をまたぐVTシーケンスも除去できるよう、解析状態を保持する。
class VtStripper {
public:
	std::string append(std::string_view input)
	{
		std::string output;
		for (unsigned char c : input) {
			switch (state_) {
			case State::Text:
				if (c == 0x1b) {
					state_ = State::Escape;
				} else {
					output.push_back(static_cast<char>(c));
				}
				break;

			case State::Escape:
				if (c == '[') {
					state_ = State::Csi;
				} else if (c == ']') {
					state_ = State::Osc;
				} else if (c == 'P' || c == 'X' || c == '^' || c == '_') {
					state_ = State::String;
				} else if (c >= 0x20 && c <= 0x2f) {
					state_ = State::EscapeIntermediate;
				} else if (c == 0x1b) {
					state_ = State::Escape;
				} else {
					state_ = State::Text;
				}
				break;

			case State::EscapeIntermediate:
				if (c >= 0x30 && c <= 0x7e) {
					state_ = State::Text;
				} else if (c == 0x1b) {
					state_ = State::Escape;
				}
				break;

			case State::Csi:
				if (c >= 0x40 && c <= 0x7e) {
					state_ = State::Text;
				} else if (c == 0x1b) {
					state_ = State::Escape;
				}
				break;

			case State::Osc:
				if (c == 0x07) {
					state_ = State::Text;
				} else if (c == 0x1b) {
					state_ = State::OscEscape;
				}
				break;

			case State::OscEscape:
				if (c == '\\') {
					state_ = State::Text;
				} else if (c != 0x1b) {
					state_ = State::Osc;
				}
				break;

			case State::String:
				if (c == 0x1b) {
					state_ = State::StringEscape;
				}
				break;

			case State::StringEscape:
				if (c == '\\') {
					state_ = State::Text;
				} else if (c != 0x1b) {
					state_ = State::String;
				}
				break;
			}
		}
		return output;
	}

private:
	enum class State {
		Text,
		Escape,
		EscapeIntermediate,
		Csi,
		Osc,
		OscEscape,
		String,
		StringEscape,
	};

	State state_ = State::Text;
};

}

#endif // PROCESSWINHELPER_H
