workspace(name = "zion_project")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_python",
    url = "https://github.com/bazelbuild/rules_python/releases/download/0.0.3/rules_python-0.0.3.tar.gz",
    sha256 = "e46612e9bb0dae8745de6a0643be69e8665a03f63163ac6610c210e80d14c3e4",
)
#load("@rules_python//python:repositories.bzl", "py_repositories")
#py_repositories()
#
#load("@rules_python//python:pip.bzl", "pip_repositories")
#pip_repositories()


# boost
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "com_github_nelhage_rules_boost",
    commit = "1e3a69bf2d5cd10c34b74f066054cd335d033d71",
    remote = "https://github.com/nelhage/rules_boost",
    shallow_since = "1591047380 -0700",
)

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()


# hmstl and it's prerequesite trix
http_archive(
    name = "trix",
    urls = ["https://github.com/anoved/libtrix/archive/fb23d3df2c4c607a28fbefbbda21fa43b1522b3f.tar.gz"],
    strip_prefix = "libtrix-fb23d3df2c4c607a28fbefbbda21fa43b1522b3f",
    sha256 = "a4401674bda8c23a4c60f33145d5ab805e582a15ccf3edb216c3a89afb156be5",
    build_file_content = """
cc_library(
    name = "trix",
    hdrs = ["libtrix.h"],
    srcs = ["libtrix.c"],
    visibility = ["//visibility:public"],
    includes = ["."],
)
"""
)

http_archive(
    name = "hmstl",
    urls = ["https://github.com/anoved/hmstl/archive/addf2f9d5fccce9d569e8001befdbe4ba8e063f7.tar.gz"],
    strip_prefix = "hmstl-addf2f9d5fccce9d569e8001befdbe4ba8e063f7",
    sha256 = "8941390a37bf9bde8ec0dfc8f783adeaab6d9514f59717dae4e5445face5f757",
    build_file_content = """
cc_library(
    name = "libhmstl",
    srcs = [
        "heightmap.c",
        "stb_image.c",
    ],
    hdrs = [
        "heightmap.h",
        "stb_image.h",
        "stb_image.c",
    ],
    deps = ["@trix//:trix"],
    visibility = ["//visibility:public"],
    #includes = ["."],
    copts = ["-Wno-implicit-function-declaration", "-Wno-misleading-indentation", "-Wno-unused-but-set-variable"],
)

cc_binary(
    name = "hmstl",
    srcs = [
        "hmstl.c",
    ],
    deps = [
       "@trix//:trix",
       ":libhmstl",
    ],
    visibility = ["//visibility:public"],
    #includes = ["."],
)
"""
)
