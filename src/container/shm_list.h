#ifndef SHM_LIST_H
#define SHM_LIST_H

#include <iterator>

namespace sk {
namespace detail {

template<typename T>
struct shm_list_node {
    T data;
    shm_ptr<shm_list_node> prev;
    shm_ptr<shm_list_node> next;

    shm_list_node(const T& t) : data(t), prev(), next() {}
};

/*
 * Note:
 *   1. T should be a shm_list type here
 *   2. C stands for constness, true makes this iterator a const iterator
 */
template<typename T, bool C>
struct shm_list_iterator {
    typedef T                                                     list_type;
    typedef typename T::node                                      node_type;
    typedef typename T::value_type                                value_type;
    typedef ptrdiff_t                                             difference_type;
    typedef shm_list_iterator<T, C>                               self;
    typedef std::bidirectional_iterator_tag                       iterator_category;
    typedef typename if_<C, const list_type*, list_type*>::type   list_pointer;
    typedef typename if_<C, const node_type*, node_type*>::type   node_pointer;
    typedef typename if_<C, const value_type*, value_type*>::type pointer;
    typedef typename if_<C, const value_type&, value_type&>::type reference;

    list_pointer l;
    node_pointer n;

    explicit shm_list_iterator(list_pointer l) : l(l), n(NULL) {}
    shm_list_iterator(list_pointer l, node_pointer n) : l(l), n(n) {}

    /*
     * make a templated copy constructor here to support construction of
     * const iterator from mutable iterator
     */
    template<bool B>
    shm_list_iterator(const shm_list_iterator<T, B>& s) : l(s.l), n(s.n) {}

    self& operator=(const self& s) {
        if (this == &s)
            return *this;

        l = s.l;
        n = s.n;

        return *this;
    }

    reference operator*() const { return n->data; }
    pointer operator->() const { return &n->data; }

    self& operator++()   { if(n) n = n->next.get(); return *this; }
    self operator++(int) { self tmp(*this); if(n) n = n->next.get(); return tmp; }
    self& operator--()   { if (n) n = n->prev.get(); return *this; }
    self operator--(int) { self tmp(*this); if(n) n = n->prev.get(); return tmp; }

    bool operator==(const self& x) const { return n == x.n && l == x.l; }
    bool operator!=(const self& x) const { return !(*this == x); }

    bool operator==(const shm_list_iterator<T, !C>& x) const { return n == x.n && l == x.l; }
    bool operator!=(const shm_list_iterator<T, !C>& x) const { return !(*this == x); }
};

} // namespace detail

template<typename T>
struct shm_list {
    typedef T                                      value_type;
    typedef detail::shm_list_node<T>               node;
    typedef shm_ptr<node>                          pointer;
    typedef shm_list<T>                            self;
    typedef detail::shm_list_iterator<self, false> iterator;
    typedef detail::shm_list_iterator<self, true>  const_iterator;

    size_t count;
    pointer head;
    pointer tail;

    shm_list() : count(0), head(), tail() {}
    ~shm_list() { clear(); }

    bool empty() const {
        bool empty = count <= 0;

        if (empty)
            sk_assert(!head && !tail);
        else
            sk_assert(head && tail);

        return empty;
    }

    size_t size() const { return count; }

    T *front() {
        check_retval(!empty(), NULL);

        return &(head.get()->data);
    }

    const T *front() const {
        check_retval(!empty(), NULL);

        return &(head.get()->data);
    }

    T *back() {
        check_retval(!empty(), NULL);

        return &(tail.get()->data);
    }

    const T *back() const {
        check_retval(!empty(), NULL);

        return &(tail.get()->data);
    }

    void __erase(pointer p) {
        assert_retnone(p);

        pointer prev = p->prev;
        pointer next = p->next;

        if (prev)
            prev->next = p->next;
        else {
            sk_assert(head == p);
            head = p->next;
        }

        if (next)
            next->prev = p->prev;
        else {
            sk_assert(tail == p);
            tail = p->prev;
        }

        --count;
        shm_del(p);
    }

    pointer __ptr2shmptr(node *n) {
        check_retval(n, SHM_NULL);

        pointer p = SHM_NULL;
        if (n->prev)
            p = n->prev->next;
        else if (n->next)
            p = n->next->prev;
        else {
            sk_assert(count == 1 && head == tail);
            p = head;
        }

        sk_assert(p);
        return p;
    }

    int insert(iterator it, const T& t) {
        node *n = it.n;
        if (!n) // empty node, it must be the end() iterator
            return push_back(t);

        pointer old_node = __ptr2shmptr(it.n);
        assert_retval(old_node, -EFAULT);

        pointer new_node = shm_new<node>(t);
        check_retval(new_node, -ENOMEM);

        new_node->prev = old_node->prev;
        new_node->next = old_node;

        if (!old_node->prev)
            head = new_node;
        else
            old_node->prev->next = new_node;

        old_node->prev = new_node;

        ++count;
        return 0;
    }

    void erase(iterator it) {
        if (empty() || !it.n)
            return;

        pointer p = __ptr2shmptr(it.n);
        assert_retnone(p);

        __erase(p);
    }

    int push_back(const T& t) {
        pointer p = shm_new<node>(t);
        check_retval(p, -ENOMEM);

        p->prev = tail;

        if (tail)
            tail->next = p;

        tail = p;

        if (!head)
            head = p;

        ++count;
        return 0;
    }

    void pop_back() {
        if (!empty())
            __erase(tail);
    }

    int push_front(const T& t) {
        pointer p = shm_new<node>(t);
        check_retval(p, -ENOMEM);

        p->next = head;

        if (head)
            head->prev = p;

        head = p;

        if (!tail)
            tail = p;

        ++count;
        return 0;
    }

    void pop_front() {
        if (!empty())
            __erase(head);
    }

    void clear() {
        pointer p = head;
        while (p) {
            pointer next = p->next;
            shm_del(p);
            p = next;
        }

        count = 0;
        head = tail = SHM_NULL;
    }

    iterator begin() { return iterator(this, head.get()); }
    iterator end()   { return iterator(this); }
    const_iterator begin() const { return const_iterator(this, head.get()); }
    const_iterator end() const   { return const_iterator(this); }
};

} // namespace sk

#endif // SHM_LIST_H
