#ifndef XMLTAGSTATE_H
#define XMLTAGSTATE_H

#include <vector>
#include <Qt>

class QString;
class QStringView;

class XmlTagState {
private:
	std::vector<ushort> arr;
public:
	void push(QString const &tag);
	void push(QStringView const &tag);
	void pop();
	bool is(char const *path) const;
	bool operator == (char const *path) const;
	QString str() const;
};

#endif // XMLTAGSTATE_H
