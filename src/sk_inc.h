#ifndef SK_INC_H
#define SK_INC_H

// C headers
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/time.h>

// C++ headers
#include <algorithm>
#include <new>

// common headers
#include "assert_helper.h"
#include "types.h"
#include "log.h"

// concrete headers
#include "utility.h"
#include "fixed_array.h"
#include "referable_array.h"
#include "shm_seg.h"
#include "shm_mgr.h"
#include "shm_ptr.h"
#include "fixed_stack.h"
#include "rbtree.h"
#include "fixed_set.h"
#include "fixed_map.h"

#endif // SK_INC_H
