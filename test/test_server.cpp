#include <gtest/gtest.h>
#include "libsk.h"

struct dummy_config {
    bool loaded;

    dummy_config() : loaded(false) {}

    int load_from_xml_file(const char *filename) {
        loaded = true;
        printf("config: %s.\n", filename);
        return 0;
    }
};

class test_svrd;
typedef sk::server<dummy_config, test_svrd> server_type;
class test_svrd : public server_type {
protected:
    virtual int on_init() {
        return 0;
    }

    virtual int on_fini() { return 0; }
    virtual int on_stop() { return 0; }
    virtual int on_run() { return 0; }
    virtual int on_reload() { return 0; }
};

TEST(server, common) {
    server_type& s = server_type::get();
    const char *argv[] = {"test_svrd", "--id=1.1.1.1"};
    int ret = s.init(2, argv);
    ASSERT_TRUE(ret == 0);
}
