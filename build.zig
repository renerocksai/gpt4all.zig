const std = @import("std");

// thanks to github.com/mattnite !!!
const libcurl = @import("src/zig-libcurl/libcurl.zig");
const mbedtls = @import("src/zig-libcurl/zig-mbedtls/mbedtls.zig");
const zlib = @import("src/zig-libcurl/zig-zlib/zlib.zig");
const libssh2 = @import("src/zig-libcurl/zig-libssh2/libssh2.zig");

pub fn build(b: *std.build.Builder) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // thanks to github.com/mattnite !!!
    const z = zlib.create(b, target, optimize);
    const tls = mbedtls.create(b, target, optimize);
    const ssh2 = libssh2.create(b, target, optimize);
    tls.link(ssh2.step);

    const curl = try libcurl.create(b, target, optimize);
    ssh2.link(curl.step);
    tls.link(curl.step);
    z.link(curl.step, .{});

    const exe = b.addExecutable(.{
        .name = "chat",
        // we added zig code to download models
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    // now, here comes the C and C++ stuff for the actual chat client
    exe.addIncludePath("src/llm");
    exe.addCSourceFile("src/llm/ggml.c", &.{ "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-pthread" });
    exe.addCSourceFiles(&.{ "src/llm/utils.cpp", "src/llm/chat.cpp" }, &.{"-std=c++11"});
    curl.link(exe, .{});
    exe.linkLibC();
    exe.linkLibCpp();
    if (exe.target.isWindows()) {
        exe.want_lto = false;
    }
    exe.install();
}
