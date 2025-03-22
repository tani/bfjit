#include <libgccjit.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    gcc_jit_block *cond_block;
    gcc_jit_block *after_block;
} LoopStackItem;

void bfc(const char *bf_code) {
    auto ctx = gcc_jit_context_acquire();
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE, 0);
    gcc_jit_context_set_int_option(ctx, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, 3);
    auto int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    auto char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
    auto char_ptr_type = gcc_jit_type_get_pointer(char_type);
    auto void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    gcc_jit_param *params[] = {};
    auto bf_func = gcc_jit_context_new_function(ctx, nullptr, GCC_JIT_FUNCTION_EXPORTED, int_type, "bf", 0, params, 0);
    auto block = gcc_jit_function_new_block(bf_func, "entry");

    auto a_type = gcc_jit_context_new_array_type(ctx, nullptr, char_type, 30000);
    auto a_var = gcc_jit_function_new_local(bf_func, nullptr, a_type, "a");

    gcc_jit_param *memset_params[] = {
        gcc_jit_context_new_param(ctx, nullptr, void_ptr_type, "s"),
        gcc_jit_context_new_param(ctx, nullptr, int_type, "c"),
        gcc_jit_context_new_param(ctx, nullptr, int_type, "n")
    };
    auto memset_func = gcc_jit_context_new_function(ctx, nullptr, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "memset", 3, memset_params, 0);
    gcc_jit_rvalue *memset_args[] = {
        gcc_jit_context_new_cast(ctx, nullptr, gcc_jit_lvalue_get_address(a_var, nullptr), char_ptr_type),
        gcc_jit_context_zero(ctx, int_type),
        gcc_jit_context_new_rvalue_from_int(ctx, int_type, 30000)
    };
    auto memset_func_call = gcc_jit_context_new_call(ctx, nullptr, memset_func, 3, memset_args);
    gcc_jit_block_add_eval(block, nullptr, memset_func_call);

    auto p_var = gcc_jit_function_new_local(bf_func, nullptr, char_ptr_type, "p");
    auto p_val = gcc_jit_context_new_cast(ctx, nullptr, gcc_jit_lvalue_get_address(a_var, nullptr), char_ptr_type);
    gcc_jit_block_add_assignment(block, nullptr, p_var, p_val);

    gcc_jit_param *putchar_params[] = {
      gcc_jit_context_new_param(ctx, nullptr, int_type, "c")
    };
    auto putchar_func = gcc_jit_context_new_function(ctx, nullptr, GCC_JIT_FUNCTION_IMPORTED, int_type, "putchar", 1, putchar_params, 0);

    gcc_jit_param *getchar_params[] = {};
    auto getchar_func = gcc_jit_context_new_function(ctx, nullptr, GCC_JIT_FUNCTION_IMPORTED, int_type, "getchar", 0, getchar_params, 0);

    /* ループ処理用のスタック（必要な最大サイズはソースコード長に依存） */
    auto code_length = strlen(bf_code);
    LoopStackItem loop_stack[code_length];
    auto loop_stack_size = 0;

    /* bf_code の各文字に対して処理 */
    for (auto i = 0; bf_code[i] != '\0'; i++) {
        char c = bf_code[i];
        switch (c) {
            case '>': {
                auto offset = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                auto element = gcc_jit_context_new_array_access(ctx, nullptr, gcc_jit_lvalue_as_rvalue(p_var), offset);
                gcc_jit_block_add_assignment(block, nullptr, p_var, gcc_jit_lvalue_get_address(element, nullptr));
                break;
            }
            case '<': {
                auto offset = gcc_jit_context_new_rvalue_from_int(ctx, int_type, -1);
                auto element = gcc_jit_context_new_array_access(ctx, nullptr, gcc_jit_lvalue_as_rvalue(p_var), offset);
                gcc_jit_block_add_assignment(block, nullptr, p_var, gcc_jit_lvalue_get_address(element, nullptr));
                break;
            }
            case '+': {
                auto p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), nullptr);
                auto one = gcc_jit_context_one(ctx, char_type);
                gcc_jit_block_add_assignment_op(block, nullptr, p_deref, GCC_JIT_BINARY_OP_PLUS, one);
                break;
            }
            case '-': {
                auto p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), nullptr);
                auto one = gcc_jit_context_one(ctx, char_type);
                gcc_jit_block_add_assignment_op(block, nullptr, p_deref, GCC_JIT_BINARY_OP_MINUS, one);
                break;
            }
            case '.': {
                auto p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), nullptr);
                gcc_jit_rvalue *putchar_args[1];
                putchar_args[0] = gcc_jit_context_new_cast(ctx, nullptr, gcc_jit_lvalue_as_rvalue(p_deref), int_type);
                auto putchar_call = gcc_jit_context_new_call(ctx, nullptr, putchar_func, 1, putchar_args);
                gcc_jit_block_add_eval(block, nullptr, putchar_call);
                break;
            }
            case ',': {
                auto p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), nullptr);
                auto getchar_call = gcc_jit_context_new_call(ctx, nullptr, getchar_func, 0, nullptr);
                gcc_jit_block_add_assignment(block, nullptr, p_deref, gcc_jit_context_new_cast(ctx, nullptr, getchar_call, char_type));
                break;
            }
            case '[': {
                auto cond_block = gcc_jit_function_new_block(bf_func, "condition");
                auto body_block = gcc_jit_function_new_block(bf_func, "body");
                auto after_block = gcc_jit_function_new_block(bf_func, "after");
                gcc_jit_block_end_with_jump(block, nullptr, cond_block);
                auto p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), nullptr);
                auto zero = gcc_jit_context_zero(ctx, char_type);
                auto cond = gcc_jit_context_new_comparison(ctx, nullptr, GCC_JIT_COMPARISON_NE, gcc_jit_lvalue_as_rvalue(p_deref), zero);
                gcc_jit_block_end_with_conditional(cond_block, nullptr, cond, body_block, after_block);
                loop_stack[loop_stack_size].cond_block = cond_block;
                loop_stack[loop_stack_size].after_block = after_block;
                loop_stack_size++;
                block = body_block;
                break;
            }
            case ']': {
                if (loop_stack_size == 0) {
                    fprintf(stderr, "Unmatched ']'\n");
                    gcc_jit_context_release(ctx);
                    return;
                }
                loop_stack_size--;
                auto cond_block = loop_stack[loop_stack_size].cond_block;
                auto after_block = loop_stack[loop_stack_size].after_block;
                gcc_jit_block_end_with_jump(block, nullptr, cond_block);
                block = after_block;
                break;
            }
        }
    }
    if (loop_stack_size != 0) {
        fprintf(stderr, "Unmatched '['\n");
        gcc_jit_context_release(ctx);
        return;
    }
    gcc_jit_block_end_with_return(block, nullptr, gcc_jit_context_zero(ctx, int_type));

    auto result = gcc_jit_context_compile(ctx);
    if (!result) {
        fprintf(stderr, "Compilation failed\n");
        gcc_jit_context_release(ctx);
        return;
    }
    typedef int (*bf_func_type)();
    auto bf = (bf_func_type)gcc_jit_result_get_code(result, "bf");
    if (!bf) {
        fprintf(stderr, "Failed to get compiled function\n");
        gcc_jit_result_release(result);
        gcc_jit_context_release(ctx);
        return;
    }
    bf();
    gcc_jit_result_release(result);
    gcc_jit_context_release(ctx);
}

