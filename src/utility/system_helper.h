#ifndef SYSTEM_HELPER_H
#define SYSTEM_HELPER_H

#include <utility/types.h>

NS_BEGIN(sk)

struct memory_usage {
    size_t peak_virtual_memory;
    size_t current_virtual_memory;
    size_t peak_physical_memory;
    size_t current_physical_memory;
};

size_t get_sys_page_size();
size_t get_sys_total_memory();
void get_memory_usage(struct memory_usage *usage);

size_t get_used_shared_memory();
size_t get_total_shared_memory();

size_t get_fd_limit();
int set_fd_limit(size_t max);
size_t get_opened_fd_count();

size_t get_signal_queue_limit();
int set_signal_queue_limit(size_t max);
size_t get_queued_signal_count();


NS_END(sk)

#endif // SYSTEM_HELPER_H
