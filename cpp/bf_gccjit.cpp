#include <libgccjit.h>

#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void bfc(const std::string &bf_code) {
  auto ctx = gcc_jit_context_acquire();
  gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE, 0);
  gcc_jit_context_set_int_option(ctx, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, 3);
  auto int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
  auto char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
  auto char_ptr_type = gcc_jit_type_get_pointer(char_type);
  auto void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
  gcc_jit_param* params[] = {};
  auto bf_func = gcc_jit_context_new_function(ctx, nullptr, GCC_JIT_FUNCTION_EXPORTED, int_type, "bf", 0, params, 0);
  auto block = gcc_jit_function_new_block(bf_func, "entry");
  auto a_type = gcc_jit_context_new_array_type(ctx, nullptr, char_type, 30000);
  auto a_var = gcc_jit_function_new_local(bf_func, nullptr, a_type, "a");
  gcc_jit_param* memset_params[] = {
    gcc_jit_context_new_param(ctx, nullptr, void_ptr_type, "s"),
    gcc_jit_context_new_param(ctx, nullptr, int_type, "c"),
    gcc_jit_context_new_param(ctx, nullptr, int_type, "n")
  };
  auto memset_func = gcc_jit_context_new_function(ctx, nullptr, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "memset", 3, memset_params, 0);
  gcc_jit_rvalue* memset_args[] = {
    gcc_jit_context_new_cast(ctx, nullptr, gcc_jit_lvalue_get_address(a_var, nullptr), char_ptr_type),
    gcc_jit_context_zero(ctx, int_type),
    gcc_jit_context_new_rvalue_from_int(ctx, int_type, 30000)
  };
  auto memset_func_call = gcc_jit_context_new_call(ctx, nullptr, memset_func, 3, memset_args);
  gcc_jit_block_add_eval(block, nullptr, memset_func_call);
  auto p_var = gcc_jit_function_new_local(bf_func, nullptr, char_ptr_type, "p");
  gcc_jit_block_add_assignment(
    block,
    nullptr,
    p_var,
    gcc_jit_context_new_cast(ctx, nullptr, gcc_jit_lvalue_get_address(a_var, nullptr), char_ptr_type)
  );
  // gcc_jit_block_add_assignment(block, nullptr, p_var, gcc_jit_lvalue_get_address(a_var, nullptr));
  gcc_jit_param* putchar_params[] = {
    gcc_jit_context_new_param(ctx, nullptr, int_type, "c")
  };
  auto putchar_func = gcc_jit_context_new_function(ctx, nullptr, GCC_JIT_FUNCTION_IMPORTED, int_type, "putchar", 1, putchar_params, 0);
  gcc_jit_param* getchar_params[] = {};
  auto getchar_func = gcc_jit_context_new_function(ctx, nullptr, GCC_JIT_FUNCTION_IMPORTED, int_type, "getchar", 0, getchar_params, 0);
  std::vector<std::pair<gcc_jit_block*, gcc_jit_block*>> loop_stack = {};
  for (char c : bf_code) {
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
        gcc_jit_rvalue* putchar_args[] = {gcc_jit_context_new_cast(ctx, nullptr, gcc_jit_lvalue_as_rvalue(p_deref), int_type)};
        auto putchar_call = gcc_jit_context_new_call(ctx, nullptr, putchar_func, 1, putchar_args);
        gcc_jit_block_add_eval(block, nullptr, putchar_call);
        break;
      }
      case ',': {
        auto p_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(p_var), nullptr);
        gcc_jit_rvalue* args[] = {};
        auto getchar_call = gcc_jit_context_new_call(ctx, nullptr, getchar_func, 0, args);
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
        loop_stack.push_back(std::make_pair(cond_block, after_block));
        block = body_block;
        break;
      }
      case ']': {
        if (loop_stack.empty()) {
          std::cerr << "Unmatched ']'" << std::endl;
          gcc_jit_context_release(ctx);
          return;
        }
        auto loop_blocks = loop_stack.back();
        loop_stack.pop_back();
        gcc_jit_block_end_with_jump(block, nullptr, loop_blocks.first);
        block = loop_blocks.second;
        break;
      }
    }
  }
  if (!loop_stack.empty()) {
    std::cerr << "Unmatched '['" << std::endl;
    gcc_jit_context_release(ctx);
    return;
  }
  gcc_jit_block_end_with_return(block, nullptr, gcc_jit_context_zero(ctx, int_type));
  // Compile the function
  gcc_jit_result *result = gcc_jit_context_compile(ctx);
  if (!result) {
    std::cerr << "Compilation failed" << std::endl;
    gcc_jit_context_release(ctx);
    return;
  }
  typedef int (*bf_func_type)();
  bf_func_type bf = (bf_func_type)gcc_jit_result_get_code(result, "bf");
  if (!bf) {
    std::cerr << "Failed to get compiled function" << std::endl;
    gcc_jit_result_release(result);
    gcc_jit_context_release(ctx);
    return;
  }
  // Execute the compiled function
  bf();
  // Clean up
  gcc_jit_result_release(result);
  gcc_jit_context_release(ctx);
}

void bfi(const std::string &bf_code) {
  char a[30000] = {0};
  char *p = a;
  int i = 0;
  while (i < bf_code.size()) {
    switch (bf_code[i]) {
      case '>':
        ++p;
        break;
      case '<':
        --p;
        break;
      case '+':
        ++*p;
        break;
      case '-':
        --*p;
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
            ++i;
            if (i >= bf_code.size()) {
              std::cerr << "Unmatched '['" << std::endl;
              return;
            }
            if (bf_code[i] == '[') ++cnt;
            if (bf_code[i] == ']') --cnt;
          }
        }
        break;
      }
      case ']': {
        if (*p != 0) {
          int cnt = 1;
          while (cnt > 0) {
            --i;
            if (i < 0) {
              std::cerr << "Unmatched ']'" << std::endl;
              return;
            }
            if (bf_code[i] == '[') --cnt;
            if (bf_code[i] == ']') ++cnt;
          }
        }
        break;
      }
    }
    ++i;
  }
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    auto filename = argv[1];
    std::ifstream ifs(filename);
    if (!ifs) {
      std::cerr << "Failed to open file: " << filename << std::endl;
      return 1;
    }
    std::string bf_code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    bfi(bf_code);
  } else if (argc == 3 && argv[1] == std::string("--jit")) {
    auto filename = argv[2];
    std::ifstream ifs(filename);
    if (!ifs) {
      std::cerr << "Failed to open file: " << filename << std::endl;
      return 1;
    }
    std::string bf_code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    bfc(bf_code);
  } else {
    std::cerr << "usage: bf [--jit] <filename>" << std::endl;
    std::cerr << "your input: " << "bf ";
    for (int i = 1; i < argc; ++i) {
      std::cerr << argv[i] << " ";
    }
    return 1;
  }
  return 0;
}

