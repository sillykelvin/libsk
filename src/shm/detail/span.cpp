#include "span.h"

namespace sk {
namespace detail {

void span::init(page_t start, int count) {
    this->in_use = false;
    this->start  = start;
    this->count  = count;
    this->prev   = SHM_NULL;
    this->next   = SHM_NULL;
}

} // namespace detail
} // namespace sk
