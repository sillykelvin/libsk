#ifndef BUS_H
#define BUS_H

#include <signal.h>
#include <string>
#include "utility/types.h"

NS_BEGIN(sk)
NS_BEGIN(bus)

static const key_t  DEFAULT_BUS_KEY        = 1799;
static const size_t DEFAULT_BUS_NODE_SIZE  = 128;
static const size_t DEFAULT_BUS_NODE_COUNT = 102400;
static const s32    BUS_MESSAGE_SIGNO      = SIGRTMIN + 7;
static const s32    BUS_REGISTRATION_SIGNO = SIGRTMIN + 8;

/**
 * @brief parse a bus id from string
 * @param str: must be the format "x.x.x.x", where "x" is a number
 * @param area_id: stores parsed area id
 * @param zone_id: stores parsed zone id
 * @param func_id: stores parsed function id
 * @param inst_id: stores parsed instance id
 * @return bus id if succeeded, -1 on parsing error
 */
int from_string(const char *str,
                int *area_id = nullptr, int *zone_id = nullptr,
                int *func_id = nullptr, int *inst_id = nullptr);

/**
 * @brief return a bus id by the given sub ids
 * @param area_id: area id
 * @param zone_id: zone id
 * @param func_id: function id
 * @param inst_id: instance id
 * @return bus id, this function never fail
 */
int from_subid(int area_id, int zone_id, int func_id, int inst_id);

/**
 * @brief convert a bus id to string
 * @param bus_id
 * @return the converted string
 */
std::string to_string(int bus_id);

/**
 * @brief bus_id
 * @return bus id of current process
 */
int bus_id();

/**
 * @brief area_id
 * @return area id of current process
 */
int area_id();

/**
 * @brief zone_id
 * @return zone id of current process
 */
int zone_id();

/**
 * @brief func_id
 * @return function id of current process
 */
int func_id();

/**
 * @brief inst_id
 * @return instance id of current process
 */
int inst_id();

/**
 * @brief area_id
 * @param bus_id
 * @return area id stored in bus_id
 */
int area_id(int bus_id);

/**
 * @brief zone_id
 * @param bus_id
 * @return zone id stored in bus_id
 */
int zone_id(int bus_id);

/**
 * @brief func_id
 * @param bus_id
 * @return function id stored in bus_id
 */
int func_id(int bus_id);

/**
 * @brief inst_id
 * @param bus_id
 * @return instance id stored in bus_id
 */
int inst_id(int bus_id);

/**
 * @brief register bus for process whose bus id is busid
 * @param bus_key: shm key of bus
 * @param busid: bus id of current process
 * @param node_size: size of a single data node
 * @param node_count: total count of data nodes
 * @return 0 if succeeds, error code otherwise
 */
int register_bus(key_t bus_key, int busid,
                 size_t node_size = DEFAULT_BUS_NODE_SIZE,
                 size_t node_count = DEFAULT_BUS_NODE_COUNT);

/**
 * @brief deregister bus for current process
 */
void deregister_bus();

/**
 * @brief send message through bus
 * @param dst_busid: destination bus id of this message
 * @param data: message data
 * @param length: message length
 * @return 0 if succeeds, error code otherwise
 */
int send(int dst_busid, const void *data, size_t length);

/**
 * @brief recv one message from bus
 * @param src_busid: stores the source bus id of this message
 * @param data: buffer to store message, must NOT be null
 * @param length: length of the buffer, also stores the length of the message
 * @return count of received message, should be 0 or 1, or negative error code
 */
int recv(int& src_busid, void *data, size_t& length);

NS_END(bus)
NS_END(sk)

#endif // BUS_H
