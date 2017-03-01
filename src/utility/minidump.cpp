#include <execinfo.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "minidump.h"
#include "log/log.h"

struct REG_SET {
    u64 rax;
    u64 rbx;
    u64 rcx;
    u64 rdx;
    u64 rsi;
    u64 rdi;
    u64 rbp;
    u64 rsp;
};

// can NOT use function here, bp/sp will be changed due to function call
#define RECORD_REGISTERS(reg)                     \
    do {                                          \
        asm ("movq %%rax, %0;" : "=m"(reg.rax));  \
        asm ("movq %%rbx, %0;" : "=m"(reg.rbx));  \
        asm ("movq %%rcx, %0;" : "=m"(reg.rcx));  \
        asm ("movq %%rdx, %0;" : "=m"(reg.rdx));  \
        asm ("movq %%rsi, %0;" : "=m"(reg.rsi));  \
        asm ("movq %%rdi, %0;" : "=m"(reg.rdi));  \
        asm ("movq %%rbp, %0;" : "=m"(reg.rbp));  \
        asm ("movq %%rsp, %0;" : "=m"(reg.rsp));  \
    } while (0)                                   \

static void dump_registers(FILE *fp, const REG_SET *reg) {
    fprintf(fp, "[registers]\n");
    fprintf(fp, "rax %016lx\n", reg->rax);
    fprintf(fp, "rbx %016lx\n", reg->rbx);
    fprintf(fp, "rcx %016lx\n", reg->rcx);
    fprintf(fp, "rdx %016lx\n", reg->rdx);
    fprintf(fp, "rsi %016lx\n", reg->rsi);
    fprintf(fp, "rdi %016lx\n", reg->rdi);
    fprintf(fp, "rbp %016lx\n", reg->rbp);
    fprintf(fp, "rsp %016lx\n", reg->rsp);
    fprintf(fp, "\n");
}

static void dump_proc_section(FILE *fp, pid_t pid, const char *section_name) {
    fprintf(fp, "[%s]\n", section_name);

    char path[256] = {0};
    snprintf(path, sizeof(path) - 1, "/proc/%d/%s", pid, section_name);

    FILE *fp_proc = fopen(path, "r");
    if (!fp_proc) {
        fprintf(fp, "read section %s failed\n", section_name);
        return;
    }

    char buff[1024] = {0};
    size_t num_read = fread(buff, 1, sizeof(buff) - 1, fp_proc);
    fwrite(buff, 1, num_read, fp);

    fprintf(fp, "\n");
    if (num_read > 0 && buff[num_read - 1] != '\n')
        fprintf(fp, "\n");

    fclose(fp_proc);
}

static void dump_backtrace(FILE *fp) {
    fprintf(fp, "[backtrace]\n");

    void *array[20];
    size_t size = backtrace(array, 20);
    char **strings = backtrace_symbols(array, size);

    for (size_t i = 0; i < size; ++i)
        fprintf(fp, "%s\n", strings[i]);

    free(strings);

    fprintf(fp, "\n");
}

FILE *create_minidump_file(pid_t pid) {
    char path[256] = {0};
    snprintf(path, sizeof(path) - 1, "/proc/%d/exe", pid);

    char buff[1024] = {0};
    ssize_t sz = readlink(path, buff, sizeof(buff));
    if (sz == -1) return NULL;

    time_t t = time(NULL);
    struct tm *local = localtime(&t);

    snprintf(path, sizeof(path) - 1,
             "%s_%04d%02d%02d_%02d%02d%02d.minidump",
             basename(buff),
             local->tm_year + 1900,
             local->tm_mon + 1,
             local->tm_mday,
             local->tm_hour,
             local->tm_min,
             local->tm_sec);

    return fopen(path, "w+");
}

static void minidump_on_sig(int sig) {
    // NOTE: registers must be dumped first due to
    //       any other operations might change them
    REG_SET reg;
    RECORD_REGISTERS(reg);

    pid_t pid = getpid();
    FILE *fp = create_minidump_file(pid);
    if (!fp) {
        sk_fatal("failed to create minidump file for signal %d", sig);
        return;
    }

    fprintf(fp, "dump by signal %d\n\n", sig);
    dump_proc_section(fp, pid, "cmdline");
    dump_proc_section(fp, pid, "stat");
    dump_proc_section(fp, pid, "statm");
    dump_backtrace(fp);
    dump_registers(fp, &reg);

    fclose(fp);

    exit(-1);
}

namespace sk {

int enable_minidump() {
    struct sigaction act;
    memset(&act, 0x00, sizeof(act));

    act.sa_handler = minidump_on_sig;

    int ret = 0;

    ret = sigaction(SIGBUS, &act, NULL);
    if (ret != 0) {
        sk_error("sigaction error, signal: SIGBUS. error: %s", strerror(errno));
        return ret;
    }

    ret = sigaction(SIGSEGV, &act, NULL);
    if (ret != 0) {
        sk_error("sigaction error, signal: SIGSEGV. error: %s", strerror(errno));
        return ret;
    }

    ret = sigaction(SIGABRT, &act, NULL);
    if (ret != 0) {
        sk_error("sigaction error, signal: SIGABRT. error: %s", strerror(errno));
        return ret;
    }

    return 0;
}

} // namespace sk
