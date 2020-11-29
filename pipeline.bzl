def make_floating_blob(mask, decimation):
    trim_name = mask['name']

    # create floating point img
    floating_blob_path = "floating_blob_{}_decim_{}.dat".format(decimation, trim_name)
    trim_args = '--trim="{}"'.format(mask['bounds']) if 'bounds' in mask else ''
    native.genrule(
        name = "floating_blob_{}_decim_{}".format(decimation, trim_name),
        tools = ["//src:pixel_data_to_floating_blob"],
        srcs = [":pixel_data.dat"],
        outs = [floating_blob_path],
        cmd ="$(location //src:pixel_data_to_floating_blob) $< $@ --decimation {} {} && du -hs $@".format(decimation, trim_args),
    )

    return {
        "path": floating_blob_path,
        "decimation": decimation,
        "trim_name": trim_name,
    }


def make_grid_mesh(floating_blob):
    decimation = floating_blob["decimation"]
    trim_name = floating_blob["trim_name"]
    floating_blob_path = floating_blob["path"]

    # Turn the blob into a ply with hmply. but only for coarse decimation.
    ident = "{}_decim_{}".format(decimation, trim_name)
    native.genrule(
        name="grid_create_{}".format(ident),
        tools = ["//src:hmply"],
        srcs = [floating_blob_path],
        outs = ["grid_{}.ply".format(ident)],
        cmd = """
time $(location //src:hmply) -i $< -b 0.25 -x 170
>&2 du -hs header.dat vertices.dat triangles.dat
>&2 echo "\nconcatenating files"
time cat header.dat vertices.dat triangles.dat > $@
rm header.dat vertices.dat triangles.dat
du -hs $@
""",
    )
    native.genrule(
        name = "grid_ply2stl_{}".format(ident),
        tools = ["//src:ply2stl"],
        srcs = ["grid_{}.ply".format(ident)],
        outs = ["grid_{}.stl".format(ident)],
        local=True,
        cmd = """
$(location //src:ply2stl) $< $@
du -hs $@
""",
    )


def make_adaptive_mesh(floating_blob, num_triangles, num_triangles_string, max_xy_size):
    decimation = floating_blob["decimation"]
    trim_name = floating_blob["trim_name"]
    floating_blob_path = floating_blob["path"]

    ident = "{}_decim_{}_triangles_{}".format(decimation, num_triangles_string, trim_name)

    # Turn the blob into a ply with hmm.
    native.genrule(
        name = "triangulate_{}".format(ident),
        tools = ["//src:hmm"],
        srcs = [":" + floating_blob_path],
        outs = ["triangulate_{}.ply".format(ident)],
        cmd = "time $(location //src:hmm) -t {} --zoffset_fraction 0.25 --border-size 100 --border-height 0 $< $@ && du -hs $@".format(num_triangles),
    )

    native.genrule(
        name="cut_bad_base_{}".format(ident),
        tools = ["//src:trim_bottom"],
        srcs = ["triangulate_{}.ply".format(ident)],
        outs = ["cut_base_{}.ply".format(ident)],
        cmd = "$(location //src:trim_bottom) $< $@ && du -hs $@",
    )

    native.genrule(
        name="scale_{}".format(ident),
        tools = ["//src:scale"],
        srcs = ["cut_base_{}.ply".format(ident)],
        outs = ["scaled_{}.ply".format(ident)],
        cmd = "$(location //src:scale) {} $< $@ && du -hs $@".format(max_xy_size),
    )

    native.genrule(
        name="add_base_{}".format(ident),
        tools = ["//src:fill_bottom"],
        srcs = ["scaled_{}.ply".format(ident)],
        outs = [
            "final_{}.ply".format(ident),
            "bottom_triangles_{}.py".format(ident),
        ],
        cmd = "$(location //src:fill_bottom) $< $(OUTS) && du -hs $(OUTS)",
    )

    native.genrule(
        name="ply2stl_{}".format(ident),
        tools = ["//src:ply2stl"],
        srcs = ["final_{}.ply".format(ident)],
        outs = ["final_{}.stl".format(ident)],
        local=True,
        cmd = "$(location //src:ply2stl) $< $@ && du -hs $@",
    )

    native.py_binary(
        name = "plot_bottom_triangles_{}".format(ident),
        main = "bottom_triangles_{}.py".format(ident),
        srcs = ["bottom_triangles_{}.py".format(ident)],
        srcs_version = "PY3",
        python_version = "PY3",
    )

    native.genrule(
        name="bottom_triangles_picture_{}".format(ident),
        tools = ["plot_bottom_triangles_{}".format(ident)],
        outs = ["bottom_triangles_{}.png".format(ident)],
        cmd = "$(location plot_bottom_triangles_{}) $@".format(ident),
    )

    # execute roundtrip test
    native.sh_test(
        name="test_roundtrip_{}".format(ident),
        srcs = ["test_roundtrip_ply.sh"],
        data = [
            "final_{}.ply".format(ident),
            "//src:roundtrip_ply",
        ],
        args = ["$(location //src:roundtrip_ply)", "$(location final_{}.ply)".format(ident)],
    )


## Simplify the ply with meshlab.
#genrule(
#    name="simplified_ply",
#    tools = ["//meshlab:meshlab_server"],
#    srcs = [
#        "height_map_decim{}.ply".format(decimation),
#        "simplify.mlx",
#    ],
#    outs = ["height_map_decim{}_simplified.ply".format(decimation)],
#    cmd = """
#DISPLAY=:0.0 time $(location //meshlab:meshlab_server) -i $(location height_map_decim{}.ply) -o $@ -s $(location simplify.mlx)
#du -hs $@
#""".format(decimation),
#    local=True,
#    tags = ["manual"],
#)
#
## Convert simplified mesh to STL with meshlab.
#genrule(
#    name="simplified_stl",
#    tools = ["//meshlab:meshlab_server"],
#    srcs = [
#        "height_map_decim{}_simplified.ply".format(decimation),
#    ],
#    outs = ["height_map_decim{}_simplified.stl".format(decimation)],
#    cmd = """
#DISPLAY=:0.0 $(location //meshlab:meshlab_server) -i $< -o $@
#du -hs $@
#""".format(decimation),
#    local=True,
#    tags = ["manual"],
#)
