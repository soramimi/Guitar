#include "FileType.h"
#include "magic.h"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#define NOMINMAX
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#define O_BINARY 0
#endif

extern "C" {
#include "file.h"
}

namespace {

#define SLOP (1 + sizeof(union VALUETYPE))

#define FILE_SEPARATOR "\n- "

static void trim_separator(magic_t ms)
{
	size_t l;

	if (ms->o.buf == NULL) return;

	l = strlen(ms->o.buf);
	if (l < sizeof(FILE_SEPARATOR)) return;

	l -= sizeof(FILE_SEPARATOR) - 1;
	if (strcmp(ms->o.buf + l, FILE_SEPARATOR) != 0) return;

	ms->o.buf[l] = '\0';
}


static int checkdone(magic_t ms, int *rv)
{
	if ((ms->flags & MAGIC_CONTINUE) == 0) return 1;

	if (file_separator(ms) == -1) {
		*rv = -1;
	}

	return 0;
}

int _file_buffer(magic_t ms, int fd, struct stat *st, const char *inname __attribute__ ((__unused__)), const void *buf, size_t nb)
{
	int m = 0, rv = 0, looks_text = 0;
	const char *code = NULL;
	const char *code_mime = "binary";
	const char *def = "data";
	const char *ftype = NULL;
	char *rbuf = NULL;
	struct buffer b;

	buffer_init(&b, fd, st, buf, nb);
	ms->mode = b.st.st_mode;

	if (nb == 0) {
		def = "empty";
		goto simple;
	} else if (nb == 1) {
		def = "very short file (no magic)";
		goto simple;
	}

	if ((ms->flags & MAGIC_NO_CHECK_ENCODING) == 0) {
		looks_text = file_encoding(ms, &b, NULL, 0, &code, &code_mime, &ftype);
	}

#if 0
#ifdef __EMX__
	if ((ms->flags & MAGIC_NO_CHECK_APPTYPE) == 0 && inname) {
		m = file_os2_apptype(ms, inname, &b);
		if ((ms->flags & MAGIC_DEBUG) != 0)
			fprintf(stderr, "[try os2_apptype %d]\n", m);
		switch (m) {
		case -1:
			return -1;
		case 0:
			break;
		default:
			return 1;
		}
	}
#endif
#if HAVE_FORK
	/* try compression stuff */
	if ((ms->flags & MAGIC_NO_CHECK_COMPRESS) == 0) {
		m = file_zmagic(ms, &b, inname);
		if ((ms->flags & MAGIC_DEBUG) != 0) {
			fprintf(stderr, "[try zmagic %d]\n", m);
		}
		if (m) goto done_encoding;
	}
#endif
#endif

	/* Check if we have a tar file */
	if ((ms->flags & MAGIC_NO_CHECK_TAR) == 0) {
		m = file_is_tar(ms, &b);
		if ((ms->flags & MAGIC_DEBUG) != 0)
			fprintf(stderr, "[try tar %d]\n", m);
		if (m) {
			if (checkdone(ms, &rv)) goto done;
		}
	}

	/* Check if we have a JSON file */
	if ((ms->flags & MAGIC_NO_CHECK_JSON) == 0) {
		m = file_is_json(ms, &b);
		if ((ms->flags & MAGIC_DEBUG) != 0)
			fprintf(stderr, "[try json %d]\n", m);
		if (m) {
			if (checkdone(ms, &rv)) goto done;
		}
	}

	/* Check if we have a CSV file */
	if ((ms->flags & MAGIC_NO_CHECK_CSV) == 0) {
		m = file_is_csv(ms, &b, looks_text, code);
		if ((ms->flags & MAGIC_DEBUG) != 0)
			fprintf(stderr, "[try csv %d]\n", m);
		if (m) {
			if (checkdone(ms, &rv)) goto done;
		}
	}

	/* Check if we have a SIMH tape file */
	if ((ms->flags & MAGIC_NO_CHECK_SIMH) == 0) {
		m = file_is_simh(ms, &b);
		if ((ms->flags & MAGIC_DEBUG) != 0)
			fprintf(stderr, "[try simh %d]\n", m);
		if (m) {
			if (checkdone(ms, &rv)) goto done;
		}
	}

	/* Check if we have a CDF file */
	if ((ms->flags & MAGIC_NO_CHECK_CDF) == 0) {
		m = file_trycdf(ms, &b);
		if ((ms->flags & MAGIC_DEBUG) != 0)
			fprintf(stderr, "[try cdf %d]\n", m);
		if (m) {
			if (checkdone(ms, &rv)) goto done;
		}
	}
#if 1//def BUILTIN_ELF
	if ((ms->flags & MAGIC_NO_CHECK_ELF) == 0 && nb > 5 && fd != -1) {
		file_pushbuf_t *pb;
		/*
		 * We matched something in the file, so this
		 * *might* be an ELF file, and the file is at
		 * least 5 bytes long, so if it's an ELF file
		 * it has at least one byte past the ELF magic
		 * number - try extracting information from the
		 * ELF headers that cannot easily be  extracted
		 * with rules in the magic file. We we don't
		 * print the information yet.
		 */
		pb = file_push_buffer(ms);
		if (pb == NULL) return -1;

		rv = file_tryelf(ms, &b);
		rbuf = file_pop_buffer(ms, pb);
		if (rv == -1) {
			free(rbuf);
			rbuf = NULL;
		}
		if ((ms->flags & MAGIC_DEBUG) != 0)
			fprintf(stderr, "[try elf %d]\n", m);
	}
#endif

	/* try soft magic tests */
	if ((ms->flags & MAGIC_NO_CHECK_SOFT) == 0) {
		m = file_softmagic(ms, &b, NULL, NULL, BINTEST, looks_text);
		if ((ms->flags & MAGIC_DEBUG) != 0) {
			fprintf(stderr, "[try softmagic %d]\n", m);
		}
		if (m == 1 && rbuf) {
			if (file_printf(ms, "%s", rbuf) == -1) goto done;
		}
		if (m) {
			if (checkdone(ms, &rv)) goto done;
		}
	}

	/* try text properties */
	if ((ms->flags & MAGIC_NO_CHECK_TEXT) == 0) {

		m = file_ascmagic(ms, &b, looks_text);
		if ((ms->flags & MAGIC_DEBUG) != 0) {
			fprintf(stderr, "[try ascmagic %d]\n", m);
		}
		if (m) goto done;
	}

simple:
	/* give up */
	if (m == 0) {
		m = 1;
		rv = file_default(ms, nb);
		if (rv == 0)
			if (file_printf(ms, "%s", def) == -1)
				rv = -1;
	}
 done:
	trim_separator(ms);
	if ((ms->flags & MAGIC_MIME_ENCODING) != 0) {
		if (ms->flags & MAGIC_MIME_TYPE) {
			if (file_printf(ms, "; charset=") == -1) {
				rv = -1;
			}
		}
		if (file_printf(ms, "%s", code_mime) == -1) {
			rv = -1;
		}
	}
#if 0//HAVE_FORK
 done_encoding:
#endif
	free(rbuf);
	buffer_fini(&b);
	if (rv)
		return rv;

	return m;
}

const char *_fd_or_buf(magic_t ms, int fd, unsigned char *buf, ssize_t nbytes, struct stat *st, bool pad_slop)
{
	if (fd != -1) {
		if (buf) {
			fprintf(stderr, "Only one of fd or buf must be valid\n");
			return nullptr;
		}

		struct stat st2;
		if (fstat(fd, &st2) != 0) return nullptr;

		nbytes = std::min((size_t)st2.st_size, ms->bytes_max);
		std::vector<unsigned char> data(nbytes + SLOP);
		nbytes = read(fd, data.data(), nbytes);
		return _fd_or_buf(ms, -1, data.data(), nbytes, &st2, false);
	}

	if (pad_slop) {
		std::vector<unsigned char> data(nbytes + SLOP);
		memcpy(data.data(), buf, nbytes);
		return _fd_or_buf(ms, -1, data.data(), nbytes, st, false);

	}

	if (_file_buffer(ms, fd, st, NULL, buf, CAST(size_t, nbytes)) == -1) return nullptr;

	return file_getbuffer(ms);

}

FileType::Result parse_mime(std::string const &mime)
{
	if (mime.empty()) return {};

	FileType::Result ret;

	char const *p = mime.c_str();
	char const *q = strchr(p, ';');
	if (q) {
		ret.mimetype = std::string(p, q - p);
		q++;
		while (*q == ' ') q++;
		if (strncmp(q, "charset=", 8) == 0) {
			q += 8;
			char const *r = strchr(q, ';');
			if (r) {
				ret.charset = std::string(q, r - q);
			} else {
				ret.charset = q;
			}
		}
	}
	return ret;
}

} // namespace

