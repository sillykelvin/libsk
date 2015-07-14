#ifndef RBTREE_H
#define RBTREE_H

namespace sk {
namespace detail {

template<typename V>
struct rbtree_node {
    static const size_t npos = static_cast<size_t>(-1);
    static const char  black = static_cast<char>(0);
    static const char   red  = static_cast<char>(1);

    char   color;
    size_t left;
    size_t right;
    size_t parent;
    V      value;

    rbtree_node() : color(red), left(npos), right(npos), parent(npos) {}
};

} // namespace detail

/*
 * K: key
 * V: value
 * F: functor to extract key from value
 * N: tree max size
 */
template<typename K, typename V, typename F, size_t N>
struct fixed_rbtree {
    typedef detail::rbtree_node<V> node;
    typedef referable_array<node, N> impl_type;

    static const size_t npos = node::npos;
    static const char black  = node::black;
    static const char   red  = node::red;

    impl_type nodes;
    size_t root;

    fixed_rbtree() : root(npos) { __check(); }

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

    node *__node(size_t index) {
        if (index == npos)
            return NULL;
        return nodes.at(index);
    }

    size_t __index(node *n) {
        if (!n)
            return npos;

        return nodes.index(n);
    }

    node *__root() {
        return __node(root);
    }

    node *__construct(const V& value, size_t& index) {
        node *n = nodes.emplace(&index);
        if (!n)
            return NULL;

        n->value = value;
        return n;
    }

    void __destroy(size_t index) {
        if (index == npos)
            return;

        nodes.erase(index);
    }

    node *__left(node *n) {
        if (n && n->left != npos)
            return nodes.at(n->left);

        return NULL;
    }

    node *__right(node *n) {
        if (n && n->right != npos)
            return nodes.at(n->right);

        return NULL;
    }

    node *__parent(node *n) {
        if (n && n->parent != npos)
            return nodes.at(n->parent);

        return NULL;
    }

    node *__grand_parent(node *n) {
        return __parent(__parent(n));
    }

    node *__uncle(node *n) {
        node *p = __parent(n);
        if (!p)
            return NULL;

        node *gp = __parent(p);
        if (!gp)
            return NULL;

        if (p == __left(gp))
            return __right(gp);
        else
            return __left(gp);
    }

    node *__sibling(node *n) {
        node *p = __parent(n);
        if (!p)
            return NULL;

        if (n == __left(p))
            return __right(p);
        else
            return __left(p);
    }

    bool __red(node *n) {
        return n ? n->color == red : false;
    }

    size_t size() const { return nodes.size(); }

    size_t capacity() const { return nodes.capacity(); }

    bool empty() const { return root == npos; }

    bool full()  const { return nodes.full(); }

    V *find(const K& key) {
        if (empty())
            return NULL;

        F f;
        node *n = __root();
        while (n) {
            if (__eq(key, f(n->value)))
                return &n->value;

            if (__lt(key, f(n->value)))
                n = __left(n);
            else
                n = __right(n);
        }

        return NULL;
    }

    V *min() {
        node *n = __node(__min(root));
        if (!n)
            return NULL;

        return &n->value;
    }

    V *max() {
        node *n = __node(__max(root));
        if (!n)
            return NULL;

        return &n->value;
    }

    int insert(const V& value) {
        if (full())
            return -ENOMEM;

        F f;

        node *x = __root();
        node *p = NULL;
        while (x) {
            p = x;
            if (__eq(f(value), f(x->value))) {
                x->value = value;
                return 0;
            }

            if (__lt(f(value), f(x->value))) {
                // check if the comparison function is sane
                assert_noeffect(__gt(f(x->value), f(value)));
                x = __left(x);
            } else
                x = __right(x);
        }

        int ret = __insert(p, value);
        if (ret == 0) __check();
        return ret;
    }

