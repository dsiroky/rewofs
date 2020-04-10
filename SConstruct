import os
import sys
import subprocess
import platform

#==========================================================================

PROJECT_ROOT = Dir("#").abspath
TOOL_PATH = os.path.join(PROJECT_ROOT, "tools/scons")

#==========================================================================
# build options

debug = bool(int(ARGUMENTS.get("debug", 1)))
verbose = bool(int(ARGUMENTS.get("verbose", 0)))
sanitizers = ARGUMENTS.get("sanitizers", "")

Help("""
Variants
--------
scons verbose=(0|1)              print short or long processing commands (default: %(verbose)i)
scons debug=(0|1)                compile debug or release (default: %(debug)i)
scons sanitizers=address,leak    link sanitizers (not in release build, default: "%(sanitizers)s")
scons run_e2e_tests
""" % {"verbose":verbose, "debug":debug,
        "sanitizers":sanitizers})

#==========================================================================

EnsurePythonVersion(2, 7)
EnsureSConsVersion(2, 4)
Decider("MD5-timestamp")
CacheDir(os.path.join(PROJECT_ROOT, ".build_cache"))

#==========================================================================

env = Environment(ENV=os.environ, toolpath=[TOOL_PATH])

env["PROJECT_ROOT"] = PROJECT_ROOT
env["ENV"]["PROJECT_ROOT"] = PROJECT_ROOT

if platform.system() == "Windows":
    env["OS"] = "msw"
    env["TARGET_CPU"] = (os.environ.get("TARGET_CPU") or
                        os.environ.get("TARGET_ARCH") or
                        "x86_64")
elif "linux" in sys.platform:
    env["OS"] = "lnx"
    env["TARGET_CPU"] = subprocess.Popen("uname -m", shell=True,
                              stdout=subprocess.PIPE).stdout.read().strip()
else:
    env["OS"] = "osx"
    env["TARGET_CPU"] = subprocess.Popen("uname -m", shell=True,
                              stdout=subprocess.PIPE).stdout.read().strip()
env["ARCH"] = "${OS}_${TARGET_CPU}"

#==========================================================================

env["CC"] = os.environ.get("CC") or env["CC"]
env["CXX"] = os.environ.get("CXX") or env["CXX"]

env["BUILD_DEBUG"] = debug
env["BUILD_VERBOSE"] = verbose
build_dir_name = "build/${ARCH}_%s" % ("debug" if debug else "release")
env["BUILD_DIR"] = env.Dir(build_dir_name)

env["CXXFILESUFFIX"] = ".cpp" # default is ".cc"

env.Tool("misc")
env.Tool("flatbuffers")
env.Tool("compile")

env["TEMPDIR"] = env.FindTempDir()

env.SetupLinker("Program")
env.SetupLinker("SharedLibrary")
env.PretifyOutput(verbose,
            ["CC", "CXX", "LINK", "SHCC", "SHCXX", "SHLINK", "AR", "RANLIB",
            "FLATC"])

#==========================================================================

