
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "zipinternal.h"
#include <zlib.h>

#ifdef _WIN32
#include <io.h>
#include <share.h>
#pragma warning(disable:4996)
#else
#define O_BINARY 0
#endif


namespace zip {

struct file_header_item_t {
	zip_file_header_t header;
	std::string path;
};

static void loadfile(char const *path, std::vector<unsigned char> *out)
{
	out->clear();
	int fd = open(path, O_RDONLY | O_BINARY);
	if (fd == -1) {
		std::string msg;
		msg = "Failed to open the file. (";
		msg += path;
		msg += ")";
		throw failed_t(msg);
	}
	struct stat st;
	if (fstat(fd, &st) == 0) {
		if (st.st_size > 0) {
			out->resize(st.st_size);
			read(fd, &out->at(0), st.st_size);
		}
	}
	close(fd);
}

static void write_(int fd, void const *p, int n)
{
	if (write(fd, p, n) != n) {
		throw failed_t("Failed to write into the zip file.");
	}
}

void archive(int fd, std::list<file_entry_t> const *filelist)
{
	std::list<file_header_item_t> fileheaders;

	unsigned short entries = 0;
	unsigned long offset_of_central_directory = 0;

	for (std::list<file_entry_t>::const_iterator filelist_it = filelist->begin(); filelist_it != filelist->end(); filelist_it++) {
		std::string srcpath = filelist_it->src;
		std::string dstpath = filelist_it->dst;

		unsigned long offset = offset_of_central_directory;

		struct tm *tm = localtime(&filelist_it->time);

		zip_local_file_header_t lfh;
		lfh.local_file_header_signature = 0x04034b50;
		lfh.version_needed_to_extract = 0x0014;
		lfh.general_purpose_bit_flag = 0;
		lfh.compression_method = 0;
		lfh.last_mod_file_time = msdos_time_format(tm->tm_hour, tm->tm_min, tm->tm_sec);
		lfh.last_mod_file_date = msdos_date_format(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
		lfh.crc_32 = 0;
		lfh.compressed_size = 0;
		lfh.uncompressed_size = 0;
		lfh.file_name_length = dstpath.size();
		lfh.extra_field_length = 0;

		std::vector<unsigned char> compressedbuffer;

		std::vector<unsigned char> sourcebuffer;
		unsigned char const *begin = 0;
		unsigned char const *end = 0;

		if (filelist_it->kind == file_entry_t::Data) {
			lfh.uncompressed_size = filelist_it->data.size();
			if (lfh.uncompressed_size > 0) {
				begin = &filelist_it->data[0];
				end = begin + lfh.uncompressed_size;
			}
		} else if (filelist_it->kind == file_entry_t::File) {
			loadfile(srcpath.c_str(), &sourcebuffer);
			lfh.uncompressed_size = sourcebuffer.size();
			begin = &sourcebuffer[0];
			end = begin + lfh.uncompressed_size;
		}


		if (lfh.uncompressed_size > 0) {

			z_stream c_stream;
			int err;

			c_stream.zalloc = (alloc_func)0;
			c_stream.zfree = (free_func)0;
			c_stream.opaque = (voidpf)0;

			err = deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
			if (err != Z_OK) {
				throw failed_t("Failed to deflateInit2.");
			}

			lfh.compression_method = 8;

			unsigned char const *ptr = begin;

			do {
				unsigned char tmp[4096];
				c_stream.total_in = 0;
				c_stream.total_out = 0;
				c_stream.next_in  = (unsigned char *)ptr;
				c_stream.next_out = tmp;
				c_stream.avail_in = end - ptr;
				c_stream.avail_out = sizeof(tmp);
				err = deflate(&c_stream, ptr < end ? Z_NO_FLUSH : Z_FINISH);
				if (err < 0) {
					throw failed_t("Failed to deflate.");
				}
				compressedbuffer.insert(compressedbuffer.end(), tmp, tmp + c_stream.total_out);
				ptr += c_stream.total_in;
			} while (err != Z_STREAM_END);

			err = deflateEnd(&c_stream);
			if (err < 0) {
				throw failed_t("Failed to deflateEnd.");
			}

			lfh.crc_32 = crc32(0, begin, end - begin);
			lfh.compressed_size = compressedbuffer.size();
		}

		write_(fd, &lfh, sizeof(zip_local_file_header_t));
		write_(fd, dstpath.c_str(), dstpath.size());

		offset_of_central_directory += sizeof(zip_local_file_header_t) + dstpath.size();

		if (!compressedbuffer.empty()) {
			write_(fd, &compressedbuffer[0], compressedbuffer.size());
			offset_of_central_directory += compressedbuffer.size();
		}

		{
			std::list<file_header_item_t>::iterator it = fileheaders.insert(fileheaders.end(), file_header_item_t());
			it->path = dstpath;
			it->header.central_file_header_signature = 0x02014b50;
			it->header.version_made_by = 0x0b16;
			it->header.version_needed_to_extract = 0x0014;
			it->header.general_purpose_bit_flag = 0;
			it->header.compression_method = 8;
			it->header.last_mod_file_time = lfh.last_mod_file_time;
			it->header.last_mod_file_date = lfh.last_mod_file_date;
			it->header.crc_32 = lfh.crc_32;
			it->header.compressed_size = compressedbuffer.size();
			it->header.uncompressed_size = lfh.uncompressed_size;
			it->header.file_name_length = it->path.size();
			it->header.extra_field_length = 0;
			it->header.file_comment_length = 0;
			it->header.disk_number_start = 0;
			it->header.internal_file_attributes = 1;
			it->header.external_file_attributes = 0x81b60020;
			it->header.relative_offset_of_local_header = offset;
		}

		entries++;
	}

	std::vector<unsigned char> eocdr_vec(sizeof(zip_end_of_central_directory_record_t));
	zip_end_of_central_directory_record_t *eocdr = (zip_end_of_central_directory_record_t *)&eocdr_vec[0];
	eocdr->end_of_central_dir_signature = 0x06054b50;
	eocdr->number_of_this_disk = 0;
	eocdr->number_of_the_disk_with_the_start_of_the_central_directory = 0;
	eocdr->total_number_of_entries_in_the_central_directory_on_this_disk = entries;
	eocdr->total_number_of_entries_in_the_central_directory = entries;
	eocdr->offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number = offset_of_central_directory;
	eocdr->comment_length = 0;

	eocdr->size_of_the_central_directory = 0;
	{
		for (std::list<file_header_item_t>::const_iterator it = fileheaders.begin(); it != fileheaders.end(); it++) {
			write_(fd, &it->header, sizeof(zip_file_header_t));
			write_(fd, it->path.c_str(), it->path.size());
			eocdr->size_of_the_central_directory += sizeof(zip_file_header_t) + it->path.size();
		}
	}

	write_(fd, &eocdr_vec[0], eocdr_vec.size());
}

void archive(char const *zipfile, std::list<file_entry_t> const *filelist)
{
	int fd = open(zipfile, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
	if (fd == -1) {
		std::string msg;
		msg = "Failed to create the zip file. (";
		msg += zipfile;
		msg += ")";
		throw failed_t(msg);
	}
	try {
		archive(fd, filelist);
		::close(fd);
	} catch (...) {
		::close(fd);
		throw;
	}
}


} // namespace zip