void bfi(const char *bf_code) {
    char a[30000] = {};
    auto p = a;
    auto i = 0;
    auto code_length = strlen(bf_code);
    while (i < code_length) {
        switch (bf_code[i]) {
            case '>':
                ++p;
                break;
            case '<':
                --p;
                break;
            case '+':
                ++(*p);
                break;
            case '-':
                --(*p);
                break;
            case '.':
                putchar(*p);
                break;
            case ',':
                *p = getchar();
                break;
            case '[': {
                if (*p == 0) {
                    auto cnt = 1;
                    while (cnt > 0) {
                        i++;
                        if (i >= code_length) {
                            fprintf(stderr, "Unmatched '['\n");
                            return;
                        }
                        if (bf_code[i] == '[') cnt++;
                        if (bf_code[i] == ']') cnt--;
                    }
                }
                break;
            }
            case ']': {
                if (*p != 0) {
                    auto cnt = 1;
                    while (cnt > 0) {
                        if (i == 0) {
                            fprintf(stderr, "Unmatched ']'\n");
                            return;
                        }
                        i--;
                        if (bf_code[i] == '[') cnt--;
                        if (bf_code[i] == ']') cnt++;
                    }
                }
                break;
            }
        }
        i++;
    }
}

int main(int argc, char *argv[]) {
    auto use_jit = false;
    const char *filename;
    if (argc == 2) {
        filename = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "--jit") == 0) {
        use_jit = 1;
        filename = argv[2];
    } else {
        fprintf(stderr, "usage: bf [--jit] <filename>\n");
        fprintf(stderr, "your input:");
        for (auto i = 1; i < argc; i++)
            fprintf(stderr, " %s", argv[i]);
        fprintf(stderr, "\n");
        return 1;
    }

    auto fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Unable to open file '%s'\n", filename);
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: fseek failed\n");
        fclose(fp);
        return 1;
    }
    auto size = ftell(fp);
    if (size < 0) {
        fprintf(stderr, "Error: ftell failed\n");
        fclose(fp);
        return 1;
    }
    rewind(fp);
    char buffer[size + 1];
    auto read_size = fread(buffer, 1, size, fp);
    if (read_size != size && ferror(fp)) {
        fprintf(stderr, "Error: fread failed\n");
        fclose(fp);
        return 1;
    }
    buffer[read_size] = '\0';
    fclose(fp);

    if (use_jit)
        bfc(buffer);
    else
        bfi(buffer);
    return 0;
}
