const std = @import("std");
const dbg = std.debug.print;

const wasm_shelf = @import("wasm_shelf");
const StackValue = wasm_shelf.StackValue;
const Instance = wasm_shelf.Instance;

pub export fn ts_wasm_load(data: [*]u8, len: usize, lang_name: [*:0]u8) callconv(.c) *anyopaque {
    const mod_data = data[0..len];
    return wasm_load(mod_data, std.mem.span(lang_name)) catch @panic("TODO: error handling");
}

pub export fn ts_wasm_get_lang_mem(lang: *WASMLanguage, off: *u32) callconv(.c) ?[*]u8 {
    off.* = lang.lang_in_mem;
    // TODO: bluff size check
    return (lang.in.mem_get_bytes(0, lang.lang_in_mem) catch return null).ptr;
}

pub export fn ts_wasm_reset_heap(lang: *WASMLanguage, serialize_buffer_size: usize) callconv(.c) void {
    lang.heap_start = @intCast(lang.dylink_mem_size + serialize_buffer_size);
    lang.heap_pos = lang.heap_start;
}

pub export fn ts_wasm_serialize_buffer(lang: *WASMLanguage) callconv(.c) u32 {
    return lang.dylink_mem_size;
}

pub export fn ts_wasm_call_tbl_func(lang: *WASMLanguage, table_idx: u32, n_res: c_int, n_arg: c_int, arg1: u32, arg2: u32, arg3: u32) callconv(.c) u32 {
    return wasm_call_tbl_func(lang, table_idx, n_res, n_arg, arg1, arg2, arg3) catch @panic("TODO: error handling");
}

fn trap(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    _ = args_ret;
    _ = in;
    const str: [*:0]u8 = @ptrCast(data);
    std.debug.print("UNIMPLEMENTED: {s}\n", .{std.mem.span(str)});
    return error.WASMTrap;
}

fn wasm_heap_alloc(lang: *WASMLanguage, size: u32) !u32 {
    const start = lang.heap_pos;
    lang.heap_pos += size;
    lang.heap_pos += (lang.heap_pos + 1) & 3 - 1; // align 4 because why not
    if (lang.heap_pos > lang.in.mem.items.len) {
        @panic("not implemented");
    }
    std.debug.print("ALLOC at {} w size {}", .{ start, size });
    return start;
}

fn cb_calloc(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    _ = in;
    const lang: *WASMLanguage = @alignCast(@ptrCast(data));

    const size = args_ret[0].u32() * args_ret[1].u32();
    args_ret[0].i32 = @bitCast(try wasm_heap_alloc(lang, size));
}

const WASMLanguage = struct {
    stack_pointer: StackValue = .{ .i32 = 0 },
    memory_base: StackValue = .{ .i32 = 0 },
    table_base: StackValue = .{ .i32 = 0 },
    mod: wasm_shelf.Module,
    in: Instance = undefined,
    lang_in_mem: u32 = undefined,
    dylink_mem_size: u32 = 0,
    heap_start: u32 = 0,
    heap_pos: u32 = 0,
};

fn bulll(aa: anytype) *anyopaque {
    return @constCast(@ptrCast(aa));
}

fn wasm_load(data: []u8, langname: []u8) !*WASMLanguage {
    const allocator = std.heap.c_allocator;

    const lang = try allocator.create(WASMLanguage);
    lang.* = .{ .mod = try .parse(data, allocator) };

    // defer mod.deinit();
    const mod = &lang.mod;

    var imports: wasm_shelf.ImportTable = .init(allocator);
    defer imports.deinit(); // module must not point to mem in "imports"

    try imports.add_global("__stack_pointer", &lang.stack_pointer, .i32);
    try imports.add_global("__memory_base", &lang.memory_base, .i32);
    try imports.add_global("__table_base", &lang.table_base, .i32);
    try imports.add_func("calloc", .{ .cb = &cb_calloc, .data = @ptrCast(lang), .n_args = 2, .n_res = 1 });
    try imports.add_func("towupper", .{ .cb = &trap, .data = bulll("towupper"), .n_args = 1, .n_res = 1 });
    try imports.add_func("iswspace", .{ .cb = &trap, .data = bulll("iswspace"), .n_args = 1, .n_res = 1 });
    try imports.add_func("strlen", .{ .cb = &trap, .data = bulll("strlen"), .n_args = 1, .n_res = 1 });
    try imports.add_func("memcmp", .{ .cb = &trap, .data = bulll("memcmp"), .n_args = 3, .n_res = 1 });
    try imports.add_func("free", .{ .cb = &trap, .data = bulll("free"), .n_args = 1, .n_res = 0 });
    try imports.add_func("realloc", .{ .cb = &trap, .data = bulll("realloc"), .n_args = 2, .n_res = 1 });
    try imports.add_func("malloc", .{ .cb = &trap, .data = bulll("malloc"), .n_args = 1, .n_res = 1 });
    try imports.add_func("__assert_fail", .{ .cb = &trap, .data = bulll("__assert_fail"), .n_args = 4, .n_res = 0 });
    try imports.add_func("strncpy", .{ .cb = &trap, .data = bulll("strncpy"), .n_args = 3, .n_res = 1 });
    try imports.add_func("iswalnum", .{ .cb = &trap, .data = bulll("iswaifu"), .n_args = 1, .n_res = 1 });

    if (try mod.get_dylink_info()) |info| {
        dbg("DYLING: {}\n", .{info});
        const pages = (info.memory_size + wasm_shelf.page_size - 1) / wasm_shelf.page_size + 1;
        imports.memory_size = pages;
        imports.func_table_size = info.table_size;
        lang.dylink_mem_size = info.memory_size;
    }

    lang.in = try .init(mod, &imports);
    const in = &lang.in;
    // defer in.deinit();

    // TODO: check how the order is really defined, this is just guesswork on the major scale
    const initializers = &[_][]const u8{ "__wasm_call_ctors", "__wasm_apply_data_relocs", "_initialize" };

    for (initializers) |init| {
        if (try mod.lookup_export(init)) |sym| {
            dbg("INIT: {s}\n", .{init});
            if (sym.kind != .func) @panic("nej");
            _ = try in.execute(sym.idx, &.{}, &.{}, true);
        }
    }

    var lang_func_name: std.ArrayList(u8) = .init(allocator);
    try std.fmt.format(lang_func_name.writer(), "tree_sitter_{s}", .{langname});
    const sym = try mod.lookup_export(lang_func_name.items) orelse @panic("no such lang");
    if (sym.kind != .func) @panic("nej");
    var res: [1]StackValue = undefined;
    _ = try in.execute(sym.idx, &.{}, &res, true);

    dbg("HERE IS THE RESULT: {}\n", .{res[0].i32});
    lang.lang_in_mem = res[0].u32();

    try mod.dbg_imports();
    try mod.dbg_exports();

    return lang;
}

fn wasm_call_tbl_func(lang: *WASMLanguage, table_idx: u32, n_res: c_int, n_arg: c_int, arg1: u32, arg2: u32, arg3: u32) !u32 {
    const func_idx = lang.in.funcref_table[table_idx];
    const args: [3]StackValue = .{ .{ .i32 = @bitCast(arg1) }, .{ .i32 = @bitCast(arg2) }, .{ .i32 = @bitCast(arg3) } };
    var res: [1]StackValue = .{.{ .i32 = 0 }};
    _ = try lang.in.execute(func_idx, args[0..@intCast(n_arg)], res[0..@intCast(n_res)], true);
    return res[0].u32();
}
