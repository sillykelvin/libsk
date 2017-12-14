#ifndef RBTREE_H
#define RBTREE_H

#include <shm/shm.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

struct rbtree_node_base {
    typedef shm_ptr<rbtree_node_base> base_pointer;

    rbtree_node_base() = default;

    base_pointer self() {
        base_pointer s(shm_ptr2addr(this));
        sk_assert(s);
        return s;
    }

    bool is_red() const { return red; }

    base_pointer right;
    base_pointer left;
    base_pointer parent;
    bool         red;
};

template<typename T>
struct rbtree_node : public rbtree_node_base {
    template<typename... Args>
    rbtree_node(Args&&... args)
        : rbtree_node_base(),
          value(std::forward<Args>(args)...) {}

    T value;
};

struct rbtree_algorithm {
    typedef typename rbtree_node_base::base_pointer base_pointer;

    static bool is_red(base_pointer n) {
        return n && n->is_red();
    }

    static bool is_left_child(base_pointer n) {
        return n == n->parent->left;
    }

    // t -> top, b -> bottom
    static size_t black_count(base_pointer t, base_pointer b) {
        size_t count = 0;

        for (; b; b = b->parent) {
            if (!is_red(b)) ++count;
            if (b == t) break;
        }

        return count;
    }

    static void rotate_left(base_pointer n, base_pointer& root) {
        base_pointer p  = n->parent;
        base_pointer r  = n->right;
        base_pointer rl = r->left;

        n->right = rl;
        if (rl) rl->parent = n;

        r->parent = p;

        if (n == root)
            root = r;
        else if (is_left_child(n))
            p->left = r;
        else
            p->right = r;

        r->left = n;
        n->parent = r;
    }

    static void rotate_right(base_pointer n, base_pointer& root) {
        base_pointer p  = n->parent;
        base_pointer l  = n->left;
        base_pointer lr = l->right;

        n->left = lr;
        if (lr) lr->parent = n;

        l->parent = p;

        if (n == root)
            root = l;
        else if (!is_left_child(n))
            p->right = l;
        else
            p->left = l;

        l->right = n;
        n->parent = l;
    }

    static base_pointer increment(base_pointer n) {
        if (n->right) {
            n = n->right;
            while (n->left)
                n = n->left;
        } else {
            base_pointer p = n->parent;
            while (n == p->right) {
                n = p;
                p = p->parent;
            }

            if (n->right != p)
                n = p;
        }

        return n;
    }

    static base_pointer decrement(base_pointer n) {
        if (n->parent->parent == n && is_red(n))
            return n->right;

        if (n->left) {
            base_pointer l = n->left;
            while (l->right)
                l = l->right;

            return l;
        }

        base_pointer p = n->parent;
        while (n == p->left) {
            n = p;
            p = p->parent;
        }

        return p;
    }

    static base_pointer min_child(base_pointer n) {
        while (n->left) n = n->left;
        return n;
    }

    static base_pointer max_child(base_pointer n) {
        while (n->right) n = n->right;
        return n;
    }

    // n -> node, p -> parent, s -> sentinel
    static void insert(base_pointer n, base_pointer p,
                       base_pointer s, bool insert_left) {
        base_pointer& root = s->parent;

        n->parent = p;
        n->right  = nullptr;
        n->left   = nullptr;
        n->red    = true;

        if (insert_left) {
            p->left = n;

            if (p == s) {
                s->parent = n;
                s->right = n;
            } else if (p == s->left) {
                s->left = n;
            }
        } else {
            p->right = n;
            if (p == s->right)
                s->right = n;
        }

        while (n != root && is_red(n->parent)) {
            base_pointer pp = n->parent->parent;

            if (n->parent == pp->left) {
                base_pointer r = pp->right;
                if (r && is_red(r)) {
                    n->parent->red = false;
                    r->red = false;
                    pp->red = true;
                    n = pp;
                } else {
                    if (n->parent && !is_left_child(n)) {
                        n = n->parent;
                        rotate_left(n, root);
                    }

                    n->parent->red = false;
                    pp->red = true;
                    rotate_right(pp, root);
                }
            } else {
                base_pointer l = pp->left;
                if (l && is_red(l)) {
                    n->parent->red = false;
                    l->red = false;
                    pp->red = true;
                    n = pp;
                } else {
                    if (is_left_child(n)) {
                        n = n->parent;
                        rotate_right(n, root);
                    }

                    n->parent->red = false;
                    pp->red = true;
                    rotate_left(pp, root);
                }
            }
        }

        root->red = false;
    }

