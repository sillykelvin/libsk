#ifndef RBTREE_H
#define RBTREE_H

namespace sk {
namespace detail {

template<typename K, typename V>
struct tree_node {
    /*
     * because left/right pointers only have 63 bits, so we have
     * to shift a bit here, otherwise if we do this:
     *     n.left != npos
     * GCC will give an error:
     * "comparison is always true due to limited range of data type"
     */
    static const size_t npos  = static_cast<size_t>(-1) >> 1;
    static const size_t black = static_cast<size_t>(0);
    static const size_t red   = static_cast<size_t>(1);

    size_t  left: 63;
    size_t right: 63;
    size_t color: 1;  // color of parent link
    size_t waste: 1;
    K key;
    V value;

    tree_node() : left(npos), right(npos), color(red) {}
};

} // namespace detail

template<typename K, typename V, size_t N>
struct fixed_rbtree {
    typedef detail::tree_node<K, V> node;
    typedef referable_array<node, N> impl_type;
    static const size_t npos  = node::npos;
    static const size_t black = node::black;
    static const size_t red   = node::red;

    impl_type nodes;
    size_t root;

    fixed_rbtree() : root(npos) {}


    /*****************************************************
     * node helper functions
     *****************************************************/

    bool __red(size_t index) {
        // treat null node as black
        if (index == npos) return false;

        node *h = nodes.at(index);
        assert_retval(h, false);

        return h->color == red;
    }

    bool __red(node& n) {
        return n.color == red;
    }

    /*****************************************************
     * size functions
     *****************************************************/

    size_t size() const { return nodes.size(); }

    size_t capacity() const { return nodes.capacity(); }

    bool empty() const { return root == npos; }

    bool full()  const { return nodes.full(); }

    /*****************************************************
     * standard bst search
     *****************************************************/

    V *get(const K& key) {
        return __get(root, key);
    }

    V *__get(size_t index, const K& key) {
        while (index != npos) {
            node *h = nodes.at(index);
            assert_retval(h, NULL);

            if      (key < h->key) index = h->left;
            else if (h->key < key) index = h->right;
            else                   return &h->value;
        }
        return NULL;
    }

    bool contains(const K& key) {
        return get(key) != NULL;
    }

    /*****************************************************
     * red-black insertion
     *****************************************************/

    int put(const K& key, const V& value) {
        if (full())
            return -ENOMEM;

        root = __put(root, key, value);
        assert_noeffect(root != npos);
        node *h = nodes.at(root);
        assert_retval(h, -EFAULT);

        h->color = black;
        return 0;
    }

    size_t __put(size_t index, const K& key, const V& value) {
        node *h = NULL;
        if (index != npos)
            h = nodes.at(index);

        if (!h) {
            h = nodes.emplace(&index);
            assert_retval(index != npos, npos);
            assert_retval(h, npos);

            h->key = key;
            h->value = value;
            // other fields are already set in ctor

            return index;
        }

        if      (key < h->key) h->left  = __put(h->left, key, value);
        else if (h->key < key) h->right = __put(h->right, key, value);
        else                   h->value = value;

        if (__red(h->right) && !__red(h->left)) {
            index = __rotate_left(index);
            h = nodes.at(index);
            assert_retval(h, npos);
        }

        assert_retval(h->left != npos, npos);
        node *l = nodes.at(h->left);
        assert_retval(l, npos);
        if (__red(h->left) && __red(l->left)) {
            index = __rotate_right(index);
            h = nodes.at(index);
            assert_retval(h, npos);
        }

        if (__red(h->left) && __red(h->right))
            __flip_colors(*h);

        return index;
    }

    /*****************************************************
     * red-black deletion
     *****************************************************/

    void erase_min() {
        if (empty()) return;

        node *h = nodes.at(root);
        assert_retnone(h);

        if (!__red(h->left) && !__red(h->right))
            h->color = red;

        root = __erase_min(root);
        if (!empty()) {
            h = nodes.at(root);
            assert_retnone(h);
            h->color = black;
        }
        assert_noeffect(__check());
    }

    size_t __erase_min(size_t index) {
        node *h = nodes.at(index);
        assert_retval(h, npos);

        if (h->left == npos) {
            nodes.erase(index);
            return npos;
        }

        node *l = nodes.at(h->left);
        assert_retval(l, npos);

        if (!__red(h->left) && !__red(l->left)) {
            index = __move_red_left(index);
            h = nodes.at(index);
            assert_retval(h, npos);
        }

        h->left = __erase_min(h->left);
        return __balance(index);
    }

    void erase_max() {
        if (empty()) return;

        node *h = nodes.at(root);
        assert_retnone(h);

        if (!__red(h->left) && !__red(h->right))
            h->color = red;

        root = __erase_max(root);
        if (!empty()) {
            h = nodes.at(root);
            assert_retnone(h);
            h->color = black;
        }
        assert_noeffect(__check());
    }

