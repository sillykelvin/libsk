#ifndef LIST_H
#define LIST_H

#include <shm/shm.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

struct list_node_base {
    typedef shm_ptr<list_node_base> base_pointer;

    list_node_base() {
        base_pointer s(self());
        prev = s;
        next = s;
    }

    base_pointer self() {
        base_pointer s(shm_ptr2addr(this));
        sk_assert(s);
        return s;
    }

    base_pointer prev;
    base_pointer next;
};

template<typename T>
struct list_node : public list_node_base {
    template<typename... Args>
    list_node(Args&&... args)
        : list_node_base(),
          value(std::forward<Args>(args)...) {}

    T value;
};

/**
 *  C: stands for constness, true makes this iterator a const iterator
 */
template<typename T, bool C>
struct list_iterator {
    typedef list_node<T>                          node_type;
    typedef list_iterator<T, C>                   self_type;
    typedef typename list_node_base::base_pointer base_pointer;

    /*
     * the following definitions are required by std::iterator_traits
     * when using algorithms defined in std, e.g. std::find_if(...)
     */
    typedef std::bidirectional_iterator_tag     iterator_category;
    typedef T                                   value_type;
    typedef ptrdiff_t                           difference_type;
    typedef typename if_<C, const T*, T*>::type pointer;
    typedef typename if_<C, const T&, T&>::type reference;

    list_iterator() : ptr(nullptr) {}
    explicit list_iterator(const base_pointer& p) : ptr(p) {}

    /*
     * this constructor enables implicit convertion: iterator -> const_iterator
     *
     * for the opposite convertion (const_iterator -> iterator), there will be
     * a compiler error, due to enable_if<false> is not defined
     */
    template<bool B, typename = typename enable_if<!B>::type>
    list_iterator(const list_iterator<T, B>& s) : ptr(s.ptr) {}

    reference operator*() const { return ptr.cast<node_type>()->value; }
    pointer operator->() const { return &(ptr.cast<node_type>()->value); }

    self_type& operator++() { ptr = ptr->next; return *this; }
    self_type operator++(int) { self_type self(*this); ++(*this); return self; }

    self_type& operator--() { ptr = ptr->prev; return *this; }
    self_type operator--(int) { self_type self(*this); --(*this); return self; }

    bool operator==(const self_type& x) const { return ptr == x.ptr; }
    bool operator!=(const self_type& x) const { return !(*this == x); }

    base_pointer ptr;
};

template<typename T, typename Allocator>
class list {
public:
    typedef list_node<T>                          node_type;
    typedef shm_ptr<node_type>                    node_pointer;
    typedef typename list_node_base::base_pointer base_pointer;
    typedef list_iterator<T, false>               iterator;
    typedef list_iterator<T, true>                const_iterator;

public:
    ~list() { clear(); }

    list() : sentinel_(dummy_.self()) {
        sk_assert(sentinel_ == dummy_.prev);
        sk_assert(sentinel_ == dummy_.next);
    }

    list(const list& that) : sentinel_(dummy_.self()) {
        sk_assert(sentinel_ == dummy_.prev);
        sk_assert(sentinel_ == dummy_.next);

        for (const_iterator it = that.begin(), end = that.end(); it != end; ++it) {
            int ret = push_back(*it);
            sk_assert(ret == 0);
        }
    }

    list& operator=(const list& that) {
        if (this != &that)
            assign(that.begin(), that.end());

        return *this;
    }

public:
    T *front() {
        check_retval(empty(), nullptr);
        return value(sentinel_->next);
    }

    const T *front() const {
        check_retval(empty(), nullptr);
        return value(sentinel_->next);
    }

    T *back() {
        check_retval(empty(), nullptr);
        return value(sentinel_->prev);
    }

    const T *back() const {
        check_retval(empty(), nullptr);
        return value(sentinel_->prev);
    }

    int push_back(const T& t) {
        base_pointer node = construct(t);
        if (!node) return -ENOMEM;

        link(node, sentinel_);
        return 0;
    }

