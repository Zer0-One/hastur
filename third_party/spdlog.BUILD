load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "spdlog",
    srcs = glob(
        include = ["src/*.cpp"],
        exclude = ["src/fmt.cpp"],
    ),
    hdrs = glob(["include/**/*.h"]),
    defines = [
        "SPDLOG_COMPILED_LIB",
        "SPDLOG_FMT_EXTERNAL",
        "SPDLOG_NO_EXCEPTIONS",
        # libc++18 doesn't set __cpp_lib_format >= 202207L which is required for this.
        # "SPDLOG_USE_STD_FORMAT",
    ],
    includes = ["include/"],
    linkopts = select({
        "@platforms//os:linux": ["-lpthread"],
        "@platforms//os:macos": ["-lpthread"],
        "@platforms//os:windows": [],
    }),
    strip_include_prefix = "include",
    target_compatible_with = select({
        "@platforms//os:wasi": ["@platforms//:incompatible"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = ["@fmt"],
)
