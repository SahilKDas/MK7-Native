const std = @import("std");

pub fn main(init: std.process.Init) !void {
    const allocator = init.arena.allocator();
    const io = init.io;
    const args = try init.minimal.args.toSlice(allocator);
    if (args.len != 4) return error.Usage;

    const map_bytes = try std.Io.Dir.cwd().readFileAlloc(io, args[1], allocator, .limited(64 * 1024 * 1024));
    const code_size = (try std.Io.Dir.cwd().statFile(io, args[2], .{})).size;
    const permille = try std.fmt.parseInt(u64, args[3], 10);
    if (permille == 0 or permille > 1000) return error.InvalidCoveragePermille;

    var functions = std.AutoHashMap(u32, void).init(allocator);
    defer functions.deinit();
    var instructions = std.AutoHashMap(u32, void).init(allocator);
    defer instructions.deinit();

    var lines = std.mem.splitScalar(u8, map_bytes, '\n');
    const header = std.mem.trimEnd(u8, lines.next() orelse return error.EmptyMap, "\r");
    if (!std.mem.eql(u8, header, "function_address,instruction_address,raw,operation,supported")) return error.InvalidHeader;

    var rows: u64 = 0;
    while (lines.next()) |raw_line| {
        const line = std.mem.trim(u8, raw_line, " \t\r");
        if (line.len == 0) continue;
        var fields = std.mem.splitScalar(u8, line, ',');
        const function_text = fields.next() orelse return error.InvalidRow;
        const instruction_text = fields.next() orelse return error.InvalidRow;
        _ = fields.next() orelse return error.InvalidRow;
        _ = fields.next() orelse return error.InvalidRow;
        const supported = fields.next() orelse return error.InvalidRow;
        if (fields.next() != null) return error.InvalidRow;
        if (!std.mem.eql(u8, supported, "true")) return error.UnsupportedInstruction;
        const function_address = try parseHex(function_text);
        const instruction_address = try parseHex(instruction_text);
        try functions.put(function_address, {});
        const result = try instructions.getOrPut(instruction_address);
        if (result.found_existing) return error.DuplicateInstruction;
        rows += 1;
    }

    const mapped_bytes = rows * 4;
    const required_bytes = (code_size * permille + 999) / 1000;
    if (mapped_bytes < required_bytes) return error.CoverageBelowTarget;
    if (functions.count() == 0) return error.NoFunctions;
    const percent_x10000 = (mapped_bytes * 1_000_000 + code_size / 2) / code_size;
    std.debug.print("mk7-map-audit: {d} bytes, {d} instructions, {d} functions ({d}.{d:0>4}% >= {d}.{d}% target)\n", .{
        mapped_bytes,            rows,                    functions.count(),
        percent_x10000 / 10_000, percent_x10000 % 10_000, permille / 10,
        permille % 10,
    });
}

fn parseHex(text: []const u8) !u32 {
    if (text.len < 3 or text[0] != '0' or (text[1] != 'x' and text[1] != 'X')) return error.InvalidAddress;
    return std.fmt.parseInt(u32, text[2..], 16);
}
