const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addLibrary(.{
        .linkage = .static,
        .name = "rocket68",
        .root_module = lib_mod,
    });

    lib.addIncludePath(b.path("include"));
    lib.linkLibC();

    const c_flags = &.{
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-pedantic",
    };

    const src_files = &.{
        "src/disasm.c",
        "src/loader.c",
        "src/m68k/m68k.c",
        "src/m68k/ops_arith.c",
        "src/m68k/ops_bit.c",
        "src/m68k/ops_control.c",
        "src/m68k/ops_logic.c",
        "src/m68k/ops_move.c",
    };

    lib.addCSourceFiles(.{
        .files = src_files,
        .flags = c_flags,
    });

    b.installArtifact(lib);

    const test_step = b.step("test", "Run tests");
    const test_dir = std.fs.cwd().openDir("test", .{}) catch null;
    if (test_dir != null) {
        const test_files = &.{
            "test/test_addressing.c",
            "test/test_arith.c",
            "test/test_control.c",
            "test/test_core.c",
            "test/test_integration.c",
            "test/test_loader.c",
            "test/test_logic.c",
            "test/test_main.c",
            "test/test_move.c",
            "test/test_regression.c",
        };

        const test_mod = b.createModule(.{
            .target = target,
            .optimize = optimize,
        });

        const test_exe = b.addExecutable(.{
            .name = "test_main",
            .root_module = test_mod,
        });
        test_exe.addIncludePath(b.path("include"));
        test_exe.linkLibC();

        test_exe.addCSourceFiles(.{
            .files = src_files,
            .flags = c_flags,
        });
        test_exe.addCSourceFiles(.{
            .files = test_files,
            .flags = c_flags,
        });

        const run_test = b.addRunArtifact(test_exe);
        test_step.dependOn(&run_test.step);
    }

    // Add conditional benchmarks
    const bench_step = b.step("bench", "Run benchmarks");
    const bench_dir = std.fs.cwd().openDir("benches", .{}) catch null;
    if (bench_dir != null) {
        const bench_mod = b.createModule(.{
            .target = target,
            .optimize = optimize,
        });

        const bench_exe = b.addExecutable(.{
            .name = "benchmark_rocket68",
            .root_module = bench_mod,
        });
        bench_exe.addIncludePath(b.path("include"));
        bench_exe.linkLibC();
        bench_exe.linkLibrary(lib); // Link the static library

        const bench_files = &.{
            "benches/benchmark_rocket68.c",
        };

        bench_exe.addCSourceFiles(.{
            .files = bench_files,
            .flags = c_flags,
        });

        const run_bench = b.addRunArtifact(bench_exe);
        bench_step.dependOn(&run_bench.step);

        // Also build Musashi benchmark for comparison if the submodule is checked out
        const musashi_dir = std.fs.cwd().openDir("external/musashi", .{}) catch null;
        if (musashi_dir != null) {
            const make_musashi = b.addSystemCommand(&.{ "make", "-C", "external/musashi" });

            const musashi_mod = b.createModule(.{
                .target = target,
                .optimize = optimize,
            });

            const musashi_exe = b.addExecutable(.{
                .name = "benchmark_musashi",
                .root_module = musashi_mod,
            });
            musashi_exe.addIncludePath(b.path("external/musashi"));
            musashi_exe.linkLibC();
            musashi_exe.linkSystemLibrary("m"); // math library needed by Musashi

            musashi_exe.step.dependOn(&make_musashi.step);

            const musashi_files = &.{
                "benches/benchmark_musashi.c",
            };

            musashi_exe.addCSourceFiles(.{
                .files = musashi_files,
                .flags = c_flags,
            });
            musashi_exe.addObjectFile(b.path("external/musashi/m68kcpu.o"));
            musashi_exe.addObjectFile(b.path("external/musashi/m68kops.o"));
            musashi_exe.addObjectFile(b.path("external/musashi/m68kdasm.o"));
            musashi_exe.addObjectFile(b.path("external/musashi/softfloat/softfloat.o"));

            const run_musashi = b.addRunArtifact(musashi_exe);
            run_musashi.step.dependOn(&run_bench.step); // run Musashi after Rocket68
            bench_step.dependOn(&run_musashi.step);
        }
    }
}
