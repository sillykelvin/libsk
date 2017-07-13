#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <uv.h>
#include <list>
#include <core/buffer.h>
#include <core/callback.h>
#include <core/inet_address.h>

NS_BEGIN(sk)

class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
public:
    static const int READ_EOF = UV_EOF;

    MAKE_NONCOPYABLE(tcp_connection);

    ~tcp_connection();

    int send(const void *data, size_t len);
    int recv();
    void close();

    const std::string& name() const { return name_; }
    const inet_address& remote_address() const { return remote_addr_; }

    bool incoming_buffer_empty() const { return incoming_.empty(); }
    bool outgoing_buffer_empty() const { return outgoing_.empty(); }

    void set_read_callback (const fn_on_read&  fn) { fn_on_read_  = fn; }
    void set_write_callback(const fn_on_write& fn) { fn_on_write_ = fn; }

private:
    struct write_request {
        // MUST put this field at the first position as
        // we do the following in the code:
        //     uv_write_t *w = ...;
        //     write_request *r = reinterpret_cast<write_request*>(w);
        uv_write_t req;
        buffer buf;

        write_request(tcp_connection* conn, const void *data, size_t len)
            : buf(data, len) {
            req.data = conn;
        }
    };
    using write_request_ptr = std::unique_ptr<write_request>;
    static_assert(std::is_standard_layout<write_request>::value, "write_request must be standard layout");

private:
    tcp_connection(uv_loop_t *loop, const uv_tcp_handle_ptr& peer,
                   const inet_address& remote_addr, const fn_on_close& fn);

    void update_name();

    void on_alloc(uv_buf_t *buf, size_t size_hint);
    static void on_alloc(uv_handle_t *handle, size_t size_hint, uv_buf_t *buf);

    void on_read(int nbytes, const uv_buf_t *buf);
    static void on_read(uv_stream_t *handle, ssize_t nbytes, const uv_buf_t *buf);

    void on_write(write_request *req, int status);
    static void on_write(uv_write_t *req, int status);

    void on_close();
    static void on_close(uv_handle_t *handle);

private:
    enum state { state_connected, state_disconnecting, state_disconnected };

    state state_;
    uv_loop_t *loop_;
    std::string name_;
    uv_tcp_handle_ptr handle_;
    inet_address remote_addr_;
    fn_on_close fn_on_close_;

    buffer incoming_;
    std::list<write_request_ptr> outgoing_;

    fn_on_read fn_on_read_;
    fn_on_write fn_on_write_;

    friend class tcp_server;
    friend class tcp_client;
};

NS_END(sk)

#endif // TCP_CONNECTION_H
