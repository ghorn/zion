workspace(name = "zion")

load("//dem/zion:zion_dem_names.bzl", "zion_dem_names", "zion_dem_checksum")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

[
    http_archive(
        name = "zion_dem_{}".format(dem_name),
        urls = ["https://storage.googleapis.com/state-of-utah-sgid-downloads/lidar/zion-np-2016/QL2/DEMs/{}.zip".format(dem_name)],
        sha256 = zion_dem_checksum(dem_name),
        build_file_content = """
filegroup(
    name = "dem_img_{dem_name}",
    srcs = [
        "{dem_name}.img",
        #"{dem_name}.img.aux.xml",
        #"{dem_name}.img.xml",
    ],
    visibility = ["//visibility:public"],
)
""".format(dem_name=dem_name)
    )
    for dem_name in zion_dem_names()
]
