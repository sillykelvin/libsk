#ifndef SPAN_H
#define SPAN_H

#include <utility/assert_helper.h>
#include <shm/detail/shm_address.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

class span {
public:
    MAKE_NONCOPYABLE(span);

    span()
        : in_use_(0), size_class_(cast_u8(-1)),
          used_count_(0), start_page_(0), page_count_(0) {}

    span(shm_page_t start_page, size_t page_count)
        : in_use_(0), size_class_(cast_u8(-1)), used_count_(0),
          start_page_(start_page), page_count_(page_count) {}

public:
    bool in_use() const { return in_use_ == 1; }
    int size_class() const {return size_class_; }
    size_t used_count() const { return used_count_; }

    size_t page_count() const { return page_count_; }
    shm_page_t start_page() const { return start_page_; }
    shm_page_t last_page() const {
        assert_retval(page_count_ > 0, start_page_);
        return start_page_ + page_count_ - 1;
    }

    shm_address prev() const { return prev_span_; }
    shm_address next() const { return next_span_; }
    shm_address chunk_list() const { return chunk_list_; }

    void set_in_use(bool on) { in_use_ = on ? 1 : 0; }
    void set_start_page(shm_page_t page) { start_page_ = page; }
    void set_page_count(size_t count) { page_count_ = count; }

    shm_address addr() const {
        shm_address addr = shm_address::from_ptr(this);
        sk_assert(addr);
        return addr;
    }

    void partition(size_t bytes, u8 size_class);
    void erase();

    shm_address fetch();
    void recycle(shm_address chunk);

private:
    size_t in_use_     : 1;
    size_t size_class_ : 8;
    size_t used_count_ : 55;
    size_t start_page_;
    size_t page_count_;

    shm_address prev_span_;
    shm_address next_span_;
    shm_address chunk_list_;

    friend void span_list_init(shm_address list);
    friend bool span_list_empty(shm_address list);
    friend void span_list_remove(shm_address node);
    friend void span_list_prepend(shm_address list, shm_address node);
};

inline void span_list_init(shm_address list) {
    span *l = list.as<span>();
    l->prev_span_ = list;
    l->next_span_ = list;
}

inline bool span_list_empty(shm_address list) {
    span *l = list.as<span>();
    return l->next_span_ == list;
}

inline void span_list_remove(shm_address node) {
    span *n = node.as<span>();
    span *prev = n->prev_span_.as<span>();
    span *next = n->next_span_.as<span>();

    prev->next_span_ = n->next_span_;
    next->prev_span_ = n->prev_span_;
    n->prev_span_ = nullptr;
    n->next_span_ = nullptr;
}

inline void span_list_prepend(shm_address list, shm_address node) {
    span *l = list.as<span>();
    span *n = node.as<span>();
    span *next = l->next_span_.as<span>();

    assert_retnone(!n->prev_span_);
    assert_retnone(!n->next_span_);

    n->next_span_    = l->next_span_;
    n->prev_span_    = list;
    next->prev_span_ = node;
    l->next_span_    = node;
}

NS_END(detail)
NS_END(sk)

#endif // SPAN_H