    // n -> node, s -> sentinel
    static void erase(base_pointer n, base_pointer s) {
        base_pointer& root      = s->parent;
        base_pointer& leftmost  = s->left;
        base_pointer& rightmost = s->right;

        base_pointer x = n;
        base_pointer c = nullptr;
        base_pointer p = nullptr;

        if (!x->left)
            c = x->right;
        else if (!x->right)
            c = x->left;
        else {
            x = x->right;
            while (x->left)
                x = x->left;

            c = x->right;
        }

        if (x == n) {
            p = x->parent;
            if (c) c->parent = x->parent;

            if (n == root)
                root = c;
            else {
                if (is_left_child(n))
                    n->parent->left = c;
                else
                    n->parent->right = c;
            }

            if (n == leftmost) {
                if (n->right) {
                    sk_assert(c);
                    leftmost = min_child(c);
                } else {
                    leftmost = n->parent;
                }
            }

            if (n == rightmost) {
                if (n->left) {
                    sk_assert(c);
                    rightmost = max_child(c);
                } else {
                    rightmost = n->parent;
                }
            }
        } else {
            n->left->parent = x;
            x->left = n->left;

            if (x == n->right)
                p = x;
            else {
                p = x->parent;
                if (c) c->parent = p;

                p->left = c;

                x->right = n->right;
                n->right->parent = x;
            }

            if (n == root)
                root = x;
            else if (is_left_child(n))
                n->parent->left = x;
            else
                n->parent->right = x;

            x->parent = n->parent;
            std::swap(x->red, n->red);
        }

        if (!is_red(n)) {
            while (c != root && !is_red(c)) {
                if (c == p->left) {
                    base_pointer r = p->right;
                    if (is_red(r)) {
                        r->red = false;
                        p->red = true;
                        rotate_left(p, root);
                        r = p->right;
                    }

                    if (!is_red(r->left) && !is_red(r->right)) {
                        r->red = true;
                        c = p;
                        p = p->parent;
                    } else {
                        if (!is_red(r->right)) {
                            r->left->red = false;
                            r->red = true;
                            rotate_right(r, root);
                            r = p->right;
                        }

                        r->red = p->red;
                        p->red = false;

                        if (r->right)
                            r->right->red = false;

                        rotate_left(p, root);
                        break;
                    }
                } else {
                    base_pointer l = p->left;
                    if (is_red(l)) {
                        l->red = false;
                        p->red = true;
                        rotate_right(p, root);
                        l = p->left;
                    }

                    if (!is_red(l->right) && !is_red(l->left)) {
                        l->red = true;
                        c = p;
                        p = p->parent;
                    } else {
                        if (!is_red(l->left)) {
                            l->right->red = false;
                            l->red = true;
                            rotate_left(l, root);
                            l = p->left;
                        }

                        l->red = p->red;
                        p->red = false;

                        if (l->left)
                            l->left->red = false;

                        rotate_right(p, root);
                        break;
                    }
                }
            }

            if (c) c->red = false;
        }
    }
};

template<typename T, bool C>
struct rbtree_iterator {
    typedef rbtree_node<T>                          node_type;
    typedef rbtree_iterator<T, C>                   self_type;
    typedef typename rbtree_node_base::base_pointer base_pointer;

    /*
     * the following definitions are required by std::iterator_traits
     * when using algorithms defined in std, e.g. std::find_if(...)
     */
    typedef std::bidirectional_iterator_tag     iterator_category;
    typedef T                                   value_type;
    typedef ptrdiff_t                           difference_type;
    typedef typename if_<C, const T*, T*>::type pointer;
    typedef typename if_<C, const T&, T&>::type reference;

    rbtree_iterator() : ptr(nullptr) {}
    explicit rbtree_iterator(base_pointer p) : ptr(p) {}

    /*
     * this constructor enables implicit convertion: iterator -> const_iterator
     *
     * for the opposite convertion (const_iterator -> iterator), there will be
     * a compiler error, due to enable_if<false> is not defined
     */
    template<bool B, typename = typename enable_if<!B>::type>
    rbtree_iterator(const rbtree_iterator<T, B>& s) : ptr(s.ptr) {}

    reference operator*() const { return ptr.cast<node_type>()->value; }
    pointer operator->() const { return &(ptr.cast<node_type>()->value); }

