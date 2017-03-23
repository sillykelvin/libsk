#include <time.h>
#include "math_helper.h"
#include "utility/assert_helper.h"

static u32 seed = 0;

NS_BEGIN(sk)

static int random() {
    if (unlikely(seed == 0)) {
        seed = static_cast<u32>(time(nullptr));
        srand(seed);
    }

    return rand();
}

int random(int min, int max) {
    if (min == max) return min;
    if (min > max) std::swap(min, max);

    return random() % (max - min + 1) + min;
}

bool probability_hit(int prob, int prob_max) {
    assert_retval(prob_max > 0, false);
    assert_retval(prob >= 0 && prob <= prob_max, false);

    int p = random(1, prob_max);
    return p <= prob;
}

bool percentage_hit(int percentage) {
    assert_retval(percentage >= 0 && percentage <= 100, false);

    return probability_hit(percentage, 100);
}

NS_END(sk)