    size_t __erase_max(size_t index) {
        node *h = nodes.at(index);
        assert_retval(h, npos);

        if (__red(h->left)) {
            index = __rotate_right(index);
            h = nodes.at(index);
            assert_retval(h, npos);
        }

        if (h->right == npos) {
            nodes.erase(index);
            return npos;
        }

        assert_retval(h->right != npos, npos);
        node *r = nodes.at(h->right);
        assert_retval(r, npos);

        if (!__red(h->right) && !__red(r->left)) {
            index = __move_red_right(index);
            h = nodes.at(index);
            assert_retval(h, npos);
        }

        h->right = __erase_max(h->right);

        return __balance(index);
    }

    void erase(const K& key) {
        if (!contains(key))
            return;

        node *h = nodes.at(root);
        assert_retnone(h);

        if (!__red(h->left) && !__red(h->right))
            h->color = red;

        root = __erase(root, key);
        if (!empty()) {
            h = nodes.at(root);
            assert_retnone(h);
            h->color = black;
        }
    }

    size_t __erase(size_t index, const K& key) {
        assert_retval(__get(index, key), npos);
        node *h = nodes.at(index);
        assert_retval(h, npos);

        if (key < h->key) {
            assert_retval(h->left != npos, npos);
            node *l = nodes.at(h->left);
            assert_retval(l, npos);

            if (!__red(h->left) && !__red(l->left)) {
                index = __move_red_left(index);
                h = nodes.at(index);
                assert_retval(h, npos);
            }
            h->left = __erase(h->left, key);
        } else {
            if (__red(h->left)) {
                index = __rotate_right(index);
                h = nodes.at(index);
                assert_retval(h, npos);
            }
            if (!(key < h->key || h->key < key) && h->right == npos) {
                nodes.erase(index);
                return npos;
            }

            assert_retval(h->right != npos, npos);
            node *r = nodes.at(h->right);
            assert_retval(r, npos);

            if (!__red(h->right) && !__red(r->left)) {
                index = __move_red_right(index);
                h = nodes.at(index);
                assert_retval(h, npos);
            }
            if (!(key < h->key || h->key < key)) {
                size_t idx = __min(h->right);
                node *x = nodes.at(idx);
                assert_retval(x, npos);
                h->key = x->key;
                h->value = x->value;
                h->right = __erase_min(h->right);
            } else {
                h->right = __erase(h->right, key);
            }
        }
        return __balance(index);
    }

    /*****************************************************
     * red-black tree helper functions
     *****************************************************/

    size_t __rotate_right(size_t index) {
        assert_retval(index != npos, npos);
        node *h = nodes.at(index);
        assert_retval(h && __red(h->left), npos);

        size_t ret = h->left;
        node *x = nodes.at(h->left);
        assert_retval(x, npos);

        h->left = x->right;
        x->right = index;
        x->color = h->color;
        h->color = red;

        return ret;
    }

    size_t __rotate_left(size_t index) {
        assert_retval(index != npos, npos);
        node *h = nodes.at(index);
        assert_retval(h && __red(h->right), npos);

        size_t ret = h->right;
        node *x = nodes.at(h->right);
        assert_retval(x, npos);

        h->right = x->left;
        x->left = index;
        x->color = h->color;
        h->color = red;

        return ret;
    }

    void __flip_colors(node& h) {
        assert_retnone(h.left != npos && h.right != npos);
        assert_retnone((!__red(h) && __red(h.left) && __red(h.right))
                       || (__red(h) && !__red(h.left) && !__red(h.right)));

        node *l = nodes.at(h.left);
        node *r = nodes.at(h.right);
        assert_retnone(l && r);

        if (__red(h)) {
            h.color = black;
            l->color = red;
            r->color = red;
        } else {
            h.color = red;
            l->color = black;
            r->color = black;
        }
    }

    size_t __move_red_left(size_t index) {
        assert_retval(index != npos, npos);
        node *h = nodes.at(index);
        assert_retval(h, npos);

        assert_retval(h->left != npos, npos);
        node *l = nodes.at(h->left);
        assert_retval(l, npos);

        assert_retval(h->right != npos, npos);
        node *r = nodes.at(h->right);
        assert_retval(r, npos);

        assert_retval(__red(index) && !__red(h->left) && !__red(l->left), npos);

        __flip_colors(*h);

        if (__red(r->left)) {
            h->right = __rotate_right(h->right);
            index = __rotate_left(index);
            h = nodes.at(index);
            assert_retval(h, npos);
            __flip_colors(*h);
        }

        return index;
    }

    size_t __move_red_right(size_t index) {
        assert_retval(index != npos, npos);
        node *h = nodes.at(index);
        assert_retval(h, npos);

        assert_retval(h->left != npos, npos);
        node *l = nodes.at(h->left);
        assert_retval(l, npos);

        assert_retval(h->right != npos, npos);
        node *r = nodes.at(h->right);
        assert_retval(r, npos);

        assert_retval(__red(index) && !__red(h->right) && !__red(r->left), npos);

        __flip_colors(*h);

        if (__red(l->left)) {
            index = __rotate_right(index);
            h = nodes.at(index);
            assert_retval(h, npos);
            __flip_colors(*h);
        }

        return index;
    }

