#ifndef SPAN_H
#define SPAN_H

#include "libsk.h"

namespace sk {
namespace detail {

/**
 * A span represents a continuous set of page(s).
 */
struct span {
    bool in_use;  // this span is in use or not
    int count;    // page count
    page_t start; // the start page id
    shm_ptr<span> prev; // previous span in list
    shm_ptr<span> next; // next span in list

    void init(page_t start, int count);
};

} // namespace detail
} // namespace sk

#endif // SPAN_H
