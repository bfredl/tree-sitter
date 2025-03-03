const std = @import("std");
const dbg = std.debug.print;

const wasm_shelf = @import("wasm_shelf");

pub export fn ts_wasm_load(data: [*]u8, len: usize) callconv(.c) void {
    const mod_data = data[0..len];
    wasm_load(mod_data) catch @panic("TODO: error handling");
}

fn wasm_load(data: []u8) !void {
    const allocator = std.heap.c_allocator;

    var mod = try wasm_shelf.Module.parse(data, allocator);
    defer mod.deinit();

    try mod.dbg_imports();
    try mod.dbg_exports();
}