    void erase(const V& value) {
        if (empty())
            return;

        F f;
        size_t index = root;
        while (index != npos) {
            node *n = __node(index);
            if (__eq(f(value), f(n->value))) {
                __erase(index);
                break;
            }

            if (__lt(f(value), f(n->value)))
                index = n->left;
            else
                index = n->right;
        }

        __check();
    }

    int __insert(node *parent, const V& value) {
        size_t idx = npos;
        node *n = __construct(value, idx);
        assert_retval(n, -EFAULT);

        // if parent is null, the tree must be an empty tree
        if (!parent) {
            assert_noeffect(root == npos && empty());
            n->color = black;
            root = idx;
            return 0;
        }

        n->parent = __index(parent);

        F f;
        if (__lt(f(value), f(parent->value)))
            parent->left = idx;
        else
            parent->right = idx;

        while (__red(__parent(n))) {
            parent = __parent(n);
            node *uncle = __uncle(n);
            node *grand_parent = __grand_parent(n);
            assert_retval(grand_parent, -EFAULT);

            if (parent == __left(grand_parent)) {
                if (__red(uncle)) {
                    parent->color = black;
                    uncle->color = black;
                    grand_parent->color = red;
                    n = grand_parent;
                } else {
                    if (n == __right(parent)) {
                        n = parent;
                        __rotate_left(n);
                    }
                    __parent(n)->color = black;
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
                    if (n == __left(parent)) {
                        n = parent;
                        __rotate_right(n);
                    }
                    __parent(n)->color = black;
                    grand_parent->color = red;
                    __rotate_left(grand_parent);
                }
            }
        }

        __root()->color = black;
        return 0;
    }

    void __erase(size_t index) {
        assert_retnone(index != npos);
        node *z = __node(index);
        assert_retnone(z);

        node *y = z;
        node *x = NULL;
        node *xp = __parent(z); // x's new parent
        bool is_black = !__red(y);

        // has [0, 1] child
        if (z->left == npos || z->right == npos) {
            // x might be null here if z has no children
            size_t x_idx = z->left != npos ? z->left : z->right;
            x = __node(x_idx);
            __replace(z, x_idx);
        } else { // two children
            size_t y_idx = __min(z->right);
            y = __node(y_idx);
            // y = __min(__right(z));
            assert_noeffect(y->left == npos);
            is_black = !__red(y);
            x = __right(y);
            xp = y;
            if (y->parent != index) { // if y is not z' right child
                xp = __parent(y);
                __replace(y, y->right);
                y->right = z->right;
                __right(y)->parent = y_idx;
            }
            y->left = z->left;
            __left(y)->parent = y_idx;
            y->color = z->color;
            __replace(z, y_idx);
        }

        if (is_black) {
            // note: x might be null here, so xp must be initialized before, not in this loop
            while (x != __root() && !__red(x)) {
                // we can still do this even x is null, x is not root, so xp must be non-null
                if (x == __left(xp)) {
                    // the original relationship is xp -> y -> x, then we use x to replace y,
                    // as y is not null and it is black (is_black is true), so y must have a
                    // non-null sibling, otherwise the tree is not balanced (property 5 is
                    // violated), that is, w here must be non-null
                    node *w = __right(xp);
                    // if w is red, then we do some work, make "new w" is black
                    if (__red(w)) {
                        w->color = black;
                        xp->color = red;
                        __rotate_left(xp);
                        w = __right(xp);
                    }
                    if (!__red(__left(w)) && !__red(__right(w))) {
                        w->color = red;
                        x = xp;
                        xp = __parent(xp);
                    } else {
                        // if w's right child is black, then w's left child must be red here,
                        // then we do some work, make "new w"'s left child black, right child red
                        if (!__red(__right(w))) {
                            __left(w)->color = black;
                            w->color = red;
                            __rotate_right(w);
                            w = __right(xp);
                        }
                        w->color = xp->color;
                        xp->color = black;
                        if (w->right != npos)
                            __right(w)->color = black;
                        __rotate_left(xp);
                        x = __root();
                        xp = NULL;
                    }
                } else {
                    node *w = __left(xp);
                    if (__red(w)) {
                        w->color = black;
                        xp->color = red;
                        __rotate_right(xp);
                        w = __left(xp);
                    }
                    if (!__red(__left(w)) && !__red(__right(w))) {
                        w->color = red;
                        x = xp;
                        xp = __parent(xp);
                    } else {
                        if (!__red(__left(w))) {
                            __right(w)->color = black;
                            w->color = red;
                            __rotate_left(w);
                            w = __left(xp);
                        }
                        w->color = xp->color;
                        xp->color = black;
                        if (w->left != npos)
                            __left(w)->color = black;
                        __rotate_right(xp);
                        x = __root();
                        xp = NULL;
                    }
                }
            }
            if (x)
                x->color = black;
        }

        __destroy(index);
    }