env["CONFIGUREDIR"] = "$BUILD_DIR/sconf_temp"
env["CONFIGURELOG"] = "$BUILD_DIR/config.log"
if not env.GetOption("clean"):
    print("Checking compiler warnings...")
    code = "void f(){}"
    conf = Configure(env.Clone())
    c_warnings = []
    cxx_warnings = []
    for warning in (
            "-Wabstract-vbase-init",
            "-Waddress-of-array-temporary",
            "-Warray-bounds",
            "-Warray-bounds-pointer-arithmetic",
            "-Wassign-enum",
            "-Wattributes",
            "-Wbool-conversion",
            "-Wbridge-cast",
            "-Wbuiltin-requires-header",
            "-Wc++11-narrowing",
            "-Wcast-align",
            "-Wchar-subscripts",
            "-Wconditional-uninitialized",
            "-Wconstant-conversion",
            "-Wconstant-logical-operand",
            "-Wconstexpr-not-const",
            "-Wconsumed",
            "-Wconversion",
            "-Wdangling-else",
            "-Wdangling-field",
            "-Wdeprecated",
            "-Wdeprecated-increment-bool",
            "-Wduplicate-method-match",
            "-Weffc++",
            "-Wempty-body",
            "-Wenum-conversion",
            "-Wfloat-conversion",
            "-Wfloat-equal",
            "-Wint-conversion",
            "-Wmissing-braces",
            "-Wnon-virtual-dtor",
            "-Wold-style-cast",
            "-Wparentheses",
            "-Wpointer-sign",
            "-Wshadow",
            "-Wshorten-64-to-32",
            "-Wstrict-aliasing",
            "-Wswitch",
            "-Wswitch-default",
            "-Wswitch-enum",
            "-Wtautological-compare",
            "-Wthread-safety-analysis",
            "-Wundeclared-selector",
            "-Wuninitialized",
            "-Wunreachable-code",
            "-Wunused-function",
            "-Wunused-parameter",
            "-Wunused-result",
            "-Wunused-value",
            "-Wunused-variable",
            ):

        conf.env["CFLAGS"] = ["-Werror", warning]
        if conf.TryCompile(code, ".c"):
            c_warnings.append(warning)

        conf.env["CXXFLAGS"] = ["-Werror", warning]
        if conf.TryCompile(code, ".cpp"):
            cxx_warnings.append(warning)

    assert len(c_warnings) > 0
    assert len(cxx_warnings) > 0
    env.AppendUnique(CFLAGS=c_warnings)
    env.AppendUnique(CXXFLAGS=cxx_warnings)

#==========================================================================

env.AppendUnique(
    CPPPATH=os.environ.get("CPPPATH", "").split(os.pathsep),
    LIBPATH=os.environ.get("LIBPATH", "").split(os.pathsep),
    RPATH=os.environ.get("LIBPATH", "").split(os.pathsep),
)

env.AppendUnique(
    CPPPATH=[
        "#/src",
        "#$BUILD_DIR/src",
        "#/thirdparty/GSL/include",
        "#/thirdparty/strong_type/include",
        ],
    CPPDEFINES=[
        "PROJECT_ROOT=\\\"%s\\\"" % env["PROJECT_ROOT"].replace("\\", "\\\\"),
        "_FILE_OFFSET_BITS=64",
    ],
)

env.AppendUnique(
    CCFLAGS=[
            "-pipe",
            "-pthread",

            "-fPIC",
            "-fstrict-aliasing",
            "-fsigned-char",

            "-pedantic",
            "-Wall",
            "-Wextra",
        ],
    CFLAGS=["-std=c11"],
    CXXFLAGS=[
            "-std=c++17",
            ],
    LINKFLAGS=["-pthread"]
    )

if "clang" in env["CC"]:
    env.AppendUnique(
            CPPDEFINES=[
                # clang does not turn on WUR checking in libstdc++
                "__attribute_warn_unused_result__=__attribute__ ((__warn_unused_result__))",
                "__wur=__attribute__ ((__warn_unused_result__))",
            ],
            CCFLAGS=[
                "-fcolor-diagnostics",
                "-ferror-limit=10",
            ],
        )
else:
    env.AppendUnique(
            CCFLAGS=[
                "-fmax-errors=10"
            ]
        )


if debug:
    env.AppendUnique(
            CPPDEFINES = ["_DEBUG"],
            CCFLAGS=[
                "-O0",
                "-ggdb3",
                "-fno-omit-frame-pointer",
                "-fno-optimize-sibling-calls",
                "-fno-inline-functions",
                "-funwind-tables"
            ]
        )
    if sanitizers:
        SANITIZERS = ["-fsanitize=" + sanitizers]
        env.AppendUnique(CCFLAGS=SANITIZERS, LINKFLAGS=SANITIZERS)
else:
    env.AppendUnique(
            CPPDEFINES=["NDEBUG"],
            CCFLAGS=[
                "-O3",
                "-Werror",
                "-Wdisabled-optimization",
                "-Wno-error=strict-overflow", # gcc false warnings
            ]
        )

#==================================================================

Export("env")

#==================================================================

env.SConsignFile("$BUILD_DIR/.sconsign")

SConscript("SConscript", variant_dir=env["BUILD_DIR"])

env.Execute("ln -sfrn $BUILD_DIR build/last")
env.Execute("ln -sfrn $BUILD_DIR build/${ARCH}_last")
