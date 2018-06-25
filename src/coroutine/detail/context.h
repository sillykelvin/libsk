#ifndef CONTEXT_H
#define CONTEXT_H

#include <utility/types.h>

NS_BEGIN(sk)
NS_BEGIN(detail)

void *make_context(void *sp, size_t stack_size, void(*fn)(intptr_t)) asm("make_context");
intptr_t jump_context(void **ofc, void *nfc, intptr_t p, bool preserve_fpu) asm("jump_context");

NS_END(detail)
NS_END(sk)

#endif // CONTEXT_H
