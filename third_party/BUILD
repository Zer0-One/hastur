load("@rules_cc//cc:defs.bzl", "cc_library")

# glib
# ================================================================================
exports_files(
    ["config.h", "glibconfig.h"],
    visibility = ["@glib//:__pkg__"],
)

cc_library(
    name = "glib_config",
    hdrs = ["config.h", "glibconfig.h"],
    includes = ["."],
    visibility = ["@glib//:__pkg__"],
)
# ================================================================================
