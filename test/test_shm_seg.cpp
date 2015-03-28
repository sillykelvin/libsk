#include "shm_seg.h"

int main(int argc, char **argv) {
    sk::shm_seg seg;
    int ret = seg.init(0x0012, 0xFF);
    if (ret != 0) {
        ERR("shm segment init failure, ret<%d>.", ret);
        return -1;
    }

    int *i = (int *) seg.malloc(sizeof(int));
    if (!i) {
        ERR("shm segment malloc failure.");
        return -1;
    }

    *i = 7;

    DBG("i: addr<0x%x>, val<%d>.", i, *i);

    return 0;
}
