const std = @import("std");

const c = @cImport({
    @cInclude("libgccjit.h");
    @cInclude("stdio.h");
    @cInclude("string.h");
});

const LoopStackItem = struct {
    cond_block: *c.gcc_jit_block,
    after_block: *c.gcc_jit_block,
};

/// JIT コンパイル版の Brainfuck 実行関数
fn bfc(bf_code: []const u8) !void {
    const allocator = std.heap.page_allocator;

    // コンテキストの作成と各種オプション設定
    const ctx = c.gcc_jit_context_acquire();
    c.gcc_jit_context_set_bool_option(ctx, c.GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE, 0);
    c.gcc_jit_context_set_int_option(ctx, c.GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, 3);
    const int_type = c.gcc_jit_context_get_type(ctx, c.GCC_JIT_TYPE_INT);
    const char_type = c.gcc_jit_context_get_type(ctx, c.GCC_JIT_TYPE_CHAR);
    const char_ptr_type = c.gcc_jit_type_get_pointer(char_type);
    const void_ptr_type = c.gcc_jit_context_get_type(ctx, c.GCC_JIT_TYPE_VOID_PTR);

    // bf() 関数を作成
    const bf_func = c.gcc_jit_context_new_function(ctx, null, c.GCC_JIT_FUNCTION_EXPORTED, int_type, "bf", 0, null, 0);
    const block = c.gcc_jit_function_new_block(bf_func, "entry");

    // 配列 a[30000] を作成
    const a_type = c.gcc_jit_context_new_array_type(ctx, null, char_type, 30000);
    const a_var = c.gcc_jit_function_new_local(bf_func, null, a_type, "a");

    // memset(s, c, n) 関数の呼び出し準備
    const memset_params: [3]*c.gcc_jit_param = .{
        c.gcc_jit_context_new_param(ctx, null, void_ptr_type, "s"),
        c.gcc_jit_context_new_param(ctx, null, int_type, "c"),
        c.gcc_jit_context_new_param(ctx, null, int_type, "n"),
    };
    const memset_func = c.gcc_jit_context_new_function(ctx, null, c.GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "memset", 3, &memset_params[0], 0);
    const memset_args: [3]*c.gcc_jit_rvalue = .{
        c.gcc_jit_context_new_cast(ctx, null, c.gcc_jit_lvalue_get_address(a_var, null), char_ptr_type),
        c.gcc_jit_context_zero(ctx, int_type),
        c.gcc_jit_context_new_rvalue_from_int(ctx, int_type, 30000),
    };
    const memset_func_call = c.gcc_jit_context_new_call(ctx, null, memset_func, 3, &memset_args[0]);
    c.gcc_jit_block_add_eval(block, null, memset_func_call);

    // ポインタ p を a の先頭に設定
    const p_var = c.gcc_jit_function_new_local(bf_func, null, char_ptr_type, "p");
    const p_val = c.gcc_jit_context_new_cast(ctx, null, c.gcc_jit_lvalue_get_address(a_var, null), char_ptr_type);
    c.gcc_jit_block_add_assignment(block, null, p_var, p_val);

    // putchar()、getchar() の準備
    const putchar_params: [1]*c.gcc_jit_param = .{
        c.gcc_jit_context_new_param(ctx, null, int_type, "c"),
    };
    const putchar_func = c.gcc_jit_context_new_function(ctx, null, c.GCC_JIT_FUNCTION_IMPORTED, int_type, "putchar", 1, &putchar_params[0], 0);
    const getchar_func = c.gcc_jit_context_new_function(ctx, null, c.GCC_JIT_FUNCTION_IMPORTED, int_type, "getchar", 0, null, 0);

    // ループ用スタック（bf_code の長さに依存）
    const code_length = bf_code.len;
    const loop_stack = try allocator.alloc(LoopStackItem, code_length);
    defer allocator.free(loop_stack);
    const loop_stack_size: usize = 0;

    // bf_code の各文字に対する処理
    const i: usize = 0;
    while (i < code_length) : (i += 1) {
        const current = bf_code[i];
        switch (current) {
            '>' => {
                const offset = c.gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                const element = c.gcc_jit_context_new_array_access(ctx, null, c.gcc_jit_lvalue_as_rvalue(p_var), offset);
                c.gcc_jit_block_add_assignment(block, null, p_var, c.gcc_jit_lvalue_get_address(element, null));
            },
            '<' => {
                const offset = c.gcc_jit_context_new_rvalue_from_int(ctx, int_type, -1);
                const element = c.gcc_jit_context_new_array_access(ctx, null, c.gcc_jit_lvalue_as_rvalue(p_var), offset);
                c.gcc_jit_block_add_assignment(block, null, p_var, c.gcc_jit_lvalue_get_address(element, null));
            },
            '+' => {
                const p_deref = c.gcc_jit_rvalue_dereference(c.gcc_jit_lvalue_as_rvalue(p_var), null);
                const one = c.gcc_jit_context_one(ctx, char_type);
                c.gcc_jit_block_add_assignment_op(block, null, p_deref, c.GCC_JIT_BINARY_OP_PLUS, one);
            },
            '-' => {
                const p_deref = c.gcc_jit_rvalue_dereference(c.gcc_jit_lvalue_as_rvalue(p_var), null);
                const one = c.gcc_jit_context_one(ctx, char_type);
                c.gcc_jit_block_add_assignment_op(block, null, p_deref, c.GCC_JIT_BINARY_OP_MINUS, one);
            },
            '.' => {
                const p_deref = c.gcc_jit_rvalue_dereference(c.gcc_jit_lvalue_as_rvalue(p_var), null);
                const putchar_args: [1]*c.gcc_jit_rvalue = .{
                    c.gcc_jit_context_new_cast(ctx, null, c.gcc_jit_lvalue_as_rvalue(p_deref), int_type),
                };
                const putchar_call = c.gcc_jit_context_new_call(ctx, null, putchar_func, 1, &putchar_args[0]);
                c.gcc_jit_block_add_eval(block, null, putchar_call);
            },
            ',' => {
                const p_deref = c.gcc_jit_rvalue_dereference(c.gcc_jit_lvalue_as_rvalue(p_var), null);
                const getchar_call = c.gcc_jit_context_new_call(ctx, null, getchar_func, 0, null);
                c.gcc_jit_block_add_assignment(block, null, p_deref, c.gcc_jit_context_new_cast(ctx, null, getchar_call, char_type));
            },
            '[' => {
                const cond_block = c.gcc_jit_function_new_block(bf_func, "condition");
                const body_block = c.gcc_jit_function_new_block(bf_func, "body");
                const after_block = c.gcc_jit_function_new_block(bf_func, "after");
                c.gcc_jit_block_end_with_jump(block, null, cond_block);
                const p_deref = c.gcc_jit_rvalue_dereference(c.gcc_jit_lvalue_as_rvalue(p_var), null);
                const zero = c.gcc_jit_context_zero(ctx, char_type);
                const cond = c.gcc_jit_context_new_comparison(ctx, null, c.GCC_JIT_COMPARISON_NE, c.gcc_jit_lvalue_as_rvalue(p_deref), zero);
                c.gcc_jit_block_end_with_conditional(cond_block, null, cond, body_block, after_block);
                loop_stack[loop_stack_size] = LoopStackItem{
                    .cond_block = cond_block,
                    .after_block = after_block,
                };
                loop_stack_size += 1;
                block = body_block;
            },
            ']' => {
                if (loop_stack_size == 0) {
                    std.debug.print("Unmatched ']'\n", .{});
                    c.gcc_jit_context_release(ctx);
                    return;
                }
                loop_stack_size -= 1;
                const cond_block = loop_stack[loop_stack_size].cond_block;
                const after_block = loop_stack[loop_stack_size].after_block;
                c.gcc_jit_block_end_with_jump(block, null, cond_block);
                block = after_block;
            },
            else => {},
        }
    }

    if (loop_stack_size != 0) {
        std.debug.print("Unmatched '['\n", .{});
        c.gcc_jit_context_release(ctx);
        return;
    }

    c.gcc_jit_block_end_with_return(block, null, c.gcc_jit_context_zero(ctx, int_type));

    const result = c.gcc_jit_context_compile(ctx);
    if (result == null) {
        std.debug.print("Compilation failed\n", .{});
        c.gcc_jit_context_release(ctx);
        return;
    }

    // 修正: 関数型の定義構文を修正
    const bf_func_type = fn () callconv(.C) c_int;
    const bf_ptr = c.gcc_jit_result_get_code(result, "bf");
    if (bf_ptr == null) {
        std.debug.print("Failed to get compiled function\n", .{});
        c.gcc_jit_result_release(result);
        c.gcc_jit_context_release(ctx);
        return;
    }

    // 修正: 適切な関数ポインタのキャストを使用
    // const bf = @ptrCast(*const bf_func_type, bf_ptr);
    const bf: *const bf_func_type = @ptrCast(@alignCast(bf_ptr));
    bf();

    c.gcc_jit_result_release(result);
    c.gcc_jit_context_release(ctx);
}

