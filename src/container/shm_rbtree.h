#ifndef SHM_RBTREE_H
#define SHM_RBTREE_H

#include <iterator>
#include "shm/shm_mgr.h"
#include "shm/shm_ptr.h"
#include "utility/log.h"
#include "utility/types.h"
#include "utility/utility.h"
#include "utility/assert_helper.h"

namespace sk {
namespace detail {

template<typename V>
struct shm_rbtree_node {
    typedef shm_rbtree_node<V> self;

    static const char black = static_cast<char>(0);
    static const char   red = static_cast<char>(1);

    char          color;
    shm_ptr<self> left;
    shm_ptr<self> right;
    shm_ptr<self> parent;
    V             value;

    shm_rbtree_node(const V& v) : color(red), value(v) {}
};

/*
 * Note:
 *   1. T should be a shm_rbtree type here
 *   2. C stands for constness, true makes this iterator a const iterator
 */
template<typename T, bool C>
struct shm_rbtree_iterator {
    typedef T                                                                     tree_type;
    typedef typename T::node                                                      node_type;
    typedef typename T::value_type                                                value_type;
    typedef ptrdiff_t                                                             difference_type;
    typedef shm_rbtree_iterator<T, C>                                             self;
    typedef std::bidirectional_iterator_tag                                       iterator_category;
    typedef typename if_<C, const tree_type*, tree_type*>::type                   tree_pointer;
    typedef typename if_<C, typename T::const_pointer, typename T::pointer>::type node_pointer;
    typedef typename if_<C, const value_type*, value_type*>::type                 pointer;
    typedef typename if_<C, const value_type&, value_type&>::type                 reference;

    tree_pointer t;
    node_pointer n;

    explicit shm_rbtree_iterator(tree_pointer t) : t(t), n(SHM_NULL) {}
    shm_rbtree_iterator(tree_pointer t, node_pointer n) : t(t), n(n) {}

    /*
     * make a templated copy constructor here to support construction of
     * const iterator from mutable iterator
     */
    template<bool B>
    shm_rbtree_iterator(const shm_rbtree_iterator<T, B>& s) : t(s.t), n(s.n) {}

    self& operator=(const self& s) {
        if (this == &s)
            return *this;

        t = s.t;
        n = s.n;

        return *this;
    }

    reference operator*() const { return n->value; }
    pointer operator->() const { return &n->value; }

    self& operator++()   { n = t->__next(n); return *this; }
    self operator++(int) { self tmp(*this); n = t->__next(n); return tmp; }
    self& operator--()   { n = t->__prev(n); return *this; }
    self operator--(int) { self tmp(*this); n = t->__prev(n); return tmp; }

    bool operator==(const self& x) const { return n == x.n && t == x.t; }
    bool operator!=(const self& x) const { return !(*this == x); }

    bool operator==(const shm_rbtree_iterator<T, !C>& x) const { return n == x.n && t == x.t; }
    bool operator!=(const shm_rbtree_iterator<T, !C>& x) const { return !(*this == x); }
};

} // namespace detail

/*
 * K: key
 * V: value
 * F: functor to extract key from value
 */
template<typename K, typename V, typename F>
struct shm_rbtree {
    typedef K                                        key_type;
    typedef V                                        value_type;
    typedef detail::shm_rbtree_node<V>               node;
    typedef shm_rbtree<K, V, F>                      self;
    typedef shm_ptr<node>                            pointer;
    typedef shm_ptr<const node>                      const_pointer;
    typedef detail::shm_rbtree_iterator<self, false> iterator;
    typedef detail::shm_rbtree_iterator<self, true>  const_iterator;

    static const char black = node::black;
    static const char   red = node::red;

    size_t  node_count;
    pointer root;

    shm_rbtree() : node_count(0), root(SHM_NULL) { __check(); }
    ~shm_rbtree() { clear(); }

    shm_rbtree(const shm_rbtree& tree) = delete;
    shm_rbtree& operator=(const shm_rbtree& tree) = delete;

    /**
     * @brief __lt stands for "less than"
     * @param lhs
     * @param rhs
     * @return true if lhs is less than rhs
     */
    static bool __lt(const K& lhs, const K& rhs) {
        return lhs < rhs;
    }

    /**
     * @brief __gt stands for "greater than"
     * @param lhs
     * @param rhs
     * @return true if lhs is greater than rhs
     */
    static bool __gt(const K& lhs, const K& rhs) {
        return rhs < lhs;
    }

