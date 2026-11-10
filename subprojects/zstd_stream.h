
#pragma once

// Shared by both plugins; errors are returned through the public ZS adapter.
#include <functional>
#include <memory>
#include <string>
#include <zstd.h>

namespace guitar_zstd {
template <typename T> void free_(T *p);
template <> inline void free_(ZSTD_DCtx *p) { ZSTD_freeDCtx(p); }
template <> inline void free_(ZSTD_CCtx *p) { ZSTD_freeCCtx(p); }

template <typename T> class Context {
private:
	T *ctx;
public:
	explicit Context(T *p)
	{
		ctx = p;
	}
	~Context()
	{
		if (ctx) {
			free_(ctx);
		}
	}
	operator bool () const
	{
		return (bool)ctx;
	}
	operator T * () const
	{
		return ctx;
	}
};

inline bool decompress(std::function<int (char *, int)> const &in_fn, std::function<int (char const *, int)> const &out_fn, size_t maxlen, std::string &error)
{
	error = {};

	const size_t buffInSize = ZSTD_DStreamInSize();
	const size_t buffOutSize = ZSTD_DStreamOutSize();
	std::unique_ptr<char[]> buffer(new char [buffInSize + buffOutSize]);
	char *const buffIn = buffer.get();
	char *const buffOut = buffIn + buffInSize;

	Context<ZSTD_DCtx> dctx(ZSTD_createDCtx());
	if (!dctx) {
		error = "ZSTD_createDCtx() failed";
		return false;
	}

	size_t total = 0;
	size_t remaining = 1;
	bool hasInput = false;
	while (true) {
		const int read = in_fn(buffIn, static_cast<int>(buffInSize));
		if (read < 0 || static_cast<size_t>(read) > buffInSize) {
			error = "input read failed";
			return false;
		}
		if (read == 0) break;
		hasInput = true;
		ZSTD_inBuffer input = {buffIn, static_cast<size_t>(read), 0};
		while (input.pos < input.size) {
			ZSTD_outBuffer output = {buffOut, buffOutSize, 0};
			remaining = ZSTD_decompressStream(dctx, &output, &input);
			if (ZSTD_isError(remaining)) {
				error = ZSTD_getErrorName(remaining);
				return false;
			}
			if (maxlen != static_cast<size_t>(-1) && output.pos > maxlen - total) {
				error = "decompressed data exceeds size limit";
				return false;
			}
			if (output.pos && out_fn(buffOut, static_cast<int>(output.pos)) != static_cast<int>(output.pos)) {
				error = "output write failed";
				return false;
			}
			total += output.pos;
		}
	}
	if (!hasInput || remaining != 0) {
		error = hasInput ? "incomplete zstd frame" : "input is empty";
		return false;
	}
	return true;
}

inline bool compress(std::function<int (char *, int)> const &in_fn, std::function<int (char const *, int)> const &out_fn, std::string &error)
{
	error = {};

	const int CLEVEL = ZSTD_CLEVEL_DEFAULT;

	const size_t buffInSize = ZSTD_CStreamInSize();
	const size_t buffOutSize = ZSTD_CStreamOutSize();
	std::unique_ptr<char[]> buffer(new char [buffInSize + buffOutSize]);
	char *const buffIn = buffer.get();
	char *const buffOut = buffIn + buffInSize;

	Context<ZSTD_CCtx> cctx(ZSTD_createCCtx());
	if (!cctx) {
		error = "ZSTD_createCCtx() failed";
		return false;
	}

	size_t ret;
	ret = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, CLEVEL);
	if (ZSTD_isError(ret)) {
		error = ZSTD_getErrorName(ret);
		return false;
	}
	ret = ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
	if (ZSTD_isError(ret)) {
		error = ZSTD_getErrorName(ret);
		return false;
	}

	const size_t toRead = buffInSize;
	while (1) {
		const int read = in_fn(buffIn, (int)toRead);
		if (read < 0 || static_cast<size_t>(read) > buffInSize) {
			error = "input read failed";
			return false;
		}
		const bool lastChunk = (read == 0);
		const ZSTD_EndDirective mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;
		ZSTD_inBuffer input = { buffIn, (size_t)read, 0 };
		int finished;
		do {
			ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
			const size_t remaining = ZSTD_compressStream2(cctx, &output , &input, mode);
			if (ZSTD_isError(remaining)) {
				error = ZSTD_getErrorName(remaining);
				return false;
			}
			if (output.pos && out_fn(buffOut, static_cast<int>(output.pos)) != static_cast<int>(output.pos)) {
				error = "output write failed";
				return false;
			}
			finished = lastChunk ? (remaining == 0) : (input.pos == input.size);
		} while (!finished);

		if (input.pos != input.size) {
			error = "zstd only returns 0 when the input is completely consumed";
			return false;
		}
		if (lastChunk) break;
	}

	return true;
}


} // namespace guitar_zstd
