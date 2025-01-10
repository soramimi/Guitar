#ifndef ZIPINTERNAL_H
#define ZIPINTERNAL_H

#include <string>
#include <vector>
#include <list>
#include <cstdint>

namespace zip {

#pragma pack(push, 1)

struct zip_local_file_header_t {
	uint32_t local_file_header_signature;    // 4_bytes  (0x04034b50)
	uint16_t version_needed_to_extract;      // 2_bytes
	uint16_t general_purpose_bit_flag;       // 2_bytes
	uint16_t compression_method;             // 2_bytes
	uint16_t last_mod_file_time;             // 2_bytes
	uint16_t last_mod_file_date;             // 2_bytes
	uint32_t crc_32;                         // 4_bytes
	uint32_t compressed_size;                // 4_bytes
	uint32_t uncompressed_size;              // 4_bytes
	uint16_t file_name_length;               // 2_bytes
	uint16_t extra_field_length;             // 2_bytes
};

struct zip_file_header_t {
	uint32_t central_file_header_signature;     // 4_bytes  (0x02014b50)
	uint16_t version_made_by;                   // 2_bytes
	uint16_t version_needed_to_extract;         // 2_bytes
	uint16_t general_purpose_bit_flag;          // 2_bytes
	uint16_t compression_method;                // 2_bytes
	uint16_t last_mod_file_time;                // 2_bytes
	uint16_t last_mod_file_date;                // 2_bytes
	uint32_t crc_32;                            // 4_bytes
	uint32_t compressed_size;                   // 4_bytes
	uint32_t uncompressed_size;                 // 4_bytes
	uint16_t file_name_length;                  // 2_bytes
	uint16_t extra_field_length;                // 2_bytes
	uint16_t file_comment_length;               // 2_bytes
	uint16_t disk_number_start;                 // 2_bytes
	uint16_t internal_file_attributes;          // 2_bytes
	uint32_t external_file_attributes;          // 4_bytes
	uint32_t relative_offset_of_local_header;   // 4_bytes
};

struct zip_end_of_central_directory_record_t {
	uint32_t end_of_central_dir_signature;                                                             // 4_bytes  (0x06054b50)
	uint16_t number_of_this_disk;                                                                      // 2_bytes
	uint16_t number_of_the_disk_with_the_start_of_the_central_directory;                               // 2_bytes
	uint16_t total_number_of_entries_in_the_central_directory_on_this_disk;                            // 2_bytes
	uint16_t total_number_of_entries_in_the_central_directory;                                         // 2_bytes
	uint32_t size_of_the_central_directory;                                                            // 4_bytes
	uint32_t offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number;            // 4_bytes
	uint16_t comment_length;                                                                          // 2_bytes
};

#pragma pack(pop)

struct failed_t {
	std::string message;
	failed_t(std::string const &message)
		: message(message)
	{
	}
};

inline uint16_t msdos_date_format(int year, int month, int day)
{
	return ((year - 1980) << 9) | (month << 5) | day;
}

inline uint16_t msdos_time_format(int hour, int minute, int second)
{
	return (hour << 11) | (minute << 5) | (second / 2);
}


struct Item {
	std::string name;
	zip_file_header_t cd;
	bool isdir() const
	{
		return cd.external_file_attributes & 0x10;
	}
};

class ZipInternal {
private:
	struct {
		char const *begin;
		char const *end;
	} data;
	std::list<Item> _items;

	static bool inflate(char *zptr, size_t zlen, std::vector<char> *out);

public:
	ZipInternal() = default;
	~ZipInternal()
	{
		close();
	}
	bool attach(char const *begin, char const *end);
	void close();
	bool extract_file(zip_file_header_t const *cd, std::vector<char> *out, zip_local_file_header_t *header = 0);
	bool extract_file(char const *path, std::vector<char> *out, zip_local_file_header_t *header = 0);
	std::list<Item> const *get_item_list() const
	{
		return &_items;
	}
	size_t size() const
	{
		return _items.size();
	}

};

struct file_entry_t {
	enum Kind {
		Data,
		File,
	};
	Kind kind;
	std::vector<unsigned char> data;
	std::string src;
	std::string dst;
	time_t time;
	file_entry_t()
		: kind(File)
	{
	}
	file_entry_t(std::string const &dst, unsigned char const *begin, unsigned char const *end, time_t time = -1)
		: kind(Data)
		, dst(dst)
		, time(time)
	{
		data.insert(data.end(), begin, end);
	}
	file_entry_t(std::string const &dst, std::vector<unsigned char> const *vec, time_t time = -1)
		: kind(Data)
		, dst(dst)
		, time(time)
	{
		data = *vec;
	}
	file_entry_t(std::string const &dst, std::string const &src, time_t time = -1)
		: kind(File)
		, src(src)
		, dst(dst)
		, time(time)
	{
	}
};

void archive(int fd, std::list<file_entry_t> const *filelist);
void archive(char const *zipfile, std::list<file_entry_t> const *filelist);

} // namespace zip

#endif