    int push_front(const T& t) {
        base_pointer node = construct(t);
        if (!node) return -ENOMEM;

        link(node, sentinel_->next);
        return 0;
    }

    void pop_back() {
        assert_retnone(!empty());
        base_pointer node = sentinel_->prev;
        unlink(node);
        destruct(node);
    }

    void pop_front() {
        assert_retnone(!empty());
        base_pointer node = sentinel_->next;
        unlink(node);
        destruct(node);
    }

    template<typename... Args>
    T *emplace_back(Args&&... args) {
        base_pointer node = construct(std::forward<Args>(args)...);
        if (!node) return nullptr;

        link(node, sentinel_);
        return value(node);
    }

    template<typename... Args>
    T *emplace_front(Args&&... args) {
        base_pointer node = construct(std::forward<Args>(args)...);
        if (!node) return nullptr;

        link(node, sentinel_->next);
        return value(node);
    }

    int insert(const_iterator pos, const T& t) {
        base_pointer node = construct(t);
        if (!node) return -ENOMEM;

        link(node, pos.ptr);
        return 0;
    }

    template<typename... Args>
    T *emplace(const_iterator pos, Args&&... args) {
        base_pointer node = construct(std::forward<Args>(args)...);
        if (!node) return nullptr;

        link(node, pos.ptr);
        return value(node);
    }

    iterator erase(const_iterator pos) {
        assert_retval(pos != end(), end());
        base_pointer next = pos.ptr->next;
        unlink(pos.ptr);
        destruct(pos.ptr);
        return iterator(next);
    }

    void clear() {
        base_pointer node = sentinel_->next;
        while (node != sentinel_) {
            base_pointer next = node->next;
            unlink(node);
            destruct(node);
            node = next;
        }

        sk_assert(empty());
    }

    template<typename Iterator>
    void assign(Iterator first, Iterator last) {
        iterator it = this->begin();
        iterator end = this->end();
        for (; first != last && it != end; ++first, ++it)
            *it = *first;

        if (it == end) {
            for (; first != last; ++first) {
                int ret = push_back(*first);
                sk_assert(ret == 0);
            }
        } else {
            while (it != end)
                it = erase(it);
        }
    }

    iterator begin() { return iterator(sentinel_->next); }
    iterator end()   { return iterator(sentinel_); }
    const_iterator begin() const { return const_iterator(sentinel_->next); }
    const_iterator end()   const { return const_iterator(sentinel_); }

    bool full() const { return allocator_.full(); }
    bool empty() const { return allocator_.empty(); }
    size_t size() const { return allocator_.size(); }
    size_t capacity() const { return allocator_.capacity(); }
    size_t peak_size() const { return allocator_.peak_size(); }

private:
    template<typename... Args>
    base_pointer construct(Args&&... args) {
        shm_ptr<void> node = allocator_.allocate(sizeof(node_type));
        if (node) new (node.get()) node_type(std::forward<Args>(args)...);

        return node;
    }

    void destruct(const base_pointer& node) {
        assert_retnone(node != sentinel_);
        node_pointer ptr = node.cast<node_type>();
        ptr->~node_type();
        allocator_.deallocate(node);
    }

    void link(const base_pointer& node, const base_pointer& next) {
        node->next = next;
        node->prev = next->prev;
        next->prev->next = node;
        next->prev = node;
    }

    void unlink(const base_pointer& node) {
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }

    T *value(const base_pointer& node) {
        assert_retval(node != sentinel_, nullptr);
        node_pointer ptr = node.cast<node_type>();
        return &(ptr->value);
    }

    const T *value(const base_pointer& node) const {
        assert_retval(node != sentinel_, nullptr);
        node_pointer ptr = node.cast<node_type>();
        return &(ptr->value);
    }

private:
    Allocator allocator_;
    list_node_base dummy_;
    base_pointer sentinel_;
};

NS_END(detail)
NS_END(sk)

#endif // LIST_H