// FileType implementation

bool FileType::open(const char *mgcptr, size_t mgclen)
{
	if (magic_set) return true;

	bool ok = false;

	magic_set = magic_open(MAGIC_MIME);

	if (!magic_set) {
		fprintf(stderr, "unable to initialize magic library\n");
	} else {
		mgcdata.resize(mgclen);
		memcpy(mgcdata.data(), mgcptr, mgclen);
		void *bufs[1];
		size_t sizes[1];
		bufs[0] = mgcdata.data();
		sizes[0] = mgclen;
		if (magic_load_buffers((magic_t)magic_set, bufs, sizes, 1) == 0) {
			ok = true;
		}
	}

	return ok;
}

/**
 * @brief Open the file type object
 * @param mgcfile The magic file
 * @return Whether the file type object was opened successfully
 */
bool FileType::open(const char *mgcfile)
{
	if (magic_set) return true;

	bool ok = false;

	if (1) {
		magic_set = magic_open(MAGIC_MIME);
		if (magic_set) {
			if (magic_load((magic_t)magic_set, mgcfile) != 0) {
				printf("cannot load magic database - %s\n", magic_error((magic_t)magic_set));
				magic_close((magic_t)magic_set);
				ok = true;
			}
		}
	} else {
		int fd = ::open(mgcfile, O_RDONLY);
		if (fd != -1) {
			struct stat st;
			if (fstat(fd, &st) == 0 && st.st_size > 0) {
				magic_set = magic_open(MAGIC_MIME);
				if (magic_set) {
					mgcdata.resize(st.st_size);
					read(fd, mgcdata.data(), st.st_size);
					void *bufs[1];
					size_t sizes[1];
					bufs[0] = mgcdata.data();
					sizes[0] = st.st_size;
					if (magic_load_buffers((magic_t)magic_set, bufs, sizes, 1) == 0) {
						ok = true;
					}
				}
			}
			::close(fd);
		}
	}
	if (!magic_set) {
		fprintf(stderr, "unable to initialize magic library\n");
	}
	return ok;
}

