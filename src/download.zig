const std = @import("std");
const curl = @import("curl.zig");

/// download url -> output_fn
/// first read into a fifo and only on completion, write everything to file
pub fn download_to_file(url: []const u8, output_fn: []const u8) !void {
    const Fifo = std.fifo.LinearFifo(u8, .{ .Dynamic = {} });

    try curl.globalInit();
    defer curl.globalCleanup();

    const allocator = std.heap.page_allocator;

    var fifo = Fifo.init(allocator);
    defer fifo.deinit();

    var easy = try curl.Easy.init();
    defer easy.cleanup();

    const c_url = try allocator.dupeZ(u8, url);
    defer allocator.free(c_url);

    try easy.setUrl(c_url);
    try easy.setSslVerifyPeer(false);
    try easy.setAcceptEncodingGzip();
    try easy.setWriteFn(curl.writeToFifo(Fifo));
    try easy.setNoProgress(false);
    try easy.setXferInfoFn(on_xfer_info);
    try easy.setWriteData(&fifo);
    try easy.setVerbose(false);
    try easy.perform();
    const code = try easy.getResponseCode();
    std.debug.print("\rDownload result code (200 is OK): {}                                 \n", .{code});

    if (code == 200) {
        std.debug.print("Writing file to: {s}... ", .{output_fn});
        errdefer std.debug.print("Download error!                                           \n", .{});

        // write to output file
        const file = try std.fs.cwd().createFile(output_fn, .{});
        defer file.close();

        var buffered_writer = std.io.bufferedWriter(file.writer());
        var output_writer = buffered_writer.writer();
        try output_writer.writeAll(fifo.readableSlice(0));
        try buffered_writer.flush();
        std.debug.print("\rDownload finished!                                               \n", .{});
    }
}

// /* This is the CURLOPT_XFERINFOFUNCTION callback prototype. It was introduced
//    in 7.32.0, avoids the use of floating point numbers and provides more
//    detailed information. */
// pub fn int (*curl_xferinfo_callback)(void *clientp,
//                                       curl_off_t dltotal,
//                                       curl_off_t dlnow,
//                                       curl_off_t ultotal,
//                                       curl_off_t ulnow);
pub fn on_xfer_info(
    clientp: ?*anyopaque,
    dltotal: curl.Offset,
    dlnow: curl.Offset,
    ultotal: curl.Offset,
    ulnow: curl.Offset,
) callconv(.C) c_int {
    _ = ulnow;
    _ = ultotal;
    _ = clientp;
    const percent: c_ulong = @intCast(c_ulong, @divTrunc(100 * dlnow, dltotal + 1));
    const dltotal_mb: c_ulong = @intCast(c_ulong, @divTrunc(@divTrunc(dltotal, 1024), 1024));
    const dlnow_mb: c_ulong = @intCast(c_ulong, @divTrunc(@divTrunc(dlnow, 1024), 1024));

    std.debug.print("\rDownloaded: {: >4} MB / {: >4} MB   [{: >3}%]                     ", .{ dlnow_mb, dltotal_mb, percent });
    return 0;
}