/// インタプリタ版の Brainfuck 実行関数
fn bfi(bf_code: []const u8) void {
    const a: [30000]u8 = undefined;
    // 配列をゼロ初期化
    std.mem.set(u8, a[0..], 0);
    const p = &a[0];
    const i: usize = 0;
    const code_length = bf_code.len;
    while (i < code_length) : (i += 1) {
        const ch = bf_code[i];
        switch (ch) {
            '>' => p = p + 1,
            '<' => p = p - 1,
            '+' => p.* = p.* + 1,
            '-' => p.* = p.* - 1,
            '.' => {
                std.debug.print("{c}", .{p.*});
            },
            ',' => {
                const input_byte = std.io.getStdIn().readByte() catch 0;
                p.* = input_byte;
            },
            '[' => {
                if (p.* == 0) {
                    const cnt: i32 = 1;
                    while (cnt > 0) : (i += 1) {
                        if (i >= code_length) {
                            std.debug.print("Unmatched '['\n", .{});
                            return;
                        }
                        if (bf_code[i] == '[') cnt += 1;
                        if (bf_code[i] == ']') cnt -= 1;
                    }
                }
            },
            ']' => {
                if (p.* != 0) {
                    const cnt: i32 = 1;
                    while (cnt > 0) : (i -= 1) {
                        if (i == 0) {
                            std.debug.print("Unmatched ']'\n", .{});
                            return;
                        }
                        if (bf_code[i] == '[') cnt -= 1;
                        if (bf_code[i] == ']') cnt += 1;
                    }
                }
            },
            else => {},
        }
    }
}

/// エントリポイント
pub fn main() !void {
    const args = try std.process.argsAlloc(std.heap.page_allocator);
    defer std.process.argsFree(std.heap.page_allocator, args);

    var use_jit = false;
    var filename: ?[]const u8 = null;

    if (args.len > 1) {
        if (std.mem.eql(u8, args[1], "--jit")) {
            use_jit = true;
            if (args.len > 2) {
                filename = args[2];
            } else {
                std.debug.print("usage: bf [--jit] <filename>\n", .{});
                return error.InvalidArgument;
            }
        } else {
            filename = args[1];
        }
    } else {
        std.debug.print("usage: bf [--jit] <filename>\n", .{});
        return error.InvalidArgument;
    }

    const file = try std.fs.cwd().openFile(filename.?, .{});
    defer file.close();
    const buffer = try file.readToEndAlloc(std.heap.page_allocator, 8192);
    defer std.heap.page_allocator.free(buffer);

    if (use_jit) {
        try bfc(buffer);
    } else {
        bfi(buffer);
    }
}
