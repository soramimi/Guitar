#include <QObject>
#include <QString>
#include <stdexcept>
#include "Service.h"

class Service : public TestService {
public:
    bool open() override
    {
        auto mode = qEnvironmentVariable("GUITAR_TEST_PLUGIN_MODE");
        if (mode == "open-throw") throw QString("dictionary decode failed");
        return mode != "open-fail";
    }
};
class LoaderFixture : public QObject, public TestPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "guitar.tests.LoaderFixture")
public:
    TestService *create() override
    {
        auto mode = qEnvironmentVariable("GUITAR_TEST_PLUGIN_MODE");
        if (mode == "null") return nullptr;
        if (mode == "create-throw") throw std::runtime_error("factory failed");
        return new Service;
    }
};
#include "loaderfixture.moc"
