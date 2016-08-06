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

        seg.release();
        return 0;
    }

    virtual int on_fini() {
        if (mgr_) {
            shmctl(mgr_->shmid, IPC_RMID, 0);
            mgr_ = NULL;
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

            sk::detail::channel_descriptor& desc = mgr_->descriptors[i];
            sk::detail::channel *wc = mgr_->get_write_channel(i);
        }

        // TODO: peek message, send message, again and again...
        return 0;
    }

    virtual int on_reload() {
        return 0;
    }

private:
    sk::detail::channel_mgr *mgr_;
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