    /**
     * @brief __eq stands for "equal"
     * @param lhs
     * @param rhs
     * @return true if lhs equals to rhs
     */
    static bool __eq(const K& lhs, const K& rhs) {
        return !__lt(lhs, rhs) && !__lt(rhs, lhs);
    }

    pointer __construct(const V& value) {
        return shm_new<node>(value);
    }

    void __destroy(pointer n) {
        shm_del(n);
    }

    pointer __grand_parent(pointer n) {
        if (n && n->parent)
            return n->parent->parent;

        return SHM_NULL;
    }

    pointer __uncle(pointer n) {
        pointer gp = __grand_parent(n);
        if (!gp)
            return SHM_NULL;

        if (n->parent == gp->left)
            return gp->right;
        else
            return gp->left;
    }

    bool __red(pointer n) {
        return n ? n->color == red : false;
    }

    size_t size() const { return node_count; }

    bool empty() const { return node_count <= 0; }

    size_t __clear(pointer n) {
        if (!n) return 0;
        size_t l = __clear(n->left);
        size_t r = __clear(n->right);
        __destroy(n);

        return l + r + 1;
    }

    void clear() {
        size_t count = __clear(root);
        assert_noeffect(count == node_count);

        node_count = 0;
        root = SHM_NULL;
    }

    iterator find(const K& key) {
        if (empty())
            return end();

        F f;
        pointer n = root;
        while (n) {
            if (__eq(key, f(n->value)))
                return iterator(this, n);

            if (__lt(key, f(n->value)))
                n = n->left;
            else
                n = n->right;
        }

        return end();
    }

    const_iterator find(const K& key) const {
        if (empty())
            return end();

        F f;
        const_pointer n = root;
        while (n) {
            if (__eq(key, f(n->value)))
                return const_iterator(this, n);

            if (__lt(key, f(n->value)))
                n = n->left;
            else
                n = n->right;
        }

        return end();
    }

    pointer min() {
        return __min(root);
    }

    const_pointer min() const {
        return __min(root);
    }

    pointer max() {
        return __max(root);
    }

    const_pointer max() const {
        return __max(root);
    }

    iterator begin() {
        return iterator(this, min());
    }

    const_iterator begin() const {
        return const_iterator(this, min());
    }

    const_iterator cbegin() const {
        return const_iterator(this, min());
    }

    iterator end() {
        return iterator(this);
    }

    const_iterator end() const {
        return const_iterator(this);
    }

    const_iterator cend() const {
        return const_iterator(this);
    }

    // TODO: consider to use emplace(...) style here
    int insert(const V& value) {
        F f;
        pointer x = root;
        pointer p = SHM_NULL;
        while (x) {
            p = x;
            if (__eq(f(value), f(x->value)))
                return 0;

            if (__lt(f(value), f(x->value))) {
                // check if the comparison function is sane
                assert_noeffect(__gt(f(x->value), f(value)));
                x = x->left;
            } else {
                // check if the comparison function is sane
                assert_noeffect(!__gt(f(x->value), f(value)));
                x = x->right;
            }
        }

        int ret = __insert(p, value);
        if (ret == 0) __check();
        return ret;
    }

    void erase(const K& key) {
        if (empty())
            return;

        F f;
        pointer n = root;
        while (n) {
            if (__eq(key, f(n->value))) {
                __erase(n);
                break;
            }

            if (__lt(key, f(n->value)))
                n = n->left;
            else
                n = n->right;
        }

        __check();
    }

    int __insert(pointer parent, const V& value) {
        pointer n = __construct(value);
        assert_retval(n, -EFAULT);
        node_count += 1;

        // if parent is null, the tree must be an empty tree
        if (!parent) {
            assert_noeffect(!root);
            n->color = black;
            root = n;
            return 0;
        }

        n->parent = parent;

        F f;
        if (__lt(f(value), f(parent->value)))
            parent->left = n;
        else
            parent->right = n;

        while (__red(n->parent)) {
            parent = n->parent;
            pointer uncle = __uncle(n);
            pointer grand_parent = __grand_parent(n);
            assert_retval(grand_parent, -EFAULT);

            if (parent == grand_parent->left) {
                if (__red(uncle)) {
                    parent->color = black;
                    uncle->color = black;
                    grand_parent->color = red;
                    n = grand_parent;
                } else {
                    if (n == parent->right) {
                        n = parent;
                        __rotate_left(n);
                    }
                    n->parent->color = black;
                    grand_parent->color = red;
                    __rotate_right(grand_parent);
                }
            } else {
                if (__red(uncle)) {
                    parent->color = black;
                    uncle->color = black;
                    grand_parent->color = red;
                    n = grand_parent;
                } else {
                    if (n == parent->left) {
                        n = parent;
                        __rotate_right(n);
                    }
                    n->parent->color = black;
                    grand_parent->color = red;
                    __rotate_left(grand_parent);
                }
            }
        }

        root->color = black;
        return 0;
    }

