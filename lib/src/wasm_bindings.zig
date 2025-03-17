const std = @import("std");
const dbg = std.debug.print;

const wasm_shelf = @import("wasm_shelf");
const StackValue = wasm_shelf.StackValue;
const Instance = wasm_shelf.Instance;

// EXPORTS:
const SharedMemInfo = extern struct {
    lang_in_mem: u32,
    lexer_in_mem: u32,
    dylink_mem_start: u32,
    dylink_mem_size: u32,
};

pub export fn ts_wasm_load(data: [*]u8, len: usize, lang_name: [*:0]u8, lexer_size: usize, any: *anyopaque) callconv(.c) *anyopaque {
    const mod_data = data[0..len];
    return wasm_load(mod_data, std.mem.span(lang_name), lexer_size, any) catch @panic("TODO: error handling");
}

pub export fn ts_wasm_get_mem(lang: *WASMLanguage) callconv(.c) ?[*]u8 {
    return (lang.in.mem_get_bytes(0, lang.lang_in_mem) catch return null).ptr;
}

pub export fn ts_wasm_get_mem_info(lang: *WASMLanguage, meminfo: *SharedMemInfo) void {
    meminfo.* = .{ .lang_in_mem = lang.lang_in_mem, .lexer_in_mem = lang.lexer_in_mem, .dylink_mem_start = lang.dylink_mem_base, .dylink_mem_size = lang.dylink_mem_size };
}

pub export fn ts_wasm_reset_heap(lang: *WASMLanguage, serialize_buffer_size: usize) callconv(.c) void {
    lang.heap_start = @intCast(lang.current_memory_offset + serialize_buffer_size);
    lang.heap_pos = lang.heap_start;
}

pub export fn ts_wasm_serialize_buffer(lang: *WASMLanguage) callconv(.c) u32 {
    return lang.current_memory_offset;
}

pub export fn ts_wasm_call_tbl_func(lang: *WASMLanguage, table_idx: u32, n_res: c_int, n_arg: c_int, arg1: u32, arg2: u32, arg3: u32) callconv(.c) u32 {
    return wasm_call_tbl_func(lang, table_idx, n_res, n_arg, arg1, arg2, arg3) catch @panic("TODO: error handling");
}

// IMPORTS:
pub extern fn ts_wasm_lexer_cb(data: *anyopaque, idx: u32, param_1: u32) u32;

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
    // std.debug.print("ALLOC at {} w size {}\n", .{ start, size });
    return start;
}

fn cb_calloc(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    const lang: *WASMLanguage = @alignCast(@ptrCast(data));

    // TODO: when recycling memory, need to memset it zero
    const size = args_ret[0].u32() * args_ret[1].u32();
    const mem = try wasm_heap_alloc(lang, size);
    @memset(in.mem.items[mem..][0..size], 0);
    args_ret[0].i32 = @bitCast(mem);
}

fn cb_malloc(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    _ = in;
    const lang: *WASMLanguage = @alignCast(@ptrCast(data));

    const size = args_ret[0].u32();
    args_ret[0].i32 = @bitCast(try wasm_heap_alloc(lang, size));
}

fn cb_free(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    _ = in;
    _ = args_ret;
    _ = data;
}

fn cb_lexer(comptime idx: u32) *const fn ([]StackValue, *Instance, *anyopaque) error{WASMTrap}!void {
    return &struct {
        fn cb(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
            _ = in;
            const res = ts_wasm_lexer_cb(data, idx, if (args_ret.len >= 2) args_ret[1].u32() else 0);
            args_ret[0] = .{ .i32 = @bitCast(res) };
        }
    }.cb;
}

extern fn iswspace(c_int) c_int;
fn cb_iswspace(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    _ = in;
    _ = data;

    const cp = args_ret[0].i32;
    args_ret[0].i32 = iswspace(cp);
}

extern fn iswalnum(c_int) c_int;
fn cb_iswaifu(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    _ = in;
    _ = data;

    const cp = args_ret[0].i32;
    args_ret[0].i32 = iswalnum(cp);
}

extern fn towupper(c_int) c_int;
fn cb_towupper(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    _ = in;
    _ = data;

    const cp = args_ret[0].i32;
    args_ret[0].i32 = towupper(cp);
}

fn cb_strlen(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    _ = data;

    const ptr = args_ret[0].u32();
    if (ptr >= in.mem.items.len) return error.WASMTrap;
    const len = std.mem.indexOfScalar(u8, in.mem.items[ptr..], 0) orelse return error.WASMTrap;
    args_ret[0].i32 = @bitCast(@as(u32, @intCast(len)));
}

fn cb_memcmp(args_ret: []StackValue, in: *Instance, data: *anyopaque) !void {
    _ = data;

    const m = in.mem.items;
    const ptr1 = args_ret[0].u32();
    const ptr2 = args_ret[1].u32();
    const len = args_ret[2].u32();
    for (ptr1..ptr1 + len, ptr2..ptr2 + len) |p1, p2| {
        if (p1 >= m.len or p2 >= m.len) return error.WASMTrap;
        const cmp: i32 = @as(i32, m[p1]) - @as(i32, m[p2]);
        if (cmp != 0) {
            args_ret[0].i32 = cmp;
            return;
        }
    }
    args_ret[0].i32 = 0;
    return;
}
const WASMLanguage = struct {
    stack_pointer: StackValue = .{ .i32 = 0 },
    memory_base: StackValue = .{ .i32 = 0 },
    table_base: StackValue = .{ .i32 = 0 },
    mod: wasm_shelf.Module,
    in: Instance = undefined,
    lang_in_mem: u32 = undefined,
    lexer_in_mem: u32 = undefined,
    dylink_mem_base: u32 = 0,
    dylink_mem_size: u32 = 0,
    current_memory_offset: u32 = 0,
    heap_start: u32 = 0,
    heap_pos: u32 = 0,
};

