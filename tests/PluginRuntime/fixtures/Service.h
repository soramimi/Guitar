#pragma once

struct TestService {
    virtual ~TestService() = default;
    virtual bool open() = 0;
};
struct TestPluginInterface {
    virtual ~TestPluginInterface() = default;
    virtual TestService *create() = 0;
};
