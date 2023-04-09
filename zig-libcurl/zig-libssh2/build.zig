const std = @import("std");
const libssh2 = @import("libssh2.zig");
const mbedtls = @import("zig-mbedtls/mbedtls.zig");

pub fn build(b: *std.build.Builder) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const tls = mbedtls.create(b, target, optimize);
    const ssh2 = libssh2.create(b, target, optimize);
    tls.link(ssh2.step);
    ssh2.step.install();

    const test_step = b.step("test", "fake test step for now");
    _ = test_step;
}
