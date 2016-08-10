#ifndef BUS_H
#define BUS_H

#include "utility/types.h"

namespace sk {
namespace bus {

static const size_t DEFAULT_BUS_NODE_SIZE = 128;
static const size_t DEFAULT_BUS_NODE_COUNT = 102400;

/**
 * @brief register bus for process whose bus id is busid
 * @param bus_key: shm key of bus
 * @param busid: bus id of current process
 * @param fd: stores file descriptor of registered bus
 * @param node_size: size of a single data node
 * @param node_count: total count of data nodes
 * @return 0 if succeeds, error code otherwise
 */
int register_bus(key_t bus_key, int busid, int& fd,
                 size_t node_size = DEFAULT_BUS_NODE_SIZE,
                 size_t node_count = DEFAULT_BUS_NODE_COUNT);

/**
 * @brief send message through bus
 * @param fd: file descriptor returned by register_bus(...)
 * @param dst_busid: destination bus id of this message
 * @param data: message data
 * @param length: message length
 * @return 0 if succeeds, error code otherwise
 */
int send(int fd, int dst_busid, const void *data, size_t length);

/**
 * @brief recv one message from bus
 * @param fd: file descriptor returned by register_bus(...)
 * @param src_busid: stores the source bus id of this message
 * @param data: buffer to store message, must NOT be NULL
 * @param length: length of the buffer, also stores the length of the message
 * @return count of received message, should be 0 or 1, or negative error code
 */
int recv(int fd, int& src_busid, void *data, size_t& length);

} // namespace bus
} // namespace sk

#endif // BUS_H
