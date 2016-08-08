#include "busd.h"
#include "server/server.h"
#include "shm/detail/shm_segment.h"
#include "bus/detail/channel_mgr.h"
#include "bus/detail/channel.h"
#include "lock_guard.h"

struct busd_config {
    int shm_key;     // where channel_mgr to be stored
    size_t shm_size; // size of the shm segment

    int load_from_xml_file(const char *filename) {
        // TODO: generate this function automatically
        return 0;
    }
};

class busd;
typedef sk::server<busd_config, busd> server_type;
class busd : public server_type {
protected:
    virtual int on_init() {
        int ret = 0;
        const busd_config& conf = config();
        const sk::server_context& ctx = context();

        sk::detail::shm_segment seg;
        ret = seg.init(conf.shm_key, conf.shm_size, ctx.resume_mode);
        if (ret != 0) return ret;

        mgr_ = seg.address();
        ret = mgr_->init(seg.shmid, conf.shm_size, ctx.resume_mode);
        if (ret != 0) return ret;

        buffer_len_ = 2 * 1024 * 1024;
        buffer_ = malloc(buffer_len_);
        if (!buffer_) return -ENOMEM;

        seg.release();
        return 0;
    }

    virtual int on_fini() {
        if (mgr_) {
            shmctl(mgr_->shmid, IPC_RMID, 0);
            mgr_ = NULL;
        }

        if (buffer_) {
            free(buffer_);
            buffer_ = NULL;
        }

        return 0;
    }

    virtual int on_stop() {
        // TODO: should not stop here if there is message in any channel
        return 0;
    }

    virtual int on_run() {
        sk::lock_guard(mgr_->lock);

        // process 200 messages in one run
        const int total_count = 200;
        int count = 0;
        for (int i = 0; i < mgr_->descriptor_count; ++i) {
            if (count >= total_count) break;

            const sk::detail::channel_descriptor& desc = mgr_->descriptors[i];
            sk::detail::channel *wc = mgr_->get_write_channel(i);

            size_t len = buffer_len_;
            int src_busid = 0;
            int dst_busid = 0;
            int ret = wc->pop(buffer_, len, &src_busid, &dst_busid);
            if (ret < 0) {
                // TODO: more robust handling here, check the return code
                // if it is because the buffer is too small, then enlarge
                // the buffer should work
                error("pop message error, ret<%d>, fd<%d>.", ret, i);
                continue;
            }

            if (ret == 0) continue; // no message in this channel

            if (ret == 1) {
                count += 1;

                if (src_busid != desc.owner)
                    warn("message bus<%x>, channel bus<%x> mismatch.", src_busid, desc.owner);

                sk::detail::channel *rc = mgr_->find_read_channel(dst_busid);
                if (!rc) {
                    // TODO: add relay feature later if the destination
                    // process is not on the same host
                    error("cannot find channel, bus<%x>.", dst_busid);
                    continue;
                }

                int ret2 = rc->push(src_busid, dst_busid, buffer_, len);
                if (ret2 != 0) {
                    // TODO: more robust handling here if the channel is full
                    error("push message error, ret<%d>, bus<%x>.", ret2, dst_busid);
                }

                continue;
            }

            // it should NOT get here
            sk_assert(0);
        }

        // if count < total_count, then all messages have been processed, so
        // return 0 to increase the idle count
        return count < total_count ? 0 : 1;
    }

    virtual int on_reload() {
        return 0;
    }

private:
    sk::detail::channel_mgr *mgr_;
    void *buffer_;
    size_t buffer_len_;
};


int main(int argc, char **argv) {
    server_type& s = server_type::get();
    int ret = 0;

    ret = s.init(argc, argv);
    if (ret != 0) return ret;

    s.run();

    ret = s.fini();
    if (ret != 0) return ret;

    return 0;
}
