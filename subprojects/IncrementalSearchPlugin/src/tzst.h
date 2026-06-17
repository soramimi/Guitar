#ifndef TZST_H
#define TZST_H

#include <functional>
#include <string>

namespace tzst {

bool archive_tar_zst(const std::string &archive_path, const std::string &src_dir, const std::string &dst_prefix_dir = {});
bool extract_tar_zst(const std::string &tarzst_path, const std::string &dstdir = {});

}

#endif // TZST_H
