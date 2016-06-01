#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

#define SHM_MGR_KEY         (0x77777)
#define SHM_SIZE            (102400)

using namespace std;
using namespace sk;

struct rbtree_test {
    int i;

    explicit rbtree_test(int i) : i(i) { /*cout << "ctor called, i: " << i << endl;*/ }
    ~rbtree_test() { /*cout << "dtor called, i: " << i << endl;*/ }

    bool operator<(const rbtree_test& that) const {
        return this->i < that.i;
    }
};

struct rbtree_key_extractor {
    const rbtree_test& operator()(const rbtree_test& value) const {
        return value;
    }
};

typedef shm_rbtree<rbtree_test, rbtree_test, rbtree_key_extractor> tree;

#define INDENT_STEP 4
void print_tree_aux(tree *t, tree::pointer n, int indent) {
    if (!n) {
        printf("<empty tree>");
        return;
    }

    if (n->right)
        print_tree_aux(t, n->right, indent + INDENT_STEP);

    for (int i = 0; i < indent; ++i)
        printf(" ");

    if (n->color == tree::black)
        printf("B%d\n", n->value.i);
    else
        printf("R%d\n", n->value.i);

    if (n->left)
        print_tree_aux(t, n->left, indent + INDENT_STEP);
}

void print_tree(tree *t) {
    print_tree_aux(t, t->root, 0);
    printf("=========================================\n");
}


TEST(shm_rbtree, normal) {
    int ret = shm_mgr_init(SHM_MGR_KEY, SHM_SIZE, false);
    ASSERT_TRUE(ret == 0);

    // must create a scope here to make sure tree is destructed
    // before shm_mgr_fini() called
    {
        tree t;

        // tree tt(t); // this should not compile
        // tree ttt;   // this should not compile
        // ttt = t;    // this should not compile

        ASSERT_TRUE(t.empty());
        ASSERT_TRUE(t.size() == 0);
        ASSERT_TRUE(t.__check());
        ASSERT_TRUE(t.find(rbtree_test(1)) == t.end());
        ASSERT_TRUE(t.begin() == t.end());
        ASSERT_TRUE(t.cbegin() == t.cend());

        for (int i = 1; i <= 20; ++i) {
            int ret = t.insert(rbtree_test(i));
            ASSERT_TRUE(ret == 0);
            ASSERT_TRUE(t.find(rbtree_test(i)) != t.end());
            ASSERT_TRUE(t.find(rbtree_test(i))->i == i);
            ASSERT_TRUE(t.size() == static_cast<size_t>(i));
            ASSERT_TRUE(t.__check());
        }

        print_tree(&t);

        tree::iterator it = t.begin();
        ASSERT_TRUE(it != t.end());
        ASSERT_TRUE(it->i == 1);
        ++it;
        ASSERT_TRUE(it->i == 2);
        tree::iterator it2 = it++;
        ASSERT_TRUE(it2->i == 2);
        ASSERT_TRUE(it->i == 3);
        --it;
        ASSERT_TRUE(it->i == 2);
        it2 = it--;
        ASSERT_TRUE(it2->i == 2);
        ASSERT_TRUE(it->i == 1);
        tree::iterator it3 = t.begin();
        ASSERT_TRUE(it == it3);
        ++it3;
        ASSERT_TRUE(it != it3);
        ++it;
        ASSERT_TRUE(it == it3);
        for (struct {int i; tree::iterator it;} s = {1, t.begin()}; s.it != t.end(); ++s.it, ++s.i)
            ASSERT_TRUE(s.it->i == s.i);

        it = t.begin();
        ASSERT_TRUE(it->i == 1);
        it->i = 999; // we should NEVER do this!!
        it->i = 1;

        tree::const_iterator cit = t.cbegin();
        tree::const_iterator ced = t.cend();
        ASSERT_TRUE(cit != ced);
        ASSERT_TRUE(cit == it);
        // cit->i = 999;   // this should not compile
        // (*cit).i = 999; // this should not compile
        tree::const_iterator cit2 = cit++;
        ASSERT_TRUE(cit2 != cit);
        ASSERT_TRUE(cit2 == it);
        tree::const_iterator cit3 = --cit;
        ASSERT_TRUE(cit3 == cit && cit2 == cit && it == cit);
        ASSERT_TRUE(cit->i == 1);
        const tree& ct = t;
        for (struct {int i; tree::const_iterator cit;} s = {1, ct.begin()}; s.cit != ct.end(); ++s.cit, ++s.i)
            ASSERT_TRUE(s.cit->i == s.i);

        ASSERT_TRUE(t.find(rbtree_test(7)) != t.end());
        t.erase(rbtree_test(7));
        ASSERT_TRUE(t.find(rbtree_test(7)) == t.end());
        ASSERT_TRUE(t.__check());

        printf("erase 7:\n");
        print_tree(&t);

        int values[] = { 2, 6, 8, 3, 14, 10, 19, 5, 17, 9, 15 };
        for (size_t i = 0; i < sizeof(values) / sizeof(int); ++i) {
            ASSERT_TRUE(t.find(rbtree_test(values[i])) != t.end());
            t.erase(rbtree_test(values[i]));
            printf("erase %d:\n", values[i]);
            print_tree(&t);
            ASSERT_TRUE(t.find(rbtree_test(values[i])) == t.end());
            ASSERT_TRUE(t.__check());
        }

        for (size_t i = 0; i < sizeof(values) / sizeof(int); ++i) {
            ASSERT_TRUE(t.find(rbtree_test(values[i])) == t.end());
            int ret = t.insert(rbtree_test(values[i]));
            printf("insert %d:\n", values[i]);
            print_tree(&t);
            ASSERT_TRUE(ret == 0);
            ASSERT_TRUE(t.find(rbtree_test(values[i])) != t.end());
            ASSERT_TRUE(t.__check());
        }

        tree::pointer min = t.min();
        ASSERT_TRUE(min && min->value.i == 1);
        tree::pointer max = t.max();
        ASSERT_TRUE(max && max->value.i == 20);

        ASSERT_TRUE(t.__check());
    }

    shm_mgr_fini();
}
