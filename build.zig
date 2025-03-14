const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const wasm = b.option(bool, "enable-wasm", "Enable Wasm support") orelse false;
    const neowasm = b.option(bool, "enable-neowasm", "Enable neoWasm support") orelse false;
    const neowasm2 = b.option(bool, "enable-neowasm2", "Enable neoWasm2 support") orelse false;
    const shared = b.option(bool, "build-shared", "Build a shared library") orelse false;
    const amalgamated = b.option(bool, "amalgamated", "Build using an amalgamated source") orelse false;

    if (neowasm and amalgamated) @panic("fail");

    const lib: *std.Build.Step.Compile = if (!shared) b.addStaticLibrary(.{
        .name = "tree-sitter",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    }) else b.addSharedLibrary(.{
        .name = "tree-sitter",
        .pic = true,
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    if (amalgamated) {
        lib.addCSourceFile(.{
            .file = b.path("lib/src/lib.c"),
            .flags = &.{"-std=c11"},
        });
    } else {
        lib.addCSourceFiles(.{
            .root = b.path("lib/src"),
            .files = try findSourceFiles(b, neowasm, neowasm2),
            .flags = &.{"-std=c11"},
        });
    }

    lib.addIncludePath(b.path("lib/include"));
    lib.addIncludePath(b.path("lib/src"));
    lib.addIncludePath(b.path("lib/src/wasm"));

    lib.root_module.addCMacro("_POSIX_C_SOURCE", "200112L");
    lib.root_module.addCMacro("_DEFAULT_SOURCE", "");

    if (wasm) {
        if (b.lazyDependency(wasmtimeDep(target.result), .{})) |wasmtime| {
            lib.root_module.addCMacro("TREE_SITTER_FEATURE_WASM", "");
            lib.addSystemIncludePath(wasmtime.path("include"));
            lib.addLibraryPath(wasmtime.path("lib"));
            lib.linkSystemLibrary("wasmtime");
        }
    }

    if (neowasm) {
        if (b.lazyDependency("wasm_anon", .{ .target = target, .optimize = optimize })) |wasm_anon| {
            const wasm_shelf = wasm_anon.module("wasm_shelf");
            lib.root_module.addImport("wasm_shelf", wasm_shelf);
            lib.root_module.root_source_file = b.path("lib/src/wasm_bindings.zig");
        }
    }

    lib.installHeadersDirectory(b.path("lib/include"), ".", .{});

    b.installArtifact(lib);

    // THIS IS JUST A TEST
    if (neowasm or neowasm2) {
        const exe = b.addExecutable(.{
            .name = "neowasm_test",
            .target = target,
            .optimize = optimize,
        });
        exe.root_module.addCSourceFile(.{ .file = b.path("./neo_wasm_test.c"), .flags = &.{"-std=c11"} });
        exe.root_module.linkLibrary(lib);
        b.installArtifact(exe);
        if (neowasm2) {
            exe.root_module.addObjectFile(b.path("../wasm-micro-runtime/build/libiwasm.a"));
        }

        const run_cmd = b.addRunArtifact(exe);
        run_cmd.step.dependOn(b.getInstallStep());
        if (b.args) |args| {
            run_cmd.addArgs(args);
        }
        const run_step = b.step("neorun", "fuck it we ball");
        run_step.dependOn(&run_cmd.step);
    }
}

fn wasmtimeDep(target: std.Target) []const u8 {
    const arch = target.cpu.arch;
    const os = target.os.tag;
    const abi = target.abi;
    return switch (os) {
        .linux => switch (arch) {
            .x86_64 => switch (abi) {
                .gnu => "wasmtime_c_api_x86_64_linux",
                .musl => "wasmtime_c_api_x86_64_musl",
                .android => "wasmtime_c_api_x86_64_android",
                else => null,
            },
            .aarch64 => switch (abi) {
                .gnu => "wasmtime_c_api_aarch64_linux",
                .android => "wasmtime_c_api_aarch64_android",
                else => null,
            },
            .s390x => "wasmtime_c_api_s390x_linux",
            .riscv64 => "wasmtime_c_api_riscv64gc_linux",
            else => null,
        },
        .windows => switch (arch) {
            .x86_64 => switch (abi) {
                .gnu => "wasmtime_c_api_x86_64_mingw",
                .msvc => "wasmtime_c_api_x86_64_windows",
                else => null,
            },
            else => null,
        },
        .macos => switch (arch) {
            .x86_64 => "wasmtime_c_api_x86_64_macos",
            .aarch64 => "wasmtime_c_api_aarch64_macos",
            else => null,
        },
        else => null,
    } orelse std.debug.panic("Unsupported target for wasmtime: {s}-{s}-{s}", .{ @tagName(arch), @tagName(os), @tagName(abi) });
}

fn findSourceFiles(b: *std.Build, neowasm: bool, neowasm2: bool) ![]const []const u8 {
    var sources = std.ArrayList([]const u8).init(b.allocator);

    var dir = try b.build_root.handle.openDir("lib/src", .{ .iterate = true });
    var iter = dir.iterate();
    defer dir.close();

    while (try iter.next()) |entry| {
        if (entry.kind != .file) continue;
        const file = entry.name;
        const ext = std.fs.path.extension(file);
        if (std.mem.eql(u8, ext, ".c") and !std.mem.eql(u8, file, "lib.c") and !std.mem.startsWith(u8, file, "wasm_store")) {
            try sources.append(b.dupe(file));
        }
    }

    if (neowasm2) {
        try sources.append("wasm_store_capi.c");
    } else if (neowasm) {
        try sources.append("wasm_store_neo.c");
    } else {
        try sources.append("wasm_store.c");
    }

    return sources.items;
}
