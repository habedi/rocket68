const std = @import("std");
const builtin = @import("builtin");

fn rootPathExists(b: *std.Build, rel_path: []const u8) bool {
    const abs_path = b.pathFromRoot(rel_path);
    // Zig 0.16 removed std.fs.accessAbsolute in favor of the std.Io interface.
    if (comptime @hasDecl(std.fs, "accessAbsolute")) {
        std.fs.accessAbsolute(abs_path, .{}) catch return false;
    } else {
        std.Io.Dir.accessAbsolute(b.graph.io, abs_path, .{}) catch return false;
    }
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

    lib_mod.addIncludePath(b.path("include"));
    lib_mod.link_libc = true;

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

    lib_mod.addCSourceFiles(.{
        .files = src_files,
        .flags = c_flags,
    });

    b.installArtifact(lib);

    // --- WASM build step ---
    const wasm_step = b.step("wasm", "Build Rocket 68 as a WASM library (wasm32-wasi)");

    // Zig 0.16.0's bundled clang crashes while compiling musl's EH-based
    // setjmp runtime, so on 0.16 and later the WASM build drops the
    // exception-handling feature and links the stubs from src/wasm_compat
    // instead of the native setjmp/longjmp lowering.
    const wasm_native_sjlj = comptime builtin.zig_version.order(.{ .major = 0, .minor = 16, .patch = 0 }) == .lt;

    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .cpu_features_add = if (wasm_native_sjlj)
            std.Target.wasm.featureSet(&.{.exception_handling})
        else
            std.Target.wasm.featureSet(&.{}),
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

    wasm_mod.addIncludePath(b.path("include"));
    wasm_mod.link_libc = true;

    // Include core CPU files only (no `loader.c` and `disasm.c` because they use file I/O)
    const wasm_src_files = &.{
        "src/m68k/m68k.c",
        "src/m68k/ops_arith.c",
        "src/m68k/ops_bit.c",
        "src/m68k/ops_control.c",
        "src/m68k/ops_logic.c",
        "src/m68k/ops_move.c",
    };

    // Enable setjmp/longjmp via the WASM exception handling proposal
    const wasm_c_flags: []const []const u8 = if (wasm_native_sjlj) &.{
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-mllvm",
        "-wasm-enable-sjlj",
    } else &.{
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-pedantic",
    };
    const wasm_stub_files: []const []const u8 = &.{
        "src/wasm_compat/setjmp_stubs.c",
    };

    wasm_mod.addCSourceFiles(.{
        .files = wasm_src_files,
        .flags = wasm_c_flags,
    });
    if (!wasm_native_sjlj) {
        wasm_mod.addIncludePath(b.path("src/wasm_compat"));
        wasm_mod.addCSourceFiles(.{
            .files = wasm_stub_files,
            .flags = wasm_c_flags,
        });
    }

    // Standalone .wasm module (WASI reactor — no main, all symbols exported)
    const wasm_exe_mod = b.createModule(.{
        .target = wasm_target,
        .optimize = optimize,
    });

    const wasm_exe = b.addExecutable(.{
        .name = "rocket68",
        .root_module = wasm_exe_mod,
    });

    wasm_exe_mod.addIncludePath(b.path("include"));
    wasm_exe_mod.link_libc = true;
    wasm_exe_mod.addCSourceFiles(.{
        .files = wasm_src_files,
        .flags = wasm_c_flags,
    });
    if (!wasm_native_sjlj) {
        wasm_exe_mod.addIncludePath(b.path("src/wasm_compat"));
        wasm_exe_mod.addCSourceFiles(.{
            .files = wasm_stub_files,
            .flags = wasm_c_flags,
        });
    }
    wasm_exe.entry = .disabled; // No main — WASI reactor
    wasm_exe.rdynamic = true; // Export all public symbols
    wasm_exe.import_symbols = true; // Allow unresolved symbols (wasi-libc __main_void)

    const wasm_static_install = b.addInstallArtifact(wasm_lib, .{});
    const wasm_exe_install = b.addInstallArtifact(wasm_exe, .{});
    wasm_step.dependOn(&wasm_static_install.step);
    wasm_step.dependOn(&wasm_exe_install.step);

    const test_step = b.step("test-unit", "Run unit tests");
    const test_exists = rootPathExists(b, "tests");
    if (test_exists) {
        const test_files = &.{
            "tests/test_addressing.c",
            "tests/test_arith.c",
            "tests/test_control.c",
            "tests/test_core.c",
            "tests/test_integration.c",
            "tests/test_loader.c",
            "tests/test_logic.c",
            "tests/test_main.c",
            "tests/test_move.c",
            "tests/test_regression.c",
        };

        const test_mod = b.createModule(.{
            .target = target,
            .optimize = optimize,
        });

        const test_exe = b.addExecutable(.{
            .name = "test_main",
            .root_module = test_mod,
        });
        test_mod.addIncludePath(b.path("include"));
        test_mod.link_libc = true;

        test_mod.addCSourceFiles(.{
            .files = src_files,
            .flags = c_flags,
        });
        test_mod.addCSourceFiles(.{
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
        bench_mod.addIncludePath(b.path("include"));
        bench_mod.addIncludePath(b.path("benches"));
        bench_mod.link_libc = true;
        bench_mod.linkLibrary(lib);

        const bench_files = &.{
            "benches/benchmark_rocket68.c",
        };

        bench_mod.addCSourceFiles(.{
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
            musashi_mod.addIncludePath(b.path("external/musashi"));
            musashi_mod.addIncludePath(b.path("benches"));
            musashi_mod.link_libc = true;
            musashi_mod.linkSystemLibrary("m", .{});

            musashi_exe.step.dependOn(&make_musashi.step);

            const musashi_files = &.{
                "benches/benchmark_musashi.c",
            };

            musashi_mod.addCSourceFiles(.{
                .files = musashi_files,
                .flags = c_flags,
            });
            musashi_mod.addObjectFile(b.path("external/musashi/m68kcpu.o"));
            musashi_mod.addObjectFile(b.path("external/musashi/m68kops.o"));
            musashi_mod.addObjectFile(b.path("external/musashi/m68kdasm.o"));
            musashi_mod.addObjectFile(b.path("external/musashi/softfloat/softfloat.o"));

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
            moira_mod.addIncludePath(b.path("external/moira/Moira"));
            moira_mod.addIncludePath(b.path("benches"));
            moira_mod.link_libc = true;
            moira_mod.link_libcpp = true;

            const cxx_flags: []const []const u8 = &.{
                "-std=c++20",
                "-Wno-unused-parameter",
                "-Wno-missing-field-initializers",
            };

            moira_mod.addCSourceFiles(.{
                .files = &.{
                    "benches/benchmark_moira.cpp",
                    "external/moira/Moira/Moira.cpp",
                    "external/moira/Moira/MoiraDebugger.cpp",
                },
                .flags = cxx_flags,
            });

            const run_moira = b.addRunArtifact(moira_exe);
            run_moira.step.dependOn(&run_bench.step);
            bench_step.dependOn(&run_moira.step);
        }
    }
}
