#include <gtest/gtest.h>
#include <iostream>
#include "sk_inc.h"

#define MAX_SIZE 20

using namespace std;
using namespace sk;

struct rbtree_test {
    int i;

    rbtree_test() {}
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

typedef fixed_rbtree<rbtree_test, rbtree_test, rbtree_key_extractor, MAX_SIZE> tree;

#define INDENT_STEP 4
void print_tree_aux(tree *t, tree::node *n, int indent) {
    if (!n) {
        printf("<empty tree>");
        return;
    }

    if (n->right != tree::npos)
        print_tree_aux(t, t->__right(n), indent + INDENT_STEP);

    for (int i = 0; i < indent; ++i)
        printf(" ");

    if (n->color == tree::black)
        printf("B%d\n", n->value.i);
    else
        printf("R%d\n", n->value.i);

    if (n->left != tree::npos)
        print_tree_aux(t, t->__left(n), indent + INDENT_STEP);
}

void print_tree(tree *t) {
    print_tree_aux(t, t->__node(t->root), 0);
    printf("=========================================\n");
}


TEST(fixed_rbtree, normal) {
    tree ta;

    ASSERT_TRUE(ta.empty());
    ASSERT_TRUE(ta.size() == 0);
    ASSERT_TRUE(ta.capacity() == MAX_SIZE);
    ASSERT_TRUE(ta.__check());
    ASSERT_TRUE(!ta.find(rbtree_test(1)));
    ASSERT_TRUE(ta.begin() == ta.end());
    ASSERT_TRUE(ta.cbegin() == ta.cend());

    for (size_t i = 1; i <= ta.capacity(); ++i) {
        int ret = ta.insert(rbtree_test(i));
        ASSERT_TRUE(ret == 0);
        ASSERT_TRUE(ta.find(rbtree_test(i)));
        ASSERT_TRUE(ta.find(rbtree_test(i))->i == static_cast<int>(i));
        ASSERT_TRUE(ta.size() == i);
        ASSERT_TRUE(ta.__check());
    }

    print_tree(&ta);

    tree::iterator it = ta.begin();
    ASSERT_TRUE(it != ta.end());
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
    tree::iterator it3 = ta.begin();
    ASSERT_TRUE(it == it3);
    ++it3;
    ASSERT_TRUE(it != it3);
    ++it;
    ASSERT_TRUE(it == it3);
    for (struct {int i; tree::iterator it;} s = {1, ta.begin()}; s.it != ta.end(); ++s.it, ++s.i)
        ASSERT_TRUE(s.it->i == s.i);

    it = ta.begin();
    ASSERT_TRUE(it->i == 1);
    it->i = 999; // we should NEVER do this!!
    it->i = 1;

    tree::const_iterator cit = ta.cbegin();
    tree::const_iterator ced = ta.cend();
    ASSERT_TRUE(cit != ced);
    ASSERT_TRUE(cit == it);
    // cit->i = 999;   // this should cannot compile
    // (*cit).i = 999; // this should cannot compile
    tree::const_iterator cit2 = cit++;
    ASSERT_TRUE(cit2 != cit);
    ASSERT_TRUE(cit2 == it);
    tree::const_iterator cit3 = --cit;
    ASSERT_TRUE(cit3 == cit && cit2 == cit && it == cit);
    ASSERT_TRUE(cit->i == 1);
    const tree& cta = ta;
    for (struct {int i; tree::const_iterator cit;} s = {1, cta.begin()}; s.cit != cta.end(); ++s.cit, ++s.i)
        ASSERT_TRUE(s.cit->i == s.i);

    ASSERT_TRUE(ta.insert(rbtree_test(999)) != 0);
    ASSERT_TRUE(ta.full());
    ASSERT_TRUE(ta.find(rbtree_test(7)));
    ta.erase(rbtree_test(7));
    ASSERT_TRUE(!ta.find(rbtree_test(7)));
    ASSERT_TRUE(!ta.full());
    ASSERT_TRUE(ta.__check());

    printf("erase 7:\n");
    print_tree(&ta);

    int values[] = { 2, 6, 8, 3, 14, 10, 19, 5, 17, 9, 15 };
    for (size_t i = 0; i < sizeof(values) / sizeof(int); ++i) {
        ASSERT_TRUE(ta.find(rbtree_test(values[i])));
        ta.erase(rbtree_test(values[i]));
        printf("erase %d:\n", values[i]);
        print_tree(&ta);
        ASSERT_TRUE(!ta.find(rbtree_test(values[i])));
        ASSERT_TRUE(!ta.full());
        ASSERT_TRUE(ta.__check());
    }

    for (size_t i = 0; i < sizeof(values) / sizeof(int); ++i) {
        ASSERT_TRUE(!ta.find(rbtree_test(values[i])));
        int ret = ta.insert(rbtree_test(values[i]));
        printf("insert %d:\n", values[i]);
        print_tree(&ta);
        ASSERT_TRUE(ret == 0);
        ASSERT_TRUE(ta.find(rbtree_test(values[i])));
        ASSERT_TRUE(!ta.full());
        ASSERT_TRUE(ta.__check());
    }

    rbtree_test *min = ta.min();
    ASSERT_TRUE(min && min->i == 1);
    rbtree_test *max = ta.max();
    ASSERT_TRUE(max && max->i == MAX_SIZE);

    ASSERT_TRUE(ta.__check());
}
