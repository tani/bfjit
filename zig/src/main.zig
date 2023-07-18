const std = @import("std");
const tcc = @cImport({
    @cInclude("libtcc.h");
});

const TccError = error{
    InitializationError,
    CompilationError,
    RelocationError,
    UndefinedSymbolError,
};

fn bfc(alloc: std.mem.Allocator, bf_code: []const u8) !void {
    var buf = std.ArrayList(u8).init(alloc);
    defer buf.deinit();
    const writer = buf.writer();
    _ = try writer.write(
        \\int putchar(int c);
        \\void bf() {
        \\char arr[30000] = {0};
        \\char *ptr = arr;
    );
    for (bf_code) |c| {
        _ = try writer.write(switch (c) {
            '<' => "ptr--;\n",
            '>' => "ptr++;\n",
            '+' => "(*ptr)++;\n",
            '-' => "(*ptr)--;\n",
            '.' => "putchar(*ptr);\n",
            ',' => "*ptr = getchar();\n",
            '[' => "while (*ptr) {\n",
            ']' => "}\n",
            else => "",
        });
    }
    _ = try writer.write("}");
    const state = tcc.tcc_new();
    defer tcc.tcc_delete(state);

    const lib_path = try std.fs.cwd().realpathAlloc(alloc, "./tinycc");
    const lib_path_c = @ptrCast([*c]const u8, try std.cstr.addNullByte(alloc, lib_path));
    tcc.tcc_set_lib_path(state, lib_path_c);

    if (tcc.tcc_set_output_type(state, tcc.TCC_OUTPUT_MEMORY) < 0) {
        return TccError.InitializationError;
    }
    const c_code = @ptrCast([*c]const u8, try buf.toOwnedSliceSentinel(0));
    if (tcc.tcc_compile_string(state, c_code) != 0) {
        return TccError.CompilationError;
    }
    if (tcc.tcc_relocate(state, tcc.TCC_RELOCATE_AUTO) < 0) {
        return TccError.RelocationError;
    }
    const bf = tcc.tcc_get_symbol(state, "bf");
    if (bf == null) {
        return TccError.UndefinedSymbolError;
    }
    @ptrCast(*const fn () void, bf)();
}

pub fn main() !void {
    const alloc = std.heap.page_allocator;
    const args = try std.process.argsAlloc(alloc);
    defer std.process.argsFree(alloc, args);
    var file = try std.fs.cwd().openFile(args[1], .{});
    defer file.close();
    const max_size = std.math.maxInt(u32);
    const bf_code = try file.reader().readAllAlloc(alloc, max_size);
    try bfc(alloc, bf_code);
}
