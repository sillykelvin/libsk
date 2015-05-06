#include "sk_inc.h"

struct singleton_info {
    int id;
    size_t size;

    bool operator==(const singleton_info& other) {
        if (this == &other)
            return true;

        if (other.id == this->id)
            return true;

        return false;
    }
};

static sk::fixed_array<singleton_info, sk::ST_MAX> singletons;

int register_singleton(int id, size_t size) {
    singleton_info tmp;
    tmp.id = id;
    assert_retval(!singletons.find(tmp), -EEXIST);

    singleton_info *info = singletons.emplace();
    assert_retval(info, -ENOMEM);

    info->id = id;
    info->size = size;

    return 0;
}
