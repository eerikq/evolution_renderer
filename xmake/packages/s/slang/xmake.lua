-- Slang from the upstream prebuilt release rather than the xmake registry.
--
-- The registry lags a long way behind (it still offers nothing newer than
-- v2025.11.0), and the renderer's material system is built on Slang modules,
-- interfaces and link-time specialization, which need a current compiler.
-- Upstream ships prebuilt binaries for every platform we target, so there is
-- nothing to build here.
--
-- To move to a newer Slang: bump the version, download the matching release
-- assets, and put their sha256 sums below.
package("slang")
    set_homepage("https://github.com/shader-slang/slang")
    set_description("Slang shading language compiler")
    set_license("Apache-2.0")

    if is_plat("linux") and is_arch("arm64", "aarch64") then
        set_urls("https://github.com/shader-slang/slang/releases/download/v$(version)/slang-$(version)-linux-aarch64-glibc-2.28.tar.gz")
        add_versions("2026.14.1", "ebbc011a210f16902c180ef82f94b3e5b52ea0c80d7827a8a830961be3aafa3c")
    elseif is_plat("linux") then
        -- glibc-2.28 build: the oldest baseline upstream publishes, so it runs
        -- on anything newer without pulling the toolchain's libstdc++ along.
        set_urls("https://github.com/shader-slang/slang/releases/download/v$(version)/slang-$(version)-linux-x86_64-glibc-2.28.tar.gz")
        add_versions("2026.14.1", "07cf4274f5f1f976d75554cc5c1cc4bd993a9a92442a7726fa701aa457183a26")
    elseif is_plat("macosx") and is_arch("arm64") then
        set_urls("https://github.com/shader-slang/slang/releases/download/v$(version)/slang-$(version)-macos-aarch64.tar.gz")
        add_versions("2026.14.1", "92da7ab6226dd951037cd85397f830ae78fe40fbbb8928882e0b2654e468fdd4")
    elseif is_plat("macosx") then
        set_urls("https://github.com/shader-slang/slang/releases/download/v$(version)/slang-$(version)-macos-x86_64.tar.gz")
        add_versions("2026.14.1", "adc5e179f5584ca572293e93612df9f0b6be8a46dcb53622238efc7a62b1da2f")
    elseif is_plat("windows") then
        set_urls("https://github.com/shader-slang/slang/releases/download/v$(version)/slang-$(version)-windows-x86_64.zip")
        add_versions("2026.14.1", "5ed0a59d650a0af0aca45d5db4e083b3d8fb5cea05748747dd95dfbe9c580658")
    end

    on_install(function (package)
        os.cp("include", package:installdir())
        os.cp("lib", package:installdir())

        if package:is_plat("windows") then
            -- The DLLs and the core module live in bin/, and lib/ holds only
            -- import libraries. There is no rpath here, so whatever runs Slang
            -- needs these beside it; the slang_link rule does that copy.
            os.cp("bin", package:installdir())
            package:addenv("PATH", "bin")
            package:set("links", "slang")
            return
        end

        package:addenv("LD_LIBRARY_PATH", "lib")
        package:addenv("DYLD_LIBRARY_PATH", "lib")

        -- Beside the real libraries sit slang-glslang, slang-glsl-module and
        -- slang-llvm: loadable bundles Slang opens at runtime, not link inputs.
        -- Mach-O rejects a bundle outright ("only MH_OBJECT and MH_DYLIB can be
        -- linked"), so letting xmake scan lib/ and link what it finds breaks the
        -- macOS build. They must stay on disk regardless: dropping slang-glslang
        -- changes the SPIR-V Slang generates, despite it logging a load failure
        -- at startup. So name the link inputs explicitly instead of letting the
        -- scan decide.
        local libdir = package:installdir("lib")
        package:set("libfiles", path.join(libdir, "libslang" .. (package:is_plat("macosx") and ".dylib" or ".so")))

        if package:is_plat("linux") then
            -- Never "slang": -lslang resolves against /usr/lib/libslang.so, which
            -- is S-Lang, an unrelated library with a colliding name. The binary
            -- then carries a NEEDED entry for it and refuses to start on a
            -- machine that has no S-Lang. libslang-compiler is the same library
            -- under the name its own SONAME already uses, and nothing else on a
            -- Linux system is called that.
            --
            -- The GLSL module is a runtime bundle rather than an API, but linking
            -- it is what keeps it loaded: without it Slang emits different
            -- SPIR-V, and it says so only in a startup log line.
            package:set("links", {"slang-compiler", "slang-glsl-module-" .. package:version_str()})
        else
            package:set("links", "slang")
        end
    end)

    -- Just the header: has_cxxfuncs cannot express a namespaced function (it
    -- builds a snippet that pastes the qualified name into a declaration), and
    -- for a prebuilt drop the useful check is that the include dir resolves.
    on_test(function (package)
        assert(package:has_cxxincludes("slang.h", {configs = {languages = "c++17"}}))
    end)
package_end()
