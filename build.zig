const std = @import("std");

pub fn build(b: *std.build.Builder) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{ .name = "chat", .target = target, .optimize = optimize });
    exe.addIncludePath(".");
    exe.addCSourceFile("ggml.c", &.{ "-std=c11", "-D_POSIX_C_SOURCE=199309L", "-pthread" });
    exe.addCSourceFiles(&.{ "utils.cpp", "chat.cpp" }, &.{"-std=c++11"});
    exe.linkLibC();
    exe.linkLibCpp();
    exe.install();
}
