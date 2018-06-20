#include <utility>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <shm/shm_mgr.h>
#include <sys/resource.h>
#include <utility/system_helper.h>

NS_BEGIN(sk)

size_t get_sys_page_size() {
    return cast_size(sysconf(_SC_PAGESIZE));
}

size_t get_sys_total_memory() {
    return cast_size(sysconf(_SC_PHYS_PAGES)) * get_sys_page_size();
}

void get_memory_usage(memory_usage *usage) {
    if (!usage) return;

    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) return;

    char unit[16] = {0};
    char line[128] = {0};
    int checked = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp("VmPeak:", line, 7) == 0) {
            checked += 1;
            if (sscanf(line, "%*s %lu %s\n", &usage->peak_virtual_memory, unit) == 2) {
                if (strncasecmp("KB", unit, 2) == 0)
                    usage->peak_virtual_memory *= 1024;

                if (strncasecmp("MB", unit, 2) == 0)
                    usage->peak_virtual_memory *= (1024 * 1024);

                if (strncasecmp("GB", unit, 2) == 0)
                    usage->peak_virtual_memory *= (1024 * 1024 * 1024);
            }
        }

        if (strncmp("VmSize:", line, 7) == 0) {
            checked += 1;
            if (sscanf(line, "%*s %lu %s\n", &usage->current_virtual_memory, unit) == 2) {
                if (strncasecmp("KB", unit, 2) == 0)
                    usage->current_virtual_memory *= 1024;

                if (strncasecmp("MB", unit, 2) == 0)
                    usage->current_virtual_memory *= (1024 * 1024);

                if (strncasecmp("GB", unit, 2) == 0)
                    usage->current_virtual_memory *= (1024 * 1024 * 1024);
            }
        }

        if (strncmp("VmHWM:", line, 6) == 0) {
            checked += 1;
            if (sscanf(line, "%*s %lu %s\n", &usage->peak_physical_memory, unit) == 2) {
                if (strncasecmp("KB", unit, 2) == 0)
                    usage->peak_physical_memory *= 1024;

                if (strncasecmp("MB", unit, 2) == 0)
                    usage->peak_physical_memory *= (1024 * 1024);

                if (strncasecmp("GB", unit, 2) == 0)
                    usage->peak_physical_memory *= (1024 * 1024 * 1024);
            }
        }

        if (strncmp("VmRSS:", line, 6) == 0) {
            checked += 1;
            if (sscanf(line, "%*s %lu %s\n", &usage->current_physical_memory, unit) == 2) {
                if (strncasecmp("KB", unit, 2) == 0)
                    usage->current_physical_memory *= 1024;

                if (strncasecmp("MB", unit, 2) == 0)
                    usage->current_physical_memory *= (1024 * 1024);

                if (strncasecmp("GB", unit, 2) == 0)
                    usage->current_physical_memory *= (1024 * 1024 * 1024);
            }
        }

        if (checked >= 4)
            break;
    }

    fclose(fp);
}

size_t get_used_shared_memory() {
    shm_mgr *mgr = shm_mgr::get();
    return mgr ? mgr->used_size : 0;
}

size_t get_total_shared_memory() {
    shm_mgr *mgr = shm_mgr::get();
    return mgr ? mgr->total_size : 0;
}

size_t get_fd_limit() {
    struct rlimit limit;
    int ret = getrlimit(RLIMIT_NOFILE, &limit);
    return ret == 0 ? limit.rlim_cur : 0;
}

int set_fd_limit(size_t max) {
    struct rlimit limit;
    int ret = getrlimit(RLIMIT_NOFILE, &limit);
    if (ret != 0) return ret;

    limit.rlim_cur = max;
    if (max > limit.rlim_max)
        limit.rlim_max = max;

    return setrlimit(RLIMIT_NOFILE, &limit);
}

size_t get_opened_fd_count() {
    DIR *dp = opendir("/proc/self/fd");
    if (!dp) return 0;

    size_t count = 0;
    struct dirent *dirp = nullptr;
    while ((dirp = readdir(dp))) {
        if (strncmp(".", dirp->d_name, 1) != 0 &&
            strncmp("..", dirp->d_name, 2) != 0)
            ++count;
    }

    closedir(dp);
    return count;
}

size_t get_signal_queue_limit() {
    struct rlimit limit;
    int ret = getrlimit(RLIMIT_SIGPENDING, &limit);
    return ret == 0 ? limit.rlim_cur : 0;
}

int set_signal_queue_limit(size_t max) {
    struct rlimit limit;
    int ret = getrlimit(RLIMIT_SIGPENDING, &limit);
    if (ret != 0) return ret;

    limit.rlim_cur = max;
    if (max > limit.rlim_max)
        limit.rlim_max = max;

    return setrlimit(RLIMIT_SIGPENDING, &limit);
}

size_t get_queued_signal_count() {
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) return 0;

    size_t count = 0;
    char line[128] = {0};
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp("SigQ:", line, 5) == 0) {
            sscanf(line, "%*s %lu/%*s\n", &count);
            break;
        }
    }

    fclose(fp);
    return count;
}

NS_END(sk)
