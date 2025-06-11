
#include "common/joinpath.h"
#include "zip.h"
#include "zipinternal.h"
#include <QDateTime>
#include <QDebug>
#include <QDirIterator>
#include <fcntl.h>


#ifdef _MSC_VER
#include <io.h>
#include <sys/utime.h>
#else
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#define O_BINARY 0
#endif

namespace zip {

int Zip::archive_main()
{
	std::list<zip::file_entry_t> files;
	QDirIterator it("../");
	while (it.hasNext()) {
		it.next();
		QFileInfo info = it.fileInfo();
		if (info.isFile()) {
			if (it.fileName().startsWith('.')) continue;
			QString src = it.filePath();
			QString dst = it.filePath();
			if (dst.startsWith("../")) {
				dst = "hoge/" + dst.mid(3);
			}
			qDebug() << dst;
			zip::file_entry_t t;
			t.src = src.toStdString();
			t.dst = dst.toStdString();
			files.push_back(t);
		}
	}
	zip::archive("c:/a/test.zip", &files);
	return 0;
}

bool Zip::extract_from_data(char const *zipdata, size_t zipsize, std::string const &destdir)
{
	zip::ZipInternal zip;
	zip.attach(zipdata, zipdata + zipsize);
	std::list<zip::Item> const *items = zip.get_item_list();
	for (std::list<zip::Item>::const_iterator it = items->begin(); it != items->end(); ++it) {
		std::vector<char> out;
		if (it->isdir()) {
			QDir().mkpath(QString::fromStdString(destdir / it->name));
		} else {
			unsigned short msdostime = it->cd.last_mod_file_date;
			QDateTime dt;
			dt.setDate(QDate((msdostime >> 9) + 1980, (msdostime >> 5) & 0xf, msdostime & 0x1f));
			dt.setTime(QTime(it->cd.last_mod_file_time >> 11, (it->cd.last_mod_file_time >> 5) & 0x3f, (it->cd.last_mod_file_time & 0x1f) * 2));
			zip.extract_file(&it->cd, &out);
			int fd = open((destdir / it->name).c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
			if (fd != -1) {
				write(fd, &out[0], out.size());
				close(fd);
#ifdef Q_OS_WIN
				struct _utimbuf t;
#else
				struct utimbuf t;
#endif
				t.actime = dt.toSecsSinceEpoch();
				t.modtime = dt.toSecsSinceEpoch();
				std::string path = destdir / it->name;
#ifdef Q_OS_WIN
				_utime(path.c_str(), &t);
#else
				utime(path.c_str(), &t);
#endif
			} else {
				perror("open");
			}
		}
	}
	return true;
}

bool Zip::extract(std::string const &zipfile, std::string const &destdir)
{
	int fd = open(zipfile.c_str(), O_RDONLY | O_BINARY);
	if (fd < 0) {
		perror("open");
		return false;
	}
	struct stat st;
	fstat(fd, &st);
	std::vector<char> data(st.st_size);
	read(fd, data.data(), data.size());
	close(fd);

	return true;
}

int Zip::ziptest()
{
	// archive_main();
	// extract_main("C:/develop/Guitar/misc/migemo.zip", "C:/a");
	return 0;
}

} // namespace zip


