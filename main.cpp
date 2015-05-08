#include <vector>
#include "sk_inc.h"

#define LOOP_COUNT 10000

#define TINY_COUNT 1000
#define HUGE_COUNT 100

#define TINY_RANGE_MIN 32
#define TINY_RANGE_MAX 1024
#define HUGE_RANGE_MIN 1025
#define HUGE_RANGE_MAX (1024 * 1024)

inline int rand_range(int min, int max) {
    if (min > max) {
        int tmp = min;
        min = max;
        max = tmp;
    }

    return  min == max ? min : (rand() % (max - min + 1) + min);
}

int test_new_delete(std::vector<char *>& tiny, std::vector<char *>& huge) {
    for (int i = 0; i < LOOP_COUNT; ++i) {
        for (int j = 0; j < TINY_COUNT; ++j) {
            int size = rand_range(TINY_RANGE_MIN, TINY_RANGE_MAX);
            char *addr = new char[size];
            if (!addr) {
                printf("new<size: %d> failed.", size);
                return -1;
            }

            tiny.push_back(addr);
        }

        for (int j = 0; j < HUGE_COUNT; ++j) {
            int size = rand_range(HUGE_RANGE_MIN, HUGE_RANGE_MAX);
            char *addr = new char[size];
            if (!addr) {
                printf("new<size: %d> failed.", size);
                return -1;
            }

            huge.push_back(addr);

        }

        for (int j = 0; j < (int) tiny.size(); ++j) {
            delete [] tiny[j];
        }

        tiny.clear();

        for (int j = 0; j < TINY_COUNT; ++j) {
            int size = rand_range(TINY_RANGE_MIN, TINY_RANGE_MAX);
            char *addr = new char[size];
            if (!addr) {
                printf("new<size: %d> failed.", size);
                return -1;
            }

            tiny.push_back(addr);
        }

        for (int j = 0; j < (int) huge.size(); ++j) {
            delete [] huge[j];
        }

        huge.clear();

        for (int j = 0; j < HUGE_COUNT; ++j) {
            int size = rand_range(HUGE_RANGE_MIN, HUGE_RANGE_MAX);
            char *addr = new char[size];
            if (!addr) {
                printf("new<size: %d> failed.", size);
                return -1;
            }

            huge.push_back(addr);

        }

        for (int j = 0; j < (int) tiny.size(); ++j) {
            delete [] tiny[j];
        }

        for (int j = 0; j < (int) huge.size(); ++j) {
            delete [] huge[j];
        }

        tiny.clear();
        huge.clear();
    }

    return 0;
}

int test_shm_mgr(std::vector<shm_ptr>& tiny, std::vector<shm_ptr>& huge) {
    for (int i = 0; i < LOOP_COUNT; ++i) {
        for (int j = 0; j < TINY_COUNT; ++j) {
            int size = rand_range(TINY_RANGE_MIN, TINY_RANGE_MAX);
            shm_ptr ptr;
            void *addr = sk::shm_malloc(size, ptr);
            if (!addr) {
                printf("shm malloc<size: %d> failed.", size);
                return -1;
            }

            tiny.push_back(ptr);
        }

        for (int j = 0; j < HUGE_COUNT; ++j) {
            int size = rand_range(HUGE_RANGE_MIN, HUGE_RANGE_MAX);
            shm_ptr ptr;
            void *addr = sk::shm_malloc(size, ptr);
            if (!addr) {
                printf("shm malloc<size: %d> failed.", size);
                return -1;
            }

            huge.push_back(ptr);

        }

        for (int j = 0; j < (int) tiny.size(); ++j) {
            sk::shm_free(tiny[i]);
        }

        tiny.clear();

        for (int j = 0; j < TINY_COUNT; ++j) {
            int size = rand_range(TINY_RANGE_MIN, TINY_RANGE_MAX);
            shm_ptr ptr;
            void *addr = sk::shm_malloc(size, ptr);
            if (!addr) {
                printf("shm malloc<size: %d> failed.", size);
                return -1;
            }

            tiny.push_back(ptr);
        }

        for (int j = 0; j < (int) huge.size(); ++j) {
            sk::shm_free(huge[i]);
        }

        huge.clear();

        for (int j = 0; j < HUGE_COUNT; ++j) {
            int size = rand_range(HUGE_RANGE_MIN, HUGE_RANGE_MAX);
            shm_ptr ptr;
            void *addr = sk::shm_malloc(size, ptr);
            if (!addr) {
                printf("shm malloc<size: %d> failed.", size);
                return -1;
            }

            huge.push_back(ptr);

        }

        for (int j = 0; j < (int) tiny.size(); ++j) {
            sk::shm_free(tiny[i]);
        }

        for (int j = 0; j < (int) huge.size(); ++j) {
            sk::shm_free(huge[i]);
        }

        tiny.clear();
        huge.clear();
    }

    return 0;
}

void print_time_cost(const char *test_type, const timeval& begin, const timeval& end) {
    timeval handle;
    if (end.tv_usec < begin.tv_usec) {
        handle.tv_usec = (1000000 + end.tv_usec) - begin.tv_usec;
        handle.tv_sec  = end.tv_sec - begin.tv_sec - 1;
    } else {
        handle.tv_usec = end.tv_usec - begin.tv_usec;
        handle.tv_sec  = end.tv_sec - begin.tv_sec;
    }

    printf("test type: %s;\t\t time cost: %f ms.\n", test_type, (float) (handle.tv_sec * 1000.0 + handle.tv_usec / 1000.0));
}

int main(int, char **) {
    srand(time(NULL));

    sk::shm_mgr_init(0x7777, 0x777, 0x77, 0x7, false, TINY_RANGE_MAX + 128, TINY_COUNT + TINY_COUNT / 100, HUGE_COUNT * HUGE_RANGE_MAX);

    timeval begin_time, end_time;

    {
        std::vector<char *> tiny;
        std::vector<char *> huge;
        tiny.reserve(TINY_COUNT + TINY_COUNT / 100);
        huge.reserve(HUGE_COUNT + HUGE_COUNT / 100);

        gettimeofday(&begin_time, NULL);
        test_new_delete(tiny, huge);
        gettimeofday(&end_time, NULL);

        print_time_cost("new/delete", begin_time, end_time);
    }

    {
        std::vector<shm_ptr> tiny;
        std::vector<shm_ptr> huge;
        tiny.reserve(TINY_COUNT + TINY_COUNT / 100);
        huge.reserve(HUGE_COUNT + HUGE_COUNT / 100);

        gettimeofday(&begin_time, NULL);
        test_shm_mgr(tiny, huge);
        gettimeofday(&end_time, NULL);

        print_time_cost("shm_mgr", begin_time, end_time);
    }

    sk::shm_mgr_fini();

    return 0;
}
