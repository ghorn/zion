workspace(name = "zion_project")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//:dem_names.bzl", "dem_names")

http_archive(
    name = "rules_python",
    url = "https://github.com/bazelbuild/rules_python/releases/download/0.0.3/rules_python-0.0.3.tar.gz",
    sha256 = "e46612e9bb0dae8745de6a0643be69e8665a03f63163ac6610c210e80d14c3e4",
)
load("@rules_python//python:repositories.bzl", "py_repositories")
py_repositories()

load("@rules_python//python:pip.bzl", "pip_repositories")
pip_repositories()


[
    http_archive(
        name = name,
        #sha256 = shasum,
        urls = [
            "https://storage.googleapis.com/state-of-utah-sgid-downloads/lidar/zion-np-2016/QL2/DEMs/{}.zip".format(name),
        ],
        build_file_content = """
filegroup(
    name = "elevation_dataset_{name}",
    srcs = [
      "{name}.img"
      "{name}.img.aux.xml"
      "{name}.img.xml"
    ],
)
""".format(name=name),
    )
    for name in dem_names()
]
