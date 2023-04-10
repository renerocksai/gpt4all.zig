const std = @import("std");
const mbedtls = @import("mbedtls.zig");

pub fn build(b: *std.build.Builder) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = mbedtls.create(b, target, optimize);
    lib.step.install();

    const selftest = b.addExecutable(.{.name = "selftest"});
    selftest.addCSourceFile("mbedtls/programs/test/selftest.c", &.{});
    selftest.defineCMacro("MBEDTLS_SELF_TEST", null);
    lib.link(selftest);

    const run_selftest = selftest.run();
    run_selftest.step.dependOn(&selftest.step);
    const test_step = b.step("test", "Run mbedtls selftest");
    test_step.dependOn(&run_selftest.step);
}
