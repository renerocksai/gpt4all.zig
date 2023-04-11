const std = @import("std");
const download = @import("download.zig");

pub extern fn cpp_main(argc: c_int, argv: [*][*c]const u8) c_int;

const DEFAULT_MODEL_URL = "https://the-eye.eu/public/AI/models/nomic-ai/gpt4all/gpt4all-lora-quantized.bin";
const DEFAULT_MODEL = "gpt4all-lora-quantized.bin";

pub fn main() !u8 {
    const allocator = std.heap.page_allocator;
    var args_it = try std.process.argsWithAllocator(allocator);

    var i: c_int = 0;
    var argv = std.ArrayList([*c]const u8).init(allocator);

    var model_url_option: ?[]const u8 = null;
    var model_filename: ?[]const u8 = null;

    var seen_minus_m = false;
    while (args_it.next()) |arg| {
        if (seen_minus_m) {
            model_filename = arg;
            seen_minus_m = false;
        }
        if (std.mem.eql(u8, arg, "-m")) {
            seen_minus_m = true;
        }
        if (std.mem.eql(u8, arg, "-u")) {
            const next = args_it.next();
            if (next) |url| {
                model_url_option = url;
            }
        } else {
            try argv.append(arg);
            i += 1;
        }
    }
    const download_url = model_url_option orelse DEFAULT_MODEL_URL;
    const model_fn = model_filename orelse DEFAULT_MODEL;

    var do_download_model = false;
    if (std.fs.cwd().statFile(model_fn)) |stat| {
        // instead of checking MD5 sum which might change, we check the file
        // size, roughly
        if (stat.size < 4 * 1000 * 1000 * 1000) {
            std.debug.print(
                "Model File size {} does not seem right, downloading from...: {s}",
                .{ stat.size, download_url },
            );
            do_download_model = true;
        } else {
            std.debug.print("Model File size seems ok: {}\n", .{stat.size});
        }
    } else |_| {
        do_download_model = true;
        std.debug.print(
            "Model File does not exist, downloading model from: {s}\n",
            .{download_url},
        );
    }

    if (do_download_model) {
        if (download.download_to_file(download_url, model_fn)) |_| {} else |err| {
            std.debug.print("Diagnostics: {any}\n", .{err});
        }
    }

    _ = cpp_main(i, argv.items.ptr);
    return 0;
}
