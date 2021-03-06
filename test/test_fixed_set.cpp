#include <gtest/gtest.h>
#include <iostream>
#include <libsk.h>

#define SHM_PATH_PREFIX "/libsk-test"
#define MAX_SIZE 20

using namespace sk;

struct fixed_set_test {
    int i;

    explicit fixed_set_test(int i) : i(i) {}

    bool operator<(const fixed_set_test& that) const { return this->i < that.i; }
};

typedef fixed_set<fixed_set_test, MAX_SIZE> set;

#define INDENT_STEP 4
void print_tree_aux(const set& s, set::base_pointer n, int indent) {
    if (!n) {
        printf("<empty tree>\n");
        return;
    }

    if (n->right)
        print_tree_aux(s, n->right, indent + INDENT_STEP);

    for (int i = 0; i < indent; ++i)
        printf(" ");

    if (!n->red)
        printf("B%d\n", n.cast<set::node_type>()->value.i);
    else
        printf("R%d\n", n.cast<set::node_type>()->value.i);

    if (n->left)
        print_tree_aux(s, n->left, indent + INDENT_STEP);
}

void print_tree(const set& s) {
    // WOW, what a tricky and dirty way!!
    set::base_pointer root = s.end().ptr->parent;
    print_tree_aux(s, root, 0);
    printf("=========================================\n");
}

TEST(fixed_set, normal) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<set> xs = shm_new<set>();
    ASSERT_TRUE(!!xs);
    set& s = *xs;

    ASSERT_TRUE(s.empty());
    ASSERT_TRUE(!s.full());
    ASSERT_TRUE(s.size() == 0);
    ASSERT_TRUE(s.capacity() == MAX_SIZE);
    ASSERT_TRUE(s.find(fixed_set_test(1)) == s.end());

    ASSERT_TRUE(s.insert(fixed_set_test(1)) == 0);
    ASSERT_TRUE(s.insert(fixed_set_test(2)) == 0);
    ASSERT_TRUE(s.insert(fixed_set_test(3)) == 0);
    ASSERT_TRUE(s.insert(fixed_set_test(4)) == 0);
    ASSERT_TRUE(s.insert(fixed_set_test(5)) == 0);

    ASSERT_TRUE(!s.empty() && !s.full());
    ASSERT_TRUE(s.size() == 5);

    ASSERT_TRUE(s.find(fixed_set_test(3)) != s.end());
    ASSERT_TRUE(s.find(fixed_set_test(3))->i == 3);

    ASSERT_TRUE(s.emplace(2));
    ASSERT_TRUE(s.size() == 5);
    ASSERT_TRUE(s.emplace(6));
    ASSERT_TRUE(s.size() == 6);

    set::iterator it = s.begin();
    // it->i = 777; // compile error, intended
    ++it; ++it;
    s.erase(it);
    ASSERT_TRUE(s.find(fixed_set_test(3)) == s.end());
    s.erase(fixed_set_test(2));
    ASSERT_TRUE(s.find(fixed_set_test(2)) == s.end());

    ASSERT_TRUE(s.insert(fixed_set_test(2)) == 0);
    ASSERT_TRUE(s.insert(fixed_set_test(3)) == 0);

    for (struct {int i; set::iterator it;} i = {1, s.begin()}; i.it != s.end(); ++i.it, ++i.i)
        ASSERT_TRUE(i.it->i == i.i);

    shm_delete(xs);
    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}

TEST(fixed_set, loop_erase) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<set> xs = shm_new<set>();
    ASSERT_TRUE(!!xs);
    set& s = *xs;

    for (int i = 0; i < MAX_SIZE; ++i) {
        ASSERT_TRUE(s.insert(fixed_set_test(i)) == 0);
    }

    std::vector<int> vec;
    vec.push_back(1);
    vec.push_back(3);
    vec.push_back(9);
    vec.push_back(15);
    vec.push_back(19);
    for (set::iterator it = s.begin(), end = s.end(); it != end;) {
        if (std::find(vec.begin(), vec.end(), it->i) != vec.end()) {
            s.erase(it++);
        } else {
            ++it;
        }
    }

    for (int i = 0; i < MAX_SIZE; ++i) {
        set::iterator it = s.find(fixed_set_test(i));
        if (std::find(vec.begin(), vec.end(), i) != vec.end()) {
            ASSERT_TRUE(it == s.end());
        } else {
            ASSERT_TRUE(it != s.end());
        }
    }

    shm_delete(xs);
    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}

TEST(fixed_set, visualization) {
    int ret = shm_init(SHM_PATH_PREFIX, false);
    ASSERT_TRUE(ret == 0);

    shm_ptr<set> xs = shm_new<set>();
    ASSERT_TRUE(!!xs);
    set& s = *xs;

    s.emplace(4);
    s.emplace(1);
    s.emplace(2);
    s.emplace(3);
    ASSERT_TRUE(s.size() == 4);

    print_tree(s);

    s.emplace(0);
    print_tree(s);

    s.emplace(6);
    print_tree(s);

    s.emplace(7);
    print_tree(s);

    s.emplace(5);
    print_tree(s);
    ASSERT_TRUE(s.size() == 8);

    s.emplace(8);
    print_tree(s);

    s.emplace(9);
    print_tree(s);
    ASSERT_TRUE(s.size() == 10);

    s.erase(fixed_set_test(7));
    print_tree(s);

    s.erase(fixed_set_test(6));
    print_tree(s);

    s.erase(fixed_set_test(0));
    print_tree(s);

    s.erase(fixed_set_test(5));
    print_tree(s);

    s.erase(fixed_set_test(3));
    print_tree(s);

    s.erase(fixed_set_test(2));
    print_tree(s);

    s.erase(fixed_set_test(1));
    print_tree(s);

    s.erase(fixed_set_test(4));
    print_tree(s);

    s.erase(fixed_set_test(8));
    print_tree(s);

    s.erase(fixed_set_test(9));
    print_tree(s);

    ASSERT_TRUE(s.empty());
    ASSERT_TRUE(s.size() == 0);

    shm_delete(xs);
    ret = shm_fini();
    ASSERT_TRUE(ret == 0);
}
