def run_pipeline(config, max_xy_size):
    for source_dem in config:
        (xscale, yscale) = config[source_dem]['scale']
        for mask in config[source_dem]['masks']:
            mask_name = mask['name']
            for decimation in mask['decimations']:
                trim_args = '--trim="{}"'.format(mask['bounds']) if 'bounds' in mask else ''
                decimation_suffix = ("" if decimation == 1 else '_decimate{}'.format(decimation))
                ident = mask_name + decimation_suffix
                data_name = ident + ".dat"
                native.genrule(
                    name = "convert_data_" + ident,
                    tools = ["//src:dem_to_data"],
                    srcs = [source_dem],
                    outs = [data_name],
                    cmd ="$(location //src:dem_to_data) $< $@ --decimation {} {} && du -hs $@".format(decimation, trim_args),
                )

                native.genrule(
                    name = ident + "_png".format(),
                    tools = ["//src:data_to_png"],
                    outs = [ident + ".png".format()],
                    srcs = [data_name],
                    cmd ="$(location //src:data_to_png) $< $@ && du -hs $@",
                )

                native.genrule(
                    name = ident + "_png_shrunk".format(),
                    tools = ["//src:data_to_png"],
                    outs = [ident + "_shrunk.png".format()],
                    srcs = [data_name],
                    cmd ="$(location //src:data_to_png) $< $@ --target_dimension 2000 && du -hs $@",
                )

                if mask['make_grid_mesh']:
                    make_grid_mesh(ident, data_name, xscale, yscale)

                for num_triangles, num_triangles_string in mask['adaptive_meshes']:
                    adaptive_ident = ident + "_" + num_triangles_string + "_triangles"
                    make_adaptive_mesh(adaptive_ident, data_name, num_triangles, num_triangles_string,
                                       max_xy_size, xscale, yscale)


def sanitize_name(name):
    for bad_char in ['/', ':', '.']:
        name = name.replace(bad_char, '_')
    return name


def make_grid_mesh(ident, data_name, xscale, yscale):
    # Turn the data blob into a ply with hmply. but only for coarse decimation.
    native.genrule(
        name="grid_create_{}".format(ident),
        tools = ["//src:hmply"],
        srcs = [data_name],
        outs = ["grid_{}.ply".format(ident)],
        cmd = """
time $(location //src:hmply) -i $< -b 0.25 -e 170 -x {xscale} -y {yscale}
>&2 du -hs header.dat vertices.dat triangles.dat
>&2 echo "\nconcatenating files"
time cat header.dat vertices.dat triangles.dat > $@
rm header.dat vertices.dat triangles.dat
du -hs $@
""".format(
    xscale=xscale,
    yscale=yscale,
),
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


def make_adaptive_mesh(ident, data_name, num_triangles, num_triangles_string, max_xy_size, xscale, yscale):
    # Turn the blob into a ply with hmm.
    native.genrule(
        name = "triangulate_{}".format(ident),
        tools = ["//src:hmm"],
        srcs = [":" + data_name],
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
