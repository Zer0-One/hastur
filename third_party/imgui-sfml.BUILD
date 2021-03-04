load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "imgui-sfml",
    srcs = ["imgui-SFML.cpp"],
    hdrs = glob(["*.h"]),
    linkopts = select({
        "@platforms//os:windows": ["-DEFAULTLIB:opengl32"],
        "@platforms//os:linux": ["-lGL"],
    }),
    visibility = ["//visibility:public"],
    deps = [
        "@imgui",
        "@sfml//:graphics",
        "@sfml//:system",
        "@sfml//:window",
    ],
)
