const std = @import("std");

fn rootPathExists(b: *std.Build, rel_path: []const u8) bool {
    std.fs.accessAbsolute(b.pathFromRoot(rel_path), .{}) catch return false;
    return true;
}

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

    // --- WASM build step ---
    const wasm_step = b.step("wasm", "Build Rocket 68 as a WASM library (wasm32-wasi)");

    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .cpu_features_add = std.Target.wasm.featureSet(&.{.exception_handling}),
    });

    const wasm_mod = b.createModule(.{
        .target = wasm_target,
        .optimize = optimize,
    });

    const wasm_lib = b.addLibrary(.{
        .linkage = .static,
        .name = "rocket68",
        .root_module = wasm_mod,
    });

    wasm_lib.addIncludePath(b.path("include"));
    wasm_lib.linkLibC();

    // Include core CPU files only (no `loader.c` and `disasm.c` because they use file I/O)
    const wasm_src_files = &.{
        "src/m68k/m68k.c",
        "src/m68k/ops_arith.c",
        "src/m68k/ops_bit.c",
        "src/m68k/ops_control.c",
        "src/m68k/ops_logic.c",
        "src/m68k/ops_move.c",
    };

    // Enable setjmp/longjmp via WASM exception handling proposal
    const wasm_c_flags = &.{
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-mllvm",
        "-wasm-enable-sjlj",
    };

    wasm_lib.addCSourceFiles(.{
        .files = wasm_src_files,
        .flags = wasm_c_flags,
    });

    // Standalone .wasm module (WASI reactor — no main, all symbols exported)
    const wasm_exe_mod = b.createModule(.{
        .target = wasm_target,
        .optimize = optimize,
    });

    const wasm_exe = b.addExecutable(.{
        .name = "rocket68",
        .root_module = wasm_exe_mod,
    });

    wasm_exe.addIncludePath(b.path("include"));
    wasm_exe.linkLibC();
    wasm_exe.addCSourceFiles(.{
        .files = wasm_src_files,
        .flags = wasm_c_flags,
    });
    wasm_exe.entry = .disabled; // No main — WASI reactor
    wasm_exe.rdynamic = true; // Export all public symbols
    wasm_exe.import_symbols = true; // Allow unresolved symbols (wasi-libc __main_void)

    const wasm_static_install = b.addInstallArtifact(wasm_lib, .{});
    const wasm_exe_install = b.addInstallArtifact(wasm_exe, .{});
    wasm_step.dependOn(&wasm_static_install.step);
    wasm_step.dependOn(&wasm_exe_install.step);

    const test_step = b.step("test-unit", "Run unit tests");
    const test_exists = rootPathExists(b, "test");
    if (test_exists) {
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

    const bench_step = b.step("bench", "Run benchmarks");
    const bench_exists = rootPathExists(b, "benches");
    if (bench_exists) {
        const bench_mod = b.createModule(.{
            .target = target,
            .optimize = optimize,
        });

        const bench_exe = b.addExecutable(.{
            .name = "benchmark_rocket68",
            .root_module = bench_mod,
        });
        bench_exe.addIncludePath(b.path("include"));
        bench_exe.addIncludePath(b.path("benches"));
        bench_exe.linkLibC();
        bench_exe.linkLibrary(lib);

        const bench_files = &.{
            "benches/benchmark_rocket68.c",
        };

        bench_exe.addCSourceFiles(.{
            .files = bench_files,
            .flags = c_flags,
        });

        const run_bench = b.addRunArtifact(bench_exe);
        bench_step.dependOn(&run_bench.step);

        // Build Musashi benchmark for comparison if the submodule is checked out
        const musashi_exists = rootPathExists(b, "external/musashi");
        if (musashi_exists) {
            const make_musashi = b.addSystemCommand(&.{ "make", "-C", "external/musashi" });
            make_musashi.setCwd(b.path("."));

            const musashi_mod = b.createModule(.{
                .target = target,
                .optimize = optimize,
            });

            const musashi_exe = b.addExecutable(.{
                .name = "benchmark_musashi",
                .root_module = musashi_mod,
            });
            musashi_exe.addIncludePath(b.path("external/musashi"));
            musashi_exe.addIncludePath(b.path("benches"));
            musashi_exe.linkLibC();
            musashi_exe.linkSystemLibrary("m");

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
            run_musashi.step.dependOn(&run_bench.step);
            bench_step.dependOn(&run_musashi.step);
        }

        // Build Moira benchmark for comparison if the submodule is checked out
        const moira_exists = rootPathExists(b, "external/moira");
        if (moira_exists) {
            const moira_mod = b.createModule(.{
                .target = target,
                .optimize = optimize,
            });

            const moira_exe = b.addExecutable(.{
                .name = "benchmark_moira",
                .root_module = moira_mod,
            });
            moira_exe.addIncludePath(b.path("external/moira/Moira"));
            moira_exe.addIncludePath(b.path("benches"));
            moira_exe.linkLibC();
            moira_exe.linkLibCpp();

            const cxx_flags: []const []const u8 = &.{
                "-std=c++20",
                "-Wno-unused-parameter",
                "-Wno-missing-field-initializers",
            };

            moira_exe.addCSourceFiles(.{
                .files = &.{
                    "benches/benchmark_moira.cpp",
                    "external/moira/Moira/Moira.cpp",
                    "external/moira/Moira/MoiraDebugger.cpp",
                },
                .flags = cxx_flags,
            });

            const run_moira = b.addRunArtifact(moira_exe);
            // Chain after Musashi if it exists, otherwise after Rocket 68
            if (musashi_exists) {
                // Find the last step added to bench_step and depend on it
                run_moira.step.dependOn(&run_bench.step);
            } else {
                run_moira.step.dependOn(&run_bench.step);
            }
            bench_step.dependOn(&run_moira.step);
        }
    }
}
