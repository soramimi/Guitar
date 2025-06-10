
#include <fcntl.h>
#include <sys/stat.h>
#include <QDebug>
#include <cstring>

#include "zipinternal.h"
#include <zlib.h>

#ifdef Q_OS_WIN
#include <io.h>
#include <share.h>
#pragma warning(disable:4996)
#endif

namespace zip {

bool ZipInternal::inflate(char *zptr, size_t zlen, std::vector<char> *out)
{
	int err;
	z_stream d_stream; /* decompression stream */

	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	d_stream.opaque = (voidpf)0;

	d_stream.next_in  = (unsigned char *)zptr;
	d_stream.avail_in = (uInt)zlen;

	err = inflateInit2(&d_stream, -MAX_WBITS);

	if (err != Z_OK) {
		return false;
	}

	while (1) {
		unsigned char tmp[65536];
		d_stream.next_out = tmp;            /* discard the output */
		d_stream.avail_out = sizeof(tmp);
		uLong total = d_stream.total_out;
		err = ::inflate(&d_stream, Z_NO_FLUSH);
		out->insert(out->end(), tmp, tmp + (d_stream.total_out - total));
		if (err == Z_STREAM_END) {
			break;
		}
		if (err != Z_OK) {
			return false;
		}
	}

	err = inflateEnd(&d_stream);
	if (err != Z_OK) {
		return false;
	}

	return true;
}

bool ZipInternal::attach(char const *begin, char const *end)
{
	// zip_end_of_central_directory_record_tを読む
	zip_end_of_central_directory_record_t const *eocd;
	size_t n = sizeof(zip_end_of_central_directory_record_t);
	if (end - begin >= n) {
		eocd = reinterpret_cast<zip_end_of_central_directory_record_t const *>(end - n);
	} else {
		return false;
	}
	if (memcmp(&eocd->end_of_central_dir_signature, "PK\x05\x06", 4) != 0) {
		return false;
	}

	// zip_file_header_tへ移動
	size_t o = eocd->offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number;
	char const *p = begin + o;

	// zip_file_header_tを読む
	for (int i = 0; i < eocd->total_number_of_entries_in_the_central_directory; i++) {
		if (end - p < sizeof(zip_file_header_t)) {
			return false;
		}
		zip_file_header_t const *cd = reinterpret_cast<zip_file_header_t const *>(p);
		p += sizeof(zip_file_header_t);
		if (memcmp(&cd->central_file_header_signature, "PK\x01\x02", 4) != 0) {
			return false;
		}
		std::vector<unsigned char> tmp;
		std::string filename;
		int n;
		n = cd->file_name_length + cd->extra_field_length;
		if (n > 0) {
			if (end - p < n) {
				return false;
			}
			tmp.resize(n);
			memcpy(&tmp[0], p, n);
			p += n;
			filename.assign((char const *)&tmp[0], cd->file_name_length);	// ファイル名
		}
		_items.push_back(Item());	// リストに追加
		_items.back().name = filename;
		_items.back().cd = *cd;
	}

	data.begin = begin;
	data.end = end;

	return true;
}

void ZipInternal::close()
{
}

// ファイルを抽出
bool ZipInternal::extract_file(zip_file_header_t const *cd, std::vector<char> *out, zip_local_file_header_t *header)
{
	if (!data.begin) return false;
	if (data.end <= data.begin) return false;

	if (cd->compression_method == 0 || cd->compression_method == 8) {	// 非圧縮またはdelflate形式
		char const *p = data.begin;
		p += cd->relative_offset_of_local_header;
		zip_local_file_header_t const *lfh = reinterpret_cast<zip_local_file_header_t const *>(p);
		p += sizeof(zip_local_file_header_t);
		p += lfh->file_name_length + lfh->extra_field_length;
		out->clear();
		if (cd->compression_method == 0) { // 非圧縮
			out->resize(lfh->uncompressed_size);
			if (lfh->uncompressed_size > 0) {
				if (data.end - p < lfh->uncompressed_size) {
					return false;
				}
				memcpy(&(*out)[0], p, lfh->uncompressed_size);
			}
		} else if (cd->compression_method == 8) { // delflate形式
			if (lfh->compressed_size > 0) {
				if (data.end - p < lfh->compressed_size) {
					return false;
				}
				std::vector<char> zip(lfh->compressed_size);
				memcpy(&zip[0], p, lfh->compressed_size);
				p += lfh->compressed_size;
				if (!inflate(&zip[0], zip.size(), out)) {	// 圧縮解除
					return false;
				}
			}
		}
		if (header) {
			*header = *lfh;
		}
		return true;
	}
	return false;
}

// 名前を指定して一個のファイルを抽出
bool ZipInternal::extract_file(char const *path, std::vector<char> *out, zip_local_file_header_t *header)
{
	for (std::list<Item>::const_iterator it = _items.begin(); it != _items.end(); it++) {
		if (it->name == path) {	// 発見
			return extract_file(&it->cd, out, header);
		}
	}
	return false;
}

//

} // namespace zip

