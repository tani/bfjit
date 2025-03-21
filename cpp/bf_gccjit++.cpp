#include <libgccjit++.h>

#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void bfc(const std::string &bf_code) {
  // Create a context
  gccjit::context ctx = gccjit::context::acquire();
  // Set options
  ctx.set_bool_option(GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE, 0);
  ctx.set_int_option(GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, 3);
  // Define types we'll need
  gccjit::type int_type = ctx.get_type(GCC_JIT_TYPE_INT);
  gccjit::type char_type = ctx.get_type(GCC_JIT_TYPE_CHAR);
  gccjit::type char_ptr_type = char_type.get_pointer();
  gccjit::type void_ptr_type = ctx.get_type(GCC_JIT_TYPE_VOID_PTR);
  gccjit::type size_t_type = ctx.get_type(GCC_JIT_TYPE_SIZE_T);
  // Create function: int bf()
  std::vector<gccjit::param> params = {};
  gccjit::function bf_func = ctx.new_function(GCC_JIT_FUNCTION_EXPORTED, int_type, "bf", params, 0);
  // Create the entry block
  gccjit::block block = bf_func.new_block("entry");
  // Define array: char a[30000]
  gccjit::lvalue a = bf_func.new_local(ctx.new_array_type(char_type, 30000), "a");
  // Import memset to zero-initialize array
  std::vector<gccjit::param> memset_params = {
    ctx.new_param(void_ptr_type, "s"),
    ctx.new_param(int_type, "c"),
    ctx.new_param(size_t_type, "n")
  };
  gccjit::function memset_func =
      ctx.new_function(GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "memset", memset_params, 0);
  // Initialize array to zeros
  std::vector<gccjit::rvalue> memset_args = {
    ctx.new_array_access(a, ctx.zero(int_type)).get_address(),
    ctx.zero(int_type),
    ctx.new_rvalue(size_t_type, 30000)
  };
  block.add_eval(ctx.new_call(memset_func, memset_args));
  // char *p = a;
  gccjit::lvalue p = bf_func.new_local(char_ptr_type, "p");
  block.add_assignment(p, ctx.new_array_access(a, ctx.zero(int_type)).get_address());
  // Import putchar and getchar
  std::vector<gccjit::param> putchar_params = {
    ctx.new_param(int_type, "c")
  };
  gccjit::function putchar_func = ctx.new_function(GCC_JIT_FUNCTION_IMPORTED, int_type, "putchar", putchar_params, 0);
  std::vector<gccjit::param> getchar_params;
  gccjit::function getchar_func = ctx.new_function(GCC_JIT_FUNCTION_IMPORTED, int_type, "getchar", getchar_params, 0);
  // Process Brainfuck code
  std::vector<std::pair<gccjit::block, gccjit::block>> loop_stack = {};
  for (char c : bf_code) {
    switch (c) {
      case '>': {
        // ++p の実装
        gccjit::rvalue offset = ctx.new_rvalue(int_type, 1);
        gccjit::lvalue element = ctx.new_array_access(p, offset);
        gccjit::rvalue p_plus_one = element.get_address();
        block.add_assignment(p, p_plus_one);
        break;
      }
      case '<': {
        // ++p の実装
        gccjit::rvalue offset = ctx.new_rvalue(int_type, -1);
        gccjit::lvalue element = ctx.new_array_access(p, offset);
        gccjit::rvalue p_minus_one = element.get_address();
        block.add_assignment(p, p_minus_one);
        break;
      }
      case '+': {
        // ++*p;
        gccjit::lvalue deref_p = p.dereference();
        gccjit::rvalue one = ctx.one(char_type);
        block.add_assignment_op(deref_p, GCC_JIT_BINARY_OP_PLUS, one);
        break;
      }
      case '-': {
        // --*p;
        gccjit::lvalue deref_p = p.dereference();
        gccjit::rvalue one = ctx.one(char_type);
        block.add_assignment_op(deref_p, GCC_JIT_BINARY_OP_MINUS, one);
        break;
      }
      case '.': {
        // putchar(*p);
        gccjit::rvalue deref_p = p.dereference();
        std::vector<gccjit::rvalue> args = {ctx.new_cast(deref_p, int_type)};
        block.add_eval(ctx.new_call(putchar_func, args));
        break;
      }
      case ',': {
        // *p = getchar();
        gccjit::lvalue deref_p = p.dereference();
        std::vector<gccjit::rvalue> args = {};
        gccjit::rvalue getchar_call = ctx.new_call(getchar_func, args);
        block.add_assignment(deref_p, ctx.new_cast(getchar_call, char_type));
        break;
      }
      case '[': {
        // while (*p) {
        gccjit::block condition_block = bf_func.new_block("condition");
        gccjit::block body_block = bf_func.new_block("body");
        gccjit::block after_block = bf_func.new_block("after");
        // Jump to condition
        block.end_with_jump(condition_block);
        // Check if *p != 0
        gccjit::rvalue deref_p = p.dereference();
        gccjit::rvalue zero = ctx.zero(char_type);
        gccjit::rvalue condition = ctx.new_comparison(GCC_JIT_COMPARISON_NE, deref_p, zero);
        condition_block.end_with_conditional(condition, body_block, after_block);
        loop_stack.push_back(std::make_pair(condition_block, after_block));
        block = body_block;
        break;
      }
      case ']': {
        if (loop_stack.empty()) {
          std::cerr << "Unmatched ']'" << std::endl;
          ctx.release();
          return;
        }
        auto loop_blocks = loop_stack.back();
        loop_stack.pop_back();
        // Jump back to condition
        block.end_with_jump(loop_blocks.first);
        // Continue after loop
        block = loop_blocks.second;
        break;
      }
    }
  }
  // Check for unmatched '['
  if (!loop_stack.empty()) {
    std::cerr << "Unmatched '['" << std::endl;
    ctx.release();
    return;
  }
  block.end_with_return(ctx.zero(int_type));
  // Compile the function
  gcc_jit_result *result = ctx.compile();
  if (!result) {
    std::cerr << "Compilation failed" << std::endl;
    ctx.release();
    return;
  }
  // Get and call the function
  typedef int (*bf_func_type)();
  bf_func_type bf = (bf_func_type)gcc_jit_result_get_code(result, "bf");
  if (!bf) {
    std::cerr << "Failed to get compiled function" << std::endl;
    gcc_jit_result_release(result);
    ctx.release();
    return;
  }
  // Execute the compiled function
  bf();
  // Clean up
  gcc_jit_result_release(result);
  ctx.release();
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