    void __erase(pointer z) {
        assert_retnone(z);

        pointer y = z;
        pointer x = SHM_NULL;
        pointer xp = z->parent; // x's new parent
        bool is_black = !__red(y);

        // has [0, 1] child
        if (!z->left || !z->right) {
            // x might be null here if z has no children
            if (z->left)
                x = z->left;
            else
                x = z->right;
            __replace(z, x);
        } else { // two children
            y = __min(z->right);
            assert_noeffect(!y->left);
            is_black = !__red(y);
            x = y->right;
            xp = y;
            if (y->parent != z) { // if y is not z' right child
                xp = y->parent;
                __replace(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
            __replace(z, y);
        }

        if (is_black) {
            // note: x might be null here, so xp must be initialized before, not in this loop
            while (x != root && !__red(x)) {
                // we can still do this even x is null, x is not root, so xp must be non-null
                if (x == xp->left) {
                    // the original relationship is xp -> y -> x, then we use x to replace y,
                    // as y is not null and it is black (is_black is true), so y must have a
                    // non-null sibling, otherwise the tree is not balanced (property 5 is
                    // violated), that is, w here must be non-null
                    pointer w = xp->right;
                    // if w is red, then we do some work, make "new w" is black
                    if (__red(w)) {
                        w->color = black;
                        xp->color = red;
                        __rotate_left(xp);
                        w = xp->right;
                    }
                    if (!__red(w->left) && !__red(w->right)) {
                        w->color = red;
                        x = xp;
                        xp = xp->parent;
                    } else {
                        // if w's right child is black, then w's left child must be red here,
                        // then we do some work, make "new w"'s left child black, right child red
                        if (!__red(w->right)) {
                            w->left->color = black;
                            w->color = red;
                            __rotate_right(w);
                            w = xp->right;
                        }
                        w->color = xp->color;
                        xp->color = black;
                        if (w->right)
                            w->right->color = black;
                        __rotate_left(xp);
                        x = root;
                        xp = SHM_NULL;
                    }
                } else {
                    pointer w = xp->left;
                    if (__red(w)) {
                        w->color = black;
                        xp->color = red;
                        __rotate_right(xp);
                        w = xp->left;
                    }
                    if (!__red(w->left) && !__red(w->right)) {
                        w->color = red;
                        x = xp;
                        xp = xp->parent;
                    } else {
                        if (!__red(w->left)) {
                            w->right->color = black;
                            w->color = red;
                            __rotate_left(w);
                            w = xp->left;
                        }
                        w->color = xp->color;
                        xp->color = black;
                        if (w->left)
                            w->left->color = black;
                        __rotate_right(xp);
                        x = root;
                        xp = SHM_NULL;
                    }
                }
            }
            if (x)
                x->color = black;
        }

        __destroy(z);
        node_count -= 1;
    }

    void __rotate_left(pointer n) {
        pointer r = n->right;

        pointer op = n;        // old parent
        pointer np = n->right; // new parent

        n->right = r->left;

        pointer rl = r->left;
        if (rl)
            rl->parent = op;

        pointer gp = n->parent; // grand parent
        r->parent = n->parent;
        if (!gp)
            root = np;
        else {
            if (gp->left == op)
                gp->left = np;
            else
                gp->right = np;
        }

        r->left = op;
        n->parent = np;
    }

    void __rotate_right(pointer n) {
        pointer l = n->left;

        pointer op = n;       // old parent
        pointer np = n->left; // new parent

        n->left = l->right;

        pointer lr = l->right;
        if (lr)
            lr->parent = op;

        pointer gp = n->parent; // grand parent
        l->parent = n->parent;
        if (!gp)
            root = np;
        else {
            if (gp->left == op)
                gp->left = np;
            else
                gp->right = np;
        }

        l->right = op;
        n->parent = np;
    }

    void __replace(pointer o, pointer n) {
        pointer p = o->parent;
        pointer x = n;

        if (!p)
            root = n;
        else {
            if (o == p->left)
                p->left = n;
            else
                p->right = n;
        }

        if (x)
            x->parent = o->parent;
    }

    pointer __min(pointer n) {
        if (!n) return SHM_NULL;

        if (!n->left)
            return n;

        return __min(n->left);
    }

    const_pointer __min(const_pointer n) const {
        if (!n) return SHM_NULL;

        if (!n->left)
            return n;

        return __min(n->left);
    }

    pointer __max(pointer n) {
        if (!n) return SHM_NULL;

        if (!n->right)
            return n;

        return __max(n->right);
    }

    const_pointer __max(const_pointer n) const {
        if (!n) return SHM_NULL;

        if (!n->right)
            return n;

        return __max(n->right);
    }

    pointer __next(pointer n) {
        if (!n)
            return SHM_NULL;

        if (n->right)
            return __min(n->right);

        while (1) {
            pointer p = n->parent;

            // 1. n is root and has no right child, so it is the last one
            if (!p)
                return SHM_NULL;

            // 2. n has no right child, and it is the left child of its parent
            if (n == p->left)
                return p;

            // 3. n has no right child, and it is the right child of its parent
            n = p;
        }
    }

    const_pointer __next(const_pointer n) const {
        if (!n)
            return SHM_NULL;

        if (n->right)
            return __min(n->right);

        while (1) {
            pointer p = n->parent;

            // 1. n is root and has no right child, so it is the last one
            if (!p)
                return SHM_NULL;

            // 2. n has no right child, and it is the left child of its parent
            if (n == p->left)
                return p;

            // 3. n has no right child, and it is the right child of its parent
            n = p;
        }
    }

    pointer __prev(pointer n) {
        if (!n)
            return SHM_NULL;

        if (n->left)
            return __max(n->left);

        while (1) {
            pointer p = n->parent;

            if (!p)
                return SHM_NULL;

            if (n == p->right)
                return p;

            n = p;
        }
    }

    const_pointer __prev(const_pointer n) const {
        if (!n)
            return SHM_NULL;

        if (n->left)
            return __max(n->left);

        while (1) {
            pointer p = n->parent;

            if (!p)
                return SHM_NULL;

            if (n == p->right)
                return p;

            n = p;
        }
    }

    bool __check() {
        bool valid = true;
#ifndef NDEBUG
        if (!__check_prop1(root)) { sk_error("rbtree property 1 violence"); valid = false; }
        if (!__check_prop2())     { sk_error("rbtree property 2 violence"); valid = false; }
        if (!__check_prop3())     { sk_error("rbtree property 3 violence"); valid = false; }
        if (!__check_prop4(root)) { sk_error("rbtree property 4 violence"); valid = false; }

        size_t pbc = -1;
        if (!__check_prop5(root, 0, pbc)) { sk_error("rbtree property 5 violence"); valid = false; }

        assert_noeffect(valid);
#endif
        return valid;
    }

    // property 1: a node's color is either red or black
    bool __check_prop1(pointer n) {
        if (!n) // null node must be black, it's ok
            return true;

        if (n->color != red && n->color != black)
            return false;

        return __check_prop1(n->left) && __check_prop1(n->right);
    }

    // property 2: root node must be black
    bool __check_prop2() {
        return !__red(root);
    }

    // property 3: all leaves (null node) are black
    bool __check_prop3() {
        return !__red(SHM_NULL);
    }

    // property 4: every red node must have two black children
    bool __check_prop4(pointer n) {
        if (!n)
            return true;

        if (__red(n)) {
            if (__red(n->parent) || __red(n->left) || __red(n->right))
                return false;
        }

        return __check_prop4(n->left) && __check_prop4(n->right);
    }

    // property 5: every path from a node to all leaves has same count of black nodes
    bool __check_prop5(pointer n, size_t curr_black_count, size_t& path_black_count) {
        // we reached the null node
        if (!n) {
            // 1. null node is black
            curr_black_count += 1;
            // 2. black count in every path is not calculated, so we consider black count in
            //    current path is the black count in every path
            if (path_black_count == static_cast<size_t>(-1)) {
                path_black_count = curr_black_count;
                return true;
            }
            // 3. black count is calculated before, so we check if the two numbers are equal
            return curr_black_count == path_black_count;
        }

        if (!__red(n))
            curr_black_count += 1;

        bool vl = __check_prop5(n->left, curr_black_count, path_black_count);
        bool vr = __check_prop5(n->right, curr_black_count, path_black_count);
        return vl && vr;
        // return __check_prop5(n->left, curr_black_count, path_black_count)
        //         && __check_prop5(n->right, curr_black_count, path_black_count);
    }
};

} // namespace sk

#endif // SHM_RBTREE_H
