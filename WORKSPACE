workspace(name = "zion_project")

load("//:dem_names.bzl", "dem_names", "dem_checksum")
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

[
    http_archive(
        name = "dem_{}".format(dem_name),
        urls = ["https://storage.googleapis.com/state-of-utah-sgid-downloads/lidar/zion-np-2016/QL2/DEMs/{}.zip".format(dem_name)],
        sha256 = dem_checksum(dem_name),
        build_file_content = """
filegroup(
    name = "dem_img_{dem_name}",
    srcs = [
        "{dem_name}.img",
        "{dem_name}.img.aux.xml",
        "{dem_name}.img.xml",
    ],
    visibility = ["//visibility:public"],
)
""".format(dem_name=dem_name)
    )
    for dem_name in dem_names()
]