    size_t __balance(size_t index) {
        assert_retval(index != npos, npos);
        node *h = nodes.at(index);
        assert_retval(h, npos);

        if (__red(h->right)) {
            index = __rotate_left(index);
            h = nodes.at(index);
            assert_retval(h, npos);
        }

        assert_retval(h->left != npos, npos);
        node *l = nodes.at(h->left);
        assert_retval(l, npos);

        if (__red(h->left) && __red(l->left)) {
            index = __rotate_right(index);
            h = nodes.at(index);
            assert_retval(h, npos);
        }

        if (__red(h->left) && __red(h->right))
            __flip_colors(*h);

        return index;
    }

    /*****************************************************
     * utility functions
     *****************************************************/

    int height() const { return __height(root); }

    int __height(size_t index) const {
        if (index == npos) return -1;

        node *h = nodes.at(index);
        assert_retval(h, -1);

        int l = __height(h->left);
        int r = __height(h->right);

        return 1 + (l >= r) ? l : r;
    }

    /*****************************************************
     * ordered symbol table functions
     *****************************************************/

    K *min() {
        if (empty()) return NULL;
        size_t idx = __min(root);
        assert_retval(idx != npos, NULL);
        node *h = nodes.at(idx);
        assert_retval(h, NULL);
        return &h->key;
    }

    size_t __min(size_t index) {
        assert_retval(index != npos, npos);
        node *x = nodes.at(index);
        assert_retval(x, npos);

        if (x->left == npos) return index;
        else                 return __min(x->left);
    }

    // the largest key less than or equal to the given key
    K *floor(const K& key) {
        size_t idx = __floor(root, key);
        if (idx == npos)
            return NULL;

        node *h = nodes.at(idx);
        assert_retval(h, NULL);
        return &h->key;
    }

    size_t __floor(size_t index, const K& key) {
        if (index == npos) return npos;

        node *x = nodes.at(index);
        assert_retval(x, npos);

        if (!(key < x->key || x->key < key))
            return index;

        if (key < x->key)
            return __floor(x->left, key);

        size_t idx = __floor(x->right, key);
        if (idx != npos) return idx;
        else             return index;
    }

    // the smallest key greater than or equal to the given key
    K *ceiling(const K& key) {
        size_t idx = __ceiling(root, key);
        if (idx == npos)
            return NULL;

        node *h = nodes.at(idx);
        assert_retval(h, NULL);
        return &h->key;
    }

    size_t __ceiling(size_t index, const K& key) {
        if (index == npos) return npos;

        node *x = nodes.at(index);
        assert_retval(x, npos);

        if (!(key < x->key || x->key < key))
            return index;

        if (x->key < key)
            return __ceiling(x->right, key);

        size_t idx = __ceiling(x->left, key);
        if (idx != npos) return idx;
        else             return index;
    }

    /*****************************************************
     * check integrity of red-black BST data structure
     *****************************************************/

    bool __check() {
        bool valid = true;

        if (!__bst())             { ERR("Not in symmetric order"); valid = false; }
        if (!__is23())            { ERR("Not a 2-3 tree"); valid = false; }
        if (!__balanced())        { ERR("Not balanced"); valid = false; }

        return valid;
    }

    // does this binary tree satisfy symmetric order?
    // Note: this test also ensures that data structure is a binary tree since order is strict
    bool __bst() { return __bst(root, NULL, NULL); }

    bool __bst(size_t index, K *min, K *max) {
        if (index == npos) return true;

        node *x = nodes.at(index);
        assert_retval(x, false);

        if (min && !(*min < x->key)) return false;
        if (max && !(x->key < *max)) return false;

        return __bst(x->left, min, &x->key) && __bst(x->right, &x->key, max);
    }

    bool __is23() { return __is23(root); }

    bool __is23(size_t index) {
        if (index == npos) return true;
        node *x = nodes.at(index);
        assert_retval(x, false);

        if (__red(x->right)) return false;
        if (index != root && __red(index) && __red(x->left)) return false;
        return __is23(x->left) && __is23(x->right);
    }

    bool __balanced() {
        int black = 0;
        size_t index = root;
        while (index != npos) {
            node *x = nodes.at(index);
            assert_retval(x, false);

            if (!__red(index)) ++black;
            index = x->left;
        }

        return __balanced(root, black);
    }

    bool __balanced(size_t index, int black) {
        if (index == npos) return black == 0;

        if (!__red(index)) --black;

        node *x = nodes.at(index);
        assert_retval(x, false);

        return __balanced(x->left, black) && __balanced(x->right, black);
    }
};

} // namespace sk

#endif // RBTREE_H
