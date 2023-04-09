const std = @import("std");

const libcurl = @import("libcurl.zig");
const mbedtls = @import("zig-mbedtls/mbedtls.zig");
const zlib = @import("zig-zlib/zlib.zig");
const libssh2 = @import("zig-libssh2/libssh2.zig");

pub fn build(b: *std.build.Builder) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const z = zlib.create(b, target, optimize);
    const tls = mbedtls.create(b, target, optimize);
    const ssh2 = libssh2.create(b, target, optimize);
    tls.link(ssh2.step);

    const curl = try libcurl.create(b, target, optimize);
    ssh2.link(curl.step);
    tls.link(curl.step);
    z.link(curl.step, .{});
    curl.step.install();

    const tests = b.addTest(.{
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });
    curl.link(tests, .{});
    z.link(tests, .{});
    tls.link(tests);
    ssh2.link(tests);

    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&tests.step);

    const exe = b.addExecutable(.{
        .name = "my-program",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });
    curl.link(exe, .{});
    exe.install();
}
