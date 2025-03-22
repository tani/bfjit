#include <libgccjit.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    gcc_jit_block *cond_block;
    gcc_jit_block *after_block;
} LoopStackItem;

void bfc(const char *bf_code) {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE, 0);
    gcc_jit_context_set_int_option(ctx, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, 3);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
    gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    gcc_jit_param *params[] = {};
    gcc_jit_function *bf_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, int_type, "bf", 0, params, 0);
    gcc_jit_block *block = gcc_jit_function_new_block(bf_func, "entry");

    gcc_jit_type *a_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, 30000);
    gcc_jit_lvalue *a_var = gcc_jit_function_new_local(bf_func, NULL, a_type, "a");

    gcc_jit_param *memset_params[] = {
        gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "s"),
        gcc_jit_context_new_param(ctx, NULL, int_type, "c"),
        gcc_jit_context_new_param(ctx, NULL, int_type, "n")
    };
    gcc_jit_function *memset_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "memset", 3, memset_params, 0);
    gcc_jit_rvalue *memset_args[] = {
        gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(a_var, NULL), char_ptr_type),
        gcc_jit_context_zero(ctx, int_type),
        gcc_jit_context_new_rvalue_from_int(ctx, int_type, 30000)
    };
    gcc_jit_rvalue *memset_func_call = gcc_jit_context_new_call(ctx, NULL, memset_func, 3, memset_args);
    gcc_jit_block_add_eval(block, NULL, memset_func_call);

    gcc_jit_lvalue *p_var = gcc_jit_function_new_local(bf_func, NULL, char_ptr_type, "p");
    gcc_jit_rvalue *p_val = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(a_var, NULL), char_ptr_type);
    gcc_jit_block_add_assignment(block, NULL, p_var, p_val);

    gcc_jit_param *putchar_params[] = {
      gcc_jit_context_new_param(ctx, NULL, int_type, "c")
    };
    gcc_jit_function *putchar_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "putchar", 1, putchar_params, 0);

    gcc_jit_param *getchar_params[] = {};
    gcc_jit_function *getchar_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "getchar", 0, getchar_params, 0);

    /* ループ処理用のスタック（必要な最大サイズはソースコード長に依存） */
    size_t code_length = strlen(bf_code);
    LoopStackItem loop_stack[code_length];
    size_t loop_stack_size = 0;

    /* bf_code の各文字に対して処理 */
    for (size_t i = 0; bf_code[i] != '\0'; i++) {
        char c = bf_code[i];
        switch (c) {
            case '>': {
                gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                gcc_jit_lvalue *element = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(p_var), offset);
                gcc_jit_block_add_assignment(block, NULL, p_var, gcc_jit_lvalue_get_address(element, NULL));
                break;
            }
            case '<': {
                gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_int(ctx, int_type, -1);
                gcc_jit_lvalue *element = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(p_var), offset);
                gcc_jit_block_add_assignment(block, NULL, p_var, gcc_jit_lvalue_get_address(element, NULL));
                break;
            }
            case '+': {
                gcc_jit_lvalue *p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), NULL);
                gcc_jit_rvalue *one = gcc_jit_context_one(ctx, char_type);
                gcc_jit_block_add_assignment_op(block, NULL, p_deref, GCC_JIT_BINARY_OP_PLUS, one);
                break;
            }
            case '-': {
                gcc_jit_lvalue *p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), NULL);
                gcc_jit_rvalue *one = gcc_jit_context_one(ctx, char_type);
                gcc_jit_block_add_assignment_op(block, NULL, p_deref, GCC_JIT_BINARY_OP_MINUS, one);
                break;
            }
            case '.': {
                gcc_jit_lvalue *p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), NULL);
                gcc_jit_rvalue *putchar_args[1];
                putchar_args[0] = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(p_deref), int_type);
                gcc_jit_rvalue *putchar_call = gcc_jit_context_new_call(ctx, NULL, putchar_func, 1, putchar_args);
                gcc_jit_block_add_eval(block, NULL, putchar_call);
                break;
            }
            case ',': {
                gcc_jit_lvalue *p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), NULL);
                gcc_jit_rvalue *getchar_call = gcc_jit_context_new_call(ctx, NULL, getchar_func, 0, NULL);
                gcc_jit_block_add_assignment(block, NULL, p_deref, gcc_jit_context_new_cast(ctx, NULL, getchar_call, char_type));
                break;
            }
            case '[': {
                gcc_jit_block *cond_block = gcc_jit_function_new_block(bf_func, "condition");
                gcc_jit_block *body_block = gcc_jit_function_new_block(bf_func, "body");
                gcc_jit_block *after_block = gcc_jit_function_new_block(bf_func, "after");
                gcc_jit_block_end_with_jump(block, NULL, cond_block);
                gcc_jit_lvalue *p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), NULL);
                gcc_jit_rvalue *zero = gcc_jit_context_zero(ctx, char_type);
                gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, gcc_jit_lvalue_as_rvalue(p_deref), zero);
                gcc_jit_block_end_with_conditional(cond_block, NULL, cond, body_block, after_block);
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
                gcc_jit_block *cond_block = loop_stack[loop_stack_size].cond_block;
                gcc_jit_block *after_block = loop_stack[loop_stack_size].after_block;
                gcc_jit_block_end_with_jump(block, NULL, cond_block);
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
    gcc_jit_block_end_with_return(block, NULL, gcc_jit_context_zero(ctx, int_type));

    gcc_jit_result *result = gcc_jit_context_compile(ctx);
    if (!result) {
        fprintf(stderr, "Compilation failed\n");
        gcc_jit_context_release(ctx);
        return;
    }
    typedef int (*bf_func_type)();
    bf_func_type bf = (bf_func_type)gcc_jit_result_get_code(result, "bf");
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
    char *p = a;
    int i = 0;
    size_t code_length = strlen(bf_code);
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
                    int cnt = 1;
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
                    int cnt = 1;
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
    int use_jit = 0;
    const char *filename;
    if (argc == 2) {
        filename = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "--jit") == 0) {
        use_jit = 1;
        filename = argv[2];
    } else {
        fprintf(stderr, "usage: bf [--jit] <filename>\n");
        fprintf(stderr, "your input:");
        for (int i = 1; i < argc; i++)
            fprintf(stderr, " %s", argv[i]);
        fprintf(stderr, "\n");
        return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Unable to open file '%s'\n", filename);
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: fseek failed\n");
        fclose(fp);
        return 1;
    }
    long size = ftell(fp);
    if (size < 0) {
        fprintf(stderr, "Error: ftell failed\n");
        fclose(fp);
        return 1;
    }
    rewind(fp);
    char buffer[size + 1];
    size_t read_size = fread(buffer, 1, size, fp);
    if (read_size != (size_t)size && ferror(fp)) {
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