    void __rotate_left(node *n) {
        assert_retnone(n);
        node *r = __right(n);
        assert_retnone(r);

        size_t op = __index(n); // old parent
        size_t np = n->right;   // new parent

        n->right = r->left;

        node *rl = __left(r);
        if (rl)
            rl->parent = op;

        node *gp = __parent(n); // grand parent
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

    void __rotate_right(node *n) {
        assert_retnone(n);
        node *l = __left(n);
        assert_retnone(l);

        size_t op = __index(n); // old parent
        size_t np = n->left;    // new parent

        n->left = l->right;

        node *lr = __right(l);
        if (lr)
            lr->parent = op;

        node *gp = __parent(n); // grand parent
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

    void __replace(node *o, size_t n) {
        node *p = __parent(o);
        node *x = __node(n);

        if (!p)
            root = n;
        else {
            if (o == __left(p))
                p->left = n;
            else
                p->right = n;
        }

        if (x)
            x->parent = o->parent;
    }

    size_t __min(size_t index) {
        if (index == npos)
            return npos;

        node *n = __node(index);
        assert_retval(n, npos);

        if (n->left == npos)
            return index;

        return __min(n->left);
    }

    size_t __max(size_t index) {
        if (index == npos)
            return npos;

        node *n = __node(index);
        assert_retval(n, npos);

        if (n->right == npos)
            return index;

        return __max(n->right);
    }

    bool __check() {
        bool valid = true;
#ifdef DEBUG
        if (!__check_prop1(root)) { ERR("rbtree property 1 violence"); valid = false; }
        if (!__check_prop2())     { ERR("rbtree property 2 violence"); valid = false; }
        if (!__check_prop3())     { ERR("rbtree property 3 violence"); valid = false; }
        if (!__check_prop4(root)) { ERR("rbtree property 4 violence"); valid = false; }

        size_t pbc = -1;
        if (!__check_prop5(root, 0, pbc)) { ERR("rbtree property 5 violence"); valid = false; }

        assert_noeffect(valid);
#endif
        return valid;
    }

    // property 1: a node's color is either red or black
    bool __check_prop1(size_t index) {
        if (index == npos) // null node must be black, it's ok
            return true;

        node *n = __node(index);
        assert_retval(n, false);

        if (n->color != red && n->color != black)
            return false;

        return __check_prop1(n->left) && __check_prop1(n->right);
    }

    // property 2: root node must be black
    bool __check_prop2() {
        return !__red(__root());
    }

    // property 3: all leaves (null node) are black
    bool __check_prop3() {
        return !__red(NULL);
    }

    // property 4: every node must have two black children
    bool __check_prop4(size_t index) {
        if (index == npos)
            return true;

        node *n = __node(index);
        assert_retval(n, false);

        if (__red(n)) {
            if (__red(__parent(n)) || __red(__left(n)) || __red(__right(n)))
                return false;
        }

        return __check_prop4(n->left) && __check_prop4(n->right);
    }

    // property 5: every path from a node to all leaves has same count of black nodes
    bool __check_prop5(size_t index, size_t curr_black_count, size_t& path_black_count) {
        // we reached the null node
        if (index == npos) {
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

        node *n = __node(index);
        assert_retval(n, false);

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

#endif // RBTREE_H
