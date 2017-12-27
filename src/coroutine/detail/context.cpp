#include <log/log.h>
#include <coroutine/detail/context.h>

using namespace sk::detail;

static void *main_ctx = nullptr;

void *make_context(void *sp, size_t stack_size, void(*fn)(intptr_t)) asm("make_context");
intptr_t jump_context(void **ofc, void *nfc, intptr_t p, bool preserve_fpu) asm("jump_context");

context::~context() {
    sk_trace("context::~context(%p), stack(%p:%lu)", this, stack_, stack_size_);
    if (stack_) {
        free(stack_);
        stack_ = nullptr;
        stack_size_ = 0;

        // ctx_ actually points to an object on stack_, so reset it
        // after stack_ get freed
        ctx_ = nullptr;
    }
}

context *context::create(const coroutine_function& fn,
                         size_t stack_size, bool preserve_fpu) {
    context *ctx = new context();
    if (!ctx) return nullptr;

    ctx->stack_size_ = stack_size;
    ctx->stack_ = char_ptr(malloc(stack_size));
    if (!ctx->stack_) {
        delete ctx;
        return nullptr;
    }

    ctx->ctx_ = make_context(ctx->stack_, ctx->stack_size_, context_main);
    if (!ctx->ctx_) {
        delete ctx;
        return nullptr;
    }

    ctx->fn_ = fn;
    ctx->preserve_fpu_ = preserve_fpu;

    sk_trace("context::create(%p), stack(%p:%lu)", ctx, ctx->stack_, ctx->stack_size_);
    return ctx;
}

bool context::swap_in() {
    jump_context(&main_ctx, ctx_, reinterpret_cast<intptr_t>(this), preserve_fpu_);
    return true;
}

bool context::swap_out() {
    jump_context(&ctx_, main_ctx, reinterpret_cast<intptr_t>(this), preserve_fpu_);
    return true;
}

__asm(
".text\n"
".globl jump_context\n"
".type jump_context,@function\n"
".align 16\n"
"jump_context:\n"
"    pushq  %rbp  \n"
"    pushq  %rbx  \n"
"    pushq  %r15  \n"
"    pushq  %r14  \n"
"    pushq  %r13  \n"
"    pushq  %r12  \n"
"    leaq  -0x8(%rsp), %rsp\n"
"    cmp  $0, %rcx\n"
"    je  1f\n"
"    stmxcsr  (%rsp)\n"
"    fnstcw   0x4(%rsp)\n"
"1:\n"
"    movq  %rsp, (%rdi)\n"
"    movq  %rsi, %rsp\n"
"    cmp  $0, %rcx\n"
"    je  2f\n"
"    ldmxcsr  (%rsp)\n"
"    fldcw  0x4(%rsp)\n"
"2:\n"
"    leaq  0x8(%rsp), %rsp\n"
"    popq  %r12  \n"
"    popq  %r13  \n"
"    popq  %r14  \n"
"    popq  %r15  \n"
"    popq  %rbx  \n"
"    popq  %rbp  \n"
"    popq  %r8\n"
"    movq  %rdx, %rax\n"
"    movq  %rdx, %rdi\n"
"    jmp  *%r8\n"
".size jump_context,.-jump_context\n"
".section .note.GNU-stack,\"\",%progbits\n"
);

__asm(
".text\n"
".globl make_context\n"
".type make_context,@function\n"
".align 16\n"
"make_context:\n"
"    movq  %rdi, %rax\n"
"    andq  $-16, %rax\n"
"    leaq  -0x48(%rax), %rax\n"
"    movq  %rdx, 0x38(%rax)\n"
"    stmxcsr  (%rax)\n"
"    fnstcw   0x4(%rax)\n"
"    leaq  finish(%rip), %rcx\n"
"    movq  %rcx, 0x40(%rax)\n"
"    ret \n"
"finish:\n"
"    xorq  %rdi, %rdi\n"
"    call  _exit@PLT\n"
"    hlt\n"
".size make_context,.-make_context\n"
".section .note.GNU-stack,\"\",%progbits\n"
);