fn bulll(aa: anytype) *anyopaque {
    return @constCast(@ptrCast(aa));
}

fn wasm_load(data: []u8, langname: []u8, lexer_size: usize, any: *anyopaque) !*WASMLanguage {
    const allocator = std.heap.c_allocator;

    const lang = try allocator.create(WASMLanguage);
    lang.* = .{ .mod = try .parse(data, allocator) };

    // defer mod.deinit();
    const mod = &lang.mod;

    var imports: wasm_shelf.ImportTable = .init(allocator);
    defer imports.deinit(); // module must not point to mem in "imports"

    const n_table_funcs = 5;

    const lexer_in_mem = 2048; // this is arbitrary, but likely should not be zero
    const memory_base = lexer_in_mem + lexer_size;

    lang.dylink_mem_base = @intCast(memory_base);
    lang.memory_base = .{ .i32 = @intCast(memory_base) };
    lang.table_base = .{ .i32 = n_table_funcs };
    lang.stack_pointer = .{ .i32 = 2032 };

    try imports.add_global("__stack_pointer", &lang.stack_pointer, .i32);
    try imports.add_global("__memory_base", &lang.memory_base, .i32);
    try imports.add_global("__table_base", &lang.table_base, .i32);
    try imports.add_func("calloc", .{ .cb = &cb_calloc, .data = @ptrCast(lang), .n_args = 2, .n_res = 1 });
    try imports.add_func("towupper", .{ .cb = &cb_towupper, .data = bulll("towupper"), .n_args = 1, .n_res = 1 });
    try imports.add_func("iswspace", .{ .cb = &cb_iswspace, .data = bulll("iswspace"), .n_args = 1, .n_res = 1 });
    try imports.add_func("strlen", .{ .cb = &cb_strlen, .data = bulll("strlen"), .n_args = 1, .n_res = 1 });
    try imports.add_func("memcmp", .{ .cb = &cb_memcmp, .data = bulll("memcmp"), .n_args = 3, .n_res = 1 });
    try imports.add_func("free", .{ .cb = &cb_free, .data = @ptrCast(lang), .n_args = 1, .n_res = 0 });
    try imports.add_func("realloc", .{ .cb = &trap, .data = bulll("realloc"), .n_args = 2, .n_res = 1 });
    try imports.add_func("malloc", .{ .cb = &cb_malloc, .data = @ptrCast(lang), .n_args = 1, .n_res = 1 });
    try imports.add_func("__assert_fail", .{ .cb = &trap, .data = bulll("__assert_fail"), .n_args = 4, .n_res = 0 });
    try imports.add_func("strncpy", .{ .cb = &trap, .data = bulll("strncpy"), .n_args = 3, .n_res = 1 });
    try imports.add_func("iswalnum", .{ .cb = &cb_iswaifu, .data = bulll("iswaifu"), .n_args = 1, .n_res = 1 });

    imports.func_table_size = n_table_funcs;
    if (try mod.get_dylink_info()) |info| {
        const memsize = memory_base + info.memory_size + 4096; // arbitrary, but need a bit of heap (not much)
        const pages = (memsize + wasm_shelf.page_size - 1) / wasm_shelf.page_size + 1;
        imports.memory_size = @intCast(pages);
        imports.func_table_size += info.table_size;
        lang.dylink_mem_size = info.memory_size;
    }
    lang.current_memory_offset = @intCast(memory_base + lang.dylink_mem_size);

    _ = try imports.add_func_to_table(.{ .cb = cb_lexer(0), .data = any, .n_args = 2, .n_res = 0 });
    _ = try imports.add_func_to_table(.{ .cb = cb_lexer(1), .data = any, .n_args = 1, .n_res = 0 });
    _ = try imports.add_func_to_table(.{ .cb = cb_lexer(2), .data = any, .n_args = 1, .n_res = 1 });
    _ = try imports.add_func_to_table(.{ .cb = cb_lexer(3), .data = any, .n_args = 1, .n_res = 1 });
    _ = try imports.add_func_to_table(.{ .cb = cb_lexer(4), .data = any, .n_args = 1, .n_res = 1 });

    lang.in = try .init(mod, &imports);
    const in = &lang.in;
    // defer in.deinit();

    // TODO: check how the order is really defined, this is just guesswork on the major scale
    const initializers = &[_][]const u8{ "__wasm_call_ctors", "__wasm_apply_data_relocs", "_initialize" };

    for (initializers) |init| {
        if (try mod.lookup_export(init)) |sym| {
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

    lang.lang_in_mem = res[0].u32();
    lang.lexer_in_mem = lexer_in_mem;

    // try mod.dbg_imports();
    // try mod.dbg_exports();

    return lang;
}

fn wasm_call_tbl_func(lang: *WASMLanguage, table_idx: u32, n_res: c_int, n_arg: c_int, arg1: u32, arg2: u32, arg3: u32) !u32 {
    const func_idx = lang.in.funcref_table[table_idx];
    const args: [3]StackValue = .{ .{ .i32 = @bitCast(arg1) }, .{ .i32 = @bitCast(arg2) }, .{ .i32 = @bitCast(arg3) } };
    var res: [1]StackValue = .{.{ .i32 = 0 }};
    _ = try lang.in.execute(func_idx, args[0..@intCast(n_arg)], res[0..@intCast(n_res)], true);
    return res[0].u32();
}