    self_type& operator++() { ptr = rbtree_algorithm::increment(ptr); return *this; }
    self_type operator++(int) { self_type self(*this); ++(*this); return self; }

    self_type& operator--() { ptr = rbtree_algorithm::decrement(ptr); return *this; }
    self_type operator--(int) { self_type self(*this); --(*this); return self; }

    bool operator==(const self_type& x) const { return ptr == x.ptr; }
    bool operator!=(const self_type& x) const { return !(*this == x); }

    base_pointer ptr;
};

template<typename K, typename V, bool Constness,
         typename Compare, typename Allocator, typename Extractor>
class rbtree {
public:
    typedef rbtree_node<V>                          node_type;
    typedef shm_ptr<node_type>                      node_pointer;
    typedef typename rbtree_node_base::base_pointer base_pointer;
    typedef rbtree_iterator<V, Constness>           iterator;
    typedef rbtree_iterator<V, true>                const_iterator;
    typedef std::reverse_iterator<iterator>         reverse_iterator;
    typedef std::reverse_iterator<const_iterator>   const_reverse_iterator;

public:
    ~rbtree() { clear(); }
    rbtree() : sentinel_(dummy_.self()) { reset_sentinel(); }

    // TODO: implementation here
    rbtree(const rbtree& that) = delete;
    rbtree& operator=(const rbtree& that) = delete;

public:
    void clear() {
        size_t total_count = size();
        size_t clear_count = clear_subtree(sentinel_->parent);
        sk_assert(total_count == clear_count);

        reset_sentinel();
    }

    int insert(const V& value) {
        Extractor extractor;
        K key(extractor(value));

        bool exists = false;
        base_pointer pos = search_position(key, &exists);
        if (exists) return 0;

        if (full()) return -ENOMEM;

        base_pointer node = construct(value);
        if (unlikely(!node)) return -ENOMEM;

        insert_child(pos, key, node);
        return 0;
    }

    template<typename... Args>
    V *emplace(Args&&... args) {
        V value(std::forward<Args>(args)...);

        Extractor extractor;
        K key(extractor(value));

        bool exists = false;
        base_pointer pos = search_position(key, &exists);
        if (exists) return this->value(pos);

        if (full()) return nullptr;

        base_pointer node = construct(std::move(value));
        if (unlikely(!node)) return nullptr;

        insert_child(pos, key, node);
        return this->value(node);
    }

    void erase(const_iterator it) {
        if (it == end()) return;

        erase_node(it.ptr);
    }

    void erase(const K& key) {
        iterator it = find(key);
        if (it == end()) return;

        erase_node(it.ptr);
    }

    iterator find(const K& key) {
        Compare compare;
        Extractor extractor;

        base_pointer n = sentinel_->parent;
        base_pointer e = sentinel_;

        while (n) {
            V *value = this->value(n);
            if (!compare(extractor(*value), key)) {
                e = n;
                n = n->left;
            } else {
                sk_assert(!compare(key, extractor(*value)));
                n = n->right;
            }
        }

        if (e != sentinel_ && !compare(key, extractor(*value(e))))
            return iterator(e);

        return iterator(sentinel_);
    }

    const_iterator find(const K& key) const {
        return const_iterator(const_cast<rbtree>(this)->find(key));
    }

