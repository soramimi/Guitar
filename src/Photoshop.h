#ifndef PHOTOSHOP_H
#define PHOTOSHOP_H

#include <vector>
#include <QIODevice>

namespace photoshop {

void readThumbnail(QIODevice *in, std::vector<char> *jpeg);

}

#endif // PHOTOSHOP_H