/**
 * @brief Close the file type object
 */
void FileType::close()
{
	if (magic_set) {
		magic_close((magic_t)magic_set);
		magic_set = nullptr;
	}
}

/**
 * @brief Get the slop size
 * @return The slop size
 *
 * The slop size is the size of the buffer that is added to the end of the file buffer.
 */
size_t FileType::slop_size()
{
	return SLOP;
}

/**
 * @brief Determine the file type of a file descriptor
 * @param fd The file descriptor
 * @return The result
 */
FileType::Result FileType::file(int fd) const
{
	if (!magic_set) {
		fprintf(stderr, "magic_set is null\n");
		return {};
	}

	std::string mime;

	if (fd != -1) {
		struct stat st;
		if (fstat(fd, &st) == 0) {
			mime = _fd_or_buf((magic_t )magic_set, fd, nullptr, 0, nullptr, false);
		}
	}

	return parse_mime(mime.c_str());
}

/**
 * @brief Determine the file type of a file
 * @param filepath The path to the file
 * @return The result
 */
FileType::Result FileType::file(const char *filepath) const
{
	if (!magic_set) {
		fprintf(stderr, "magic_set is null\n");
		return {};
	}

	Result ret;

	int fd = ::open(filepath, O_RDONLY | O_BINARY | O_NONBLOCK | O_CLOEXEC);
	if (fd != -1) {
		ret = file(fd);
		::close(fd);
	}

	return ret;
}

/**
 * @brief Determine the file type of a buffer
 * @param data The buffer
 * @param size The size of the buffer
 * @param st_mode The mode of the file
 * @param pad_slop Whether to pad the buffer with slop
 * @return The result
 */
FileType::Result FileType::file(const char *data, size_t size, int st_mode, bool pad_slop) const
{
	if (!magic_set) {
		fprintf(stderr, "magic_set is null\n");
		return {};
	}

	struct stat st;
	st = {};
	st.st_size = size;
	st.st_mode = st_mode;
	std::string mime = _fd_or_buf((magic_t )magic_set, -1, (unsigned char *)data, size, &st, pad_slop);

	return parse_mime(mime.c_str());
}