    iterator begin() { return iterator(sentinel_->left); }
    iterator end()   { return iterator(sentinel_); }
    const_iterator begin() const { return const_iterator(sentinel_->left); }
    const_iterator end()   const { return const_iterator(sentinel_); }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend()   { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend()   const { return const_reverse_iterator(begin()); }

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

    void destruct(base_pointer node) {
        assert_retnone(node != sentinel_);
        node_pointer ptr = node.cast<node_type>();
        ptr->~node_type();
        allocator_.deallocate(node);
    }

    base_pointer search_position(const K& key, bool *exists) {
        Compare compare;
        Extractor extractor;

        base_pointer n = sentinel_->parent;
        base_pointer l = sentinel_;
        base_pointer p = nullptr;
        bool lt = true;

        while (n) {
            V *value = this->value(n);
            lt = compare(key, extractor(*value));
            l = n;

            if (lt) {
                // check if the compare function is sane
                sk_assert(!compare(extractor(*value), key));
                n = n->left;
            } else {
                n = n->right;
            }
        }

        p = l;

        if (lt) {
            if (l != sentinel_->left)
                l = rbtree_algorithm::decrement(l);
            else {
                if (exists) *exists = false;
                return l;
            }
        }

        if (compare(extractor(*this->value(l)), key)) {
            sk_assert(!compare(key, extractor(*this->value(l))));
            if (exists) *exists = false;
            return p;
        }

        if (exists) *exists = true;
        return l;
    }

    void insert_child(base_pointer parent, const K& key, base_pointer node) {
        Compare compare;
        Extractor extractor;

        bool insert_left = true;
        if (parent != sentinel_)
            insert_left = compare(key, extractor(*value(parent)));

        rbtree_algorithm::insert(node, parent, sentinel_, insert_left);
        verify_properties();
    }

    void erase_node(base_pointer node) {
        rbtree_algorithm::erase(node, sentinel_);
        destruct(node);
        verify_properties();
    }

    size_t clear_subtree(base_pointer node) {
        if (!node) return 0;
        size_t l = clear_subtree(node->left);
        size_t r = clear_subtree(node->right);
        destruct(node);
        return l + r + 1;
    }

    void reset_sentinel() {
        sk_assert(sentinel_.get() == &dummy_);
        dummy_.right  = sentinel_;
        dummy_.left   = sentinel_;
        dummy_.parent = nullptr;
        dummy_.red    = true;
    }

    void verify_properties() {
#ifndef NDEBUG
        /*
         * red-black trees have the following canonical properties:
         *
         * 0. a node's color is either red or black, this is true by default as
         *    we defines the color as a boolean value
         * 1. root node must be black
         * 2. all leaves (null node) are black
         * 3. every red node must have two black children
         * 4. every path from a node to all leaves has same count of black nodes
         *
         * except the above ones, we also verify the following:
         *
         * 5. the size() of the tree must equal to the count of nodes in the tree
         * 6. the tree is sorted as per a conventional binary search tree
         * 7. the compare function is sane, it obeys strict weak ordering
         */

        base_pointer root      = sentinel_->parent;
        base_pointer leftmost  = sentinel_->left;
        base_pointer rightmost = sentinel_->right;

        // verify #1
        sk_assert(!rbtree_algorithm::is_red(root));

        // verify #2
        sk_assert(!rbtree_algorithm::is_red(base_pointer(nullptr)));

        if (empty()) {
            sk_assert(size() <= 0);
            sk_assert(leftmost == sentinel_ && rightmost == sentinel_);
        } else {
            sk_assert(root);
            sk_assert(leftmost  == rbtree_algorithm::min_child(root));
            sk_assert(rightmost == rbtree_algorithm::max_child(root));

            Compare compare;
            Extractor extractor;
            size_t black_count = rbtree_algorithm::black_count(root, leftmost);
            size_t loop_count  = 0;

            for (const_iterator it = begin(); it != end(); ++it, ++loop_count) {
                base_pointer n = it.ptr;
                base_pointer l = n->left;
                base_pointer r = n->right;

                // verify #7
                if (r) {
                    bool rn = compare(extractor(*value(r)), extractor(*value(n)));
                    bool nr = compare(extractor(*value(n)), extractor(*value(r)));
                    sk_assert(!rn || !nr);
                }

                // verify #7
                if (l) {
                    bool ln = compare(extractor(*value(l)), extractor(*value(n)));
                    bool nl = compare(extractor(*value(n)), extractor(*value(l)));
                    sk_assert(!ln || !nl);
                }

                // verify #6
                if (r) {
                    bool rn = compare(extractor(*value(r)), extractor(*value(n)));
                    sk_assert(!rn);
                }

                // verify #6
                if (l) {
                    bool nl = compare(extractor(*value(n)), extractor(*value(l)));
                    sk_assert(!nl);
                }

                // verify #3
                if (rbtree_algorithm::is_red(n)) {
                    sk_assert(!rbtree_algorithm::is_red(l));
                    sk_assert(!rbtree_algorithm::is_red(r));
                }

                // verify #4
                if (!r && !l)
                    sk_assert(rbtree_algorithm::black_count(root, n) == black_count);
            }

            // verify #5
            sk_assert(loop_count == size());
        }
#endif
    }

    V *value(base_pointer node) {
        assert_retval(node != sentinel_, nullptr);
        node_pointer ptr = node.cast<node_type>();
        return &(ptr->value);
    }

    const V *value(base_pointer node) const {
        return const_cast<rbtree>(this)->value(node);
    }

private:
    Allocator allocator_;
    rbtree_node_base dummy_;
    base_pointer sentinel_;
};

NS_END(detail)
NS_END(sk)

#endif // RBTREE_H
