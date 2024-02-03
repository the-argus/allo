const std = @import("std");
const builtin = @import("builtin");
const zcc = @import("compile_commands");

const release_flags = &[_][]const u8{
    "-DNDEBUG",
    "-std=c++17",
    "-DALLO_LOGGING",
    "-DALLO_ALLOC_RESULT_CHECKED",
    "-DALLO_STACK_ALLOCATOR_USE_CTTI",
};

const debug_flags = &[_][]const u8{
    "-g",
    "-std=c++17",
    "-DALLO_LOGGING",
    "-DALLO_ALLOC_RESULT_CHECKED",
    "-DALLO_STACK_ALLOCATOR_USE_CTTI",
};

const testing_flags = &[_][]const u8{
    "-DFMT_EXCEPTIONS=1",
    "-DTESTING",
    "-DTESTING_NOEXCEPT=",
    "-DTESTING_THELIB_OPT_T_NO_NOTHROW",
    "-DTESTING_THELIB_RESULT_T_NO_NOTHROW",
    "-DTESTING_ALLO_STACK_ALLOCATOR_T_NO_NOTHROW",
};

const non_testing_flags = &[_][]const u8{
    "-DTESTING_NOEXCEPT=noexcept",
    "-DFMT_EXCEPTIONS=0",
    "-fno-exceptions",
    "-fno-rtti",
};

const cpp_sources = &[_][]const u8{
    "src/random_allocation_registry.cpp",
    "src/stack_allocator.cpp",
};

const public_include_dirs = &[_][]const u8{
    "include/",
};

const private_include_dirs = &[_][]const u8{
    "src/include/",
    "include/allo/",
};

const test_source_files = &[_][]const u8{
    "stack_allocator_t/stack_allocator_t.cpp",
    "pool_allocator_generational_t/pool_allocator_generational_t.cpp",
};

pub fn build(b: *std.Build) !void {
    // options
    const target = b.standardTargetOptions(.{});
    const mode = b.standardOptimizeOption(.{});

    var flags = std.ArrayList([]const u8).init(b.allocator);
    defer flags.deinit();
    try flags.appendSlice(if (mode == .Debug) debug_flags else release_flags);

    var targets = std.ArrayList(*std.Build.Step.Compile).init(b.allocator);
    defer targets.deinit();
    var tests = std.ArrayList(*std.Build.Step.Compile).init(b.allocator);
    defer tests.deinit();

    var lib: *std.Build.CompileStep =
        b.addStaticLibrary(.{
        .name = "allo",
        .optimize = mode,
        .target = target,
    });
    try targets.append(lib);
    b.installArtifact(lib);

    lib.linkLibCpp();

    for (private_include_dirs) |include_dir| {
        try flags.append(b.fmt("-I{s}", .{include_dir}));
    }

    var tests_lib = b.addSharedLibrary(.{
        .name = "main",
        .target = target,
        .optimize = mode,
    });
    tests_lib.linkLibCpp();

    const flags_owned = flags.toOwnedSlice() catch @panic("OOM");
    const all_sources_owned = cpp_sources;
    lib.addCSourceFiles(all_sources_owned, flags_owned);
    tests_lib.addCSourceFiles(all_sources_owned, flags_owned);
    // set up tests (executables which dont link artefacts built from
    // all_sources_owned but they do need flags, so we do it in this
    // scope so we can have flags_owned)
    for (test_source_files) |source_file| {
        var test_exe = b.addExecutable(.{
            .name = std.fs.path.stem(source_file),
            .optimize = mode,
            .target = target,
        });
        test_exe.addCSourceFile(.{
            .file = .{ .path = b.pathJoin(&.{ "tests", source_file }) },
            .flags = flags_owned,
        });
        test_exe.addIncludePath(.{ .path = "tests/" });
        test_exe.linkLibCpp();
        test_exe.linkLibCpp(tests_lib);
        try tests.append(test_exe);
    }

    // make step that runs all of the tests
    // TODO: add some custom step that just checks to see if we're in testing mode or not
    const run_tests_step = b.step("run_tests", "Compile and run all the tests");
    const install_tests_step = b.step("install_tests", "Install all the tests but don't run them");
    for (tests.items) |test_exe| {
        const test_install = b.addInstallArtifact(test_exe, .{});
        install_tests_step.dependOn(&test_install.step);

        test_exe.linkLibrary(tests_lib);
        const test_run = b.addRunArtifact(test_exe);
        if (b.args) |args| {
            test_run.addArgs(args);
        }
        run_tests_step.dependOn(&test_run.step);
    }

    targets.appendSlice(tests.toOwnedSlice() catch @panic("OOM")) catch @panic("OOM");

    zcc.createStep(b, "cdb", try targets.toOwnedSlice());
}
