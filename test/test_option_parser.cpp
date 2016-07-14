#include <gtest/gtest.h>
#include "libsk.h"

using namespace sk;

struct context {
    bool append;
    std::string binary;
    int create;
    u32 del;
    s64 erase;
    u64 find;

    context() : append(false), create(0), del(0), erase(0), find(0) {}
};

static int init(option_parser *parser, context *ctx) {
    int ret = 0;

    ret = parser->register_option('a', "append", "append something", false, &ctx->append);
    if (ret != 0) return ret;

    ret = parser->register_option('b', "binary", "binary file location", true, &ctx->binary);
    if (ret != 0) return ret;

    ret = parser->register_option('c', NULL, "create something", true, &ctx->create);
    if (ret != 0) return ret;

    ret = parser->register_option(0, "delete-from", "where to delete", false, &ctx->del);
    if (ret != 0) return ret;

    ret = parser->register_option('e', "erase-at", "erase index", false, &ctx->erase);
    if (ret != 0) return ret;

    ret = parser->register_option('f', "find-right-place", "find right place", false, &ctx->find);
    if (ret != 0) return ret;

    return 0;
}

TEST(option_parser, normal) {
    {
        option_parser p;
        context ctx;
        int ret = init(&p, &ctx);
        ASSERT_TRUE(ret == 0);

        int argc = 11;
        const char *argv[] = {"./test", "-b", "/path/to/binary", "-c", "1", "-e", "3", "--delete-from=2", "-f", "4", "-a"};

        ret = p.parse(argc, argv);
        ASSERT_TRUE(ret == 0);
        ASSERT_TRUE(ctx.append == true);
        ASSERT_TRUE(ctx.binary == "/path/to/binary");
        ASSERT_TRUE(ctx.create == 1);
        ASSERT_TRUE(ctx.del == 2);
        ASSERT_TRUE(ctx.erase == 3);
        ASSERT_TRUE(ctx.find == 4);
    }

    {
        option_parser p;
        context ctx;
        int ret = init(&p, &ctx);
        ASSERT_TRUE(ret == 0);

        int argc = 8;
        const char *argv[] = {"./test", "-c", "1", "--delete-from=2", "--binary=/path/to/binary", "--erase-at=3", "--append", "--find-right-place=4"};

        ret = p.parse(argc, argv);
        ASSERT_TRUE(ret == 0);
        ASSERT_TRUE(ctx.append == true);
        ASSERT_TRUE(ctx.binary == "/path/to/binary");
        ASSERT_TRUE(ctx.create == 1);
        ASSERT_TRUE(ctx.del == 2);
        ASSERT_TRUE(ctx.erase == 3);
        ASSERT_TRUE(ctx.find == 4);
    }

    {
        option_parser p;
        context ctx;
        int ret = init(&p, &ctx);
        ASSERT_TRUE(ret == 0);

        int argc = 7;
        const char *argv[] = {"./test", "-ab", "/path/to/binary", "-c", "1", "--delete-from=2", "--find-right-place=4"};

        ret = p.parse(argc, argv);
        ASSERT_TRUE(ret == 0);
        ASSERT_TRUE(ctx.append == true);
        ASSERT_TRUE(ctx.binary == "/path/to/binary");
        ASSERT_TRUE(ctx.create == 1);
        ASSERT_TRUE(ctx.del == 2);
        ASSERT_TRUE(ctx.erase == 0);
        ASSERT_TRUE(ctx.find == 4);
    }

    {
        option_parser p;
        context ctx;
        int ret = init(&p, &ctx);
        ASSERT_TRUE(ret == 0);

        int argc = 4;
        const char *argv[] = {"./test", "--binary=/path/to/binary", "-c", "1"};

        ret = p.parse(argc, argv);
        ASSERT_TRUE(ret == 0);
        ASSERT_TRUE(ctx.append == false);
        ASSERT_TRUE(ctx.binary == "/path/to/binary");
        ASSERT_TRUE(ctx.create == 1);
        ASSERT_TRUE(ctx.del == 0);
        ASSERT_TRUE(ctx.erase == 0);
        ASSERT_TRUE(ctx.find == 0);
    }

    {
        option_parser p;
        context ctx;
        int ret = init(&p, &ctx);
        ASSERT_TRUE(ret == 0);

        ret = p.register_option('a', NULL, "duplicate", false, &ctx.binary);
        ASSERT_TRUE(ret != 0);

        ret = p.register_option(0, "binary", "duplicate", true, &ctx.append);
        ASSERT_TRUE(ret != 0);

        int argc = 8;
        const char *argv[] = {"./test", "--binary=/path/to/binary", "-c", "1", "-x", "1", "-q", "--not-exists=true"};

        ret = p.parse(argc, argv);
        ASSERT_TRUE(ret == 0);
        ASSERT_TRUE(ctx.append == false);
        ASSERT_TRUE(ctx.binary == "/path/to/binary");
        ASSERT_TRUE(ctx.create == 1);
        ASSERT_TRUE(ctx.del == 0);
        ASSERT_TRUE(ctx.erase == 0);
        ASSERT_TRUE(ctx.find == 0);
    }

    {
        option_parser p;
        context ctx;
        int ret = init(&p, &ctx);
        ASSERT_TRUE(ret == 0);

        int argc = 3;
        const char *argv[] = {"./test", "-abc", "1"};

        ret = p.parse(argc, argv);
        ASSERT_TRUE(ret != 0);
    }

    {
        option_parser p;
        context ctx;
        int ret = init(&p, &ctx);
        ASSERT_TRUE(ret == 0);

        int argc = 4;
        const char *argv[] = {"./test", "-ac", "1", "-b"};

        ret = p.parse(argc, argv);
        ASSERT_TRUE(ret != 0);
    }
}
