import pandas as pd
import argparse
from skimage import metrics, io, util, color, restoration
from os import listdir
from os.path import isfile, join
import matplotlib
from matplotlib import pyplot as plt, gridspec as gridspec
import numpy as np
import cv2
import re
import pywt

matplotlib.use("pgf")
matplotlib.rcParams.update({
    "pgf.texsystem": "pdflatex",
    'font.family': 'serif',
    'text.usetex': True,
    'pgf.rcfonts': False,
})


def psnr(ref, res):
    return metrics.peak_signal_noise_ratio(ref, res)


def mse(ref, res):
    return metrics.mean_squared_error(ref, res)


def cwssim(ref, res):
    ref_cw = restoration.denoise_wavelet(ref, wavelet='cmor')
    res_cw = restoration.denoise_wavelet(res, wavelet='cmor')

    return metrics.structural_similarity(ref_cw, res_cw, channel_axis=2)


def ssim(ref, res):
    return metrics.structural_similarity(ref, res, channel_axis=2)


def nrmse(ref, res):
    return metrics.normalized_root_mse(ref, res)


def mae(ref, res):
    ref_lab = color.rgb2lab(ref)
    res_lab = color.rgb2lab(res)

    total_error = sum(sum(color.deltaE_cie76(ref_lab, res_lab)))

    mean_abs_error = total_error / len(ref_lab)

    return mean_abs_error


def read_image(img_name):
    return io.imread(img_name)


def read_images(img_names):
    return [read_image(img_name) for img_name in img_names]


def dir_files(path):
    return [join(path, file) for file in listdir(path) if isfile(join(path, file)) and file.endswith(".png")]


def load_images(ref_renders_names, gpus_renders_names):
    ref_images = read_images(ref_renders_names)
    gpus_images = [read_images(gpu_renders_names) for gpu_renders_names in gpus_renders_names]

    return ref_images, gpus_images


def comp_metrics_res(ref_img, res_img, comp_metrics):
    return [comp_metric(ref_img, res_img) for _, comp_metric in comp_metrics.items()]


def ref_gpu_res(comp_metrics, img_pairs):
    return [comp_metrics_res(ref_img, res_img, comp_metrics) for ref_img, res_img in img_pairs]


def ref_gpu_dataframe(img_pairs, comp_metrics, scenes_names):
    metric_names_upper = [name.upper() for name in comp_metrics]

    return pd.DataFrame(ref_gpu_res(comp_metrics, img_pairs), index=scenes_names, columns=metric_names_upper)


def ref_gpu_dataframes(ref_images, gpus_images, comp_metrics, scenes_names):
    ref_gpus_img_pairs = ref_gpus_lists(ref_images, gpus_images)

    return [ref_gpu_dataframe(img_pairs, comp_metrics, scenes_names) for img_pairs in ref_gpus_img_pairs]


def ref_gpus_lists(ref_images, gpus_images):
    return [zip(ref_images, gpu_images) for gpu_images in gpus_images]


def get_dataframe_parameters(implementation_names):
    levels_names = ["Implementation", "Metric"]

    return implementation_names, levels_names


def combine_res_dataframes(dataframes, implementations_names):
    implementation_names, levels_names = get_dataframe_parameters(implementations_names)

    combined_dataframe = pd.concat(dataframes, keys=implementation_names, names=levels_names, axis=1)
    swaplevel_dataframe = combined_dataframe.swaplevel(axis=1)
    sorted_dataframe = swaplevel_dataframe.reindex(sorted(swaplevel_dataframe.columns), axis=1)

    return sorted_dataframe


def gen_res_dataframe(comp_metrics, ref_images, gpus_images, scenes_names, implementations_names):
    dataframes = ref_gpu_dataframes(ref_images, gpus_images, comp_metrics, scenes_names)

    return combine_res_dataframes(dataframes, implementations_names)


def metrics_avg_dataframe(dataframe):
    return dataframe.mean().to_frame().transpose()


def get_latex_table(dataframe):
    return dataframe.style.to_latex(hrules=True)


def get_plot(dataframe):
    print(dataframe)

    dataframe.columns = dataframe.columns.droplevel()
    dataframe["idx"] = dataframe.index
    dataframe["implementation1"] = dataframe.iloc[:, 0]
    dataframe["implementation2"] = dataframe.iloc[:, 1]

    ax = dataframe.plot.scatter(x="idx", y="implementation1", color="Blue", label=dataframe.iloc[:, 0].name)
    dataframe.plot.scatter(x="idx", y="implementation2", color="Red", label=dataframe.iloc[:, 1].name, ax=ax)
    ax.vlines(x=dataframe.index, ymin=dataframe.iloc[:, 0], ymax=dataframe.iloc[:, 1])
    ax.set(xlabel="Scenarios", ylabel="Measured values")

    return ax


def keyword_to_dataframe(dataframe, res_type="all"):
    if res_type == "metricsavg":
        return metrics_avg_dataframe(dataframe)
    else:
        return dataframe


def write_table(dataframe_latex, outfile):
    with open(outfile, 'w') as file:
        file.write(dataframe_latex)


def valid_metrics(metrics_names):
    return {key: comparison_metrics[key] for key in metrics_names}


def valid_scenes_names(scenes_names, renders_names):
    if scenes_names is None:
        return [re.sub('.+/', '', name)[:-4] for name in renders_names]
    else:
        return scenes_names


def valid_renders_names(renders_names):
    if renders_names[0].endswith(".png"):
        return renders_names
    else:
        return dir_files(renders_names[0])


def remove_ticks(plot):
    plot.tick_params(
        axis='both',
        which='both',
        left=False,
        right=False,
        labelleft=False,
        labelbottom=False,
        bottom=False
    )


def plot_input_image(image, fig, grid_pos, label):
    axis = fig.add_subplot(grid_pos)

    axis.imshow(image, cmap=plt.cm.gray)
    axis.set_title(label)

    remove_ticks(axis)


def plot_input_images(image1, image2, fig, grid, image_names):
    if image_names is None:
        image_names = ["Image 1", "Image 2"]

    plot_input_image(image1, fig, grid[0, 0], image_names[0])
    plot_input_image(image2, fig, grid[0, 1], image_names[1])


def plot_diff_images(diff_images, diff_images_names, fig, grid):
    num_cols = 2

    for i, img in enumerate(diff_images):
        x = (i % num_cols) * 2
        y = int(i / num_cols) * 2

        if i == len(diff_images) - 1 and i % 2 == 0:
            x = num_cols - 1

        comp_plot = fig.add_subplot(grid[y:y + 2, x:x + 2])
        comp_plot.imshow(img, cmap=plt.cm.viridis, vmin=0.0, vmax=1.0)
        comp_plot.set_title(diff_images_names[i])
        remove_ticks(comp_plot)


# Partially taken from: https://scikit-image.org/docs/stable/auto_examples/applications/plot_image_comparison.html
def output_comparison_images(image1, image2, diff_images, diff_images_names, outfile, image_names,
                             show_input_images=False):
    num_cols = 2
    num_rows = int(np.ceil(len(diff_images) / num_cols))
    fig_height = 4 * num_rows
    grid_height = 1
    diff_gs_start_row = 0

    if show_input_images:
        fig_height += 3
        grid_height += 1
        diff_gs_start_row += 1

    fig = plt.figure(figsize=(8, fig_height), constrained_layout=True)

    outer_gs = gridspec.GridSpec(3, grid_height, figure=fig)

    if show_input_images:
        plot_input_images(image1, image2, fig, outer_gs, image_names)

    diff_gs = gridspec.GridSpecFromSubplotSpec(num_rows * 2, num_cols * 2, subplot_spec=outer_gs[diff_gs_start_row:, :])

    plot_diff_images(diff_images, diff_images_names, fig, diff_gs)

    plt.savefig(outfile)
    print("Output saved in", outfile)


# Taken from:
# https://stackoverflow.com/questions/56183201/detect-and-visualize-differences-between-two-images-with-opencv-python
def img_ssim_diff(image1, image2):
    image1_gray = cv2.cvtColor(image1, cv2.COLOR_BGR2GRAY)
    image2_gray = cv2.cvtColor(image2, cv2.COLOR_BGR2GRAY)

    (score, diff) = metrics.structural_similarity(image1_gray, image2_gray, full=True)

    return diff


# Taken from:
# https://stackoverflow.com/questions/56183201/detect-and-visualize-differences-between-two-images-with-opencv-python
def img_diff_contours(image1, image2):
    image1_gray = cv2.cvtColor(image1, cv2.COLOR_BGR2GRAY)
    image2_gray = cv2.cvtColor(image2, cv2.COLOR_BGR2GRAY)

    (score, diff) = metrics.structural_similarity(image1_gray, image2_gray, full=True)

    diff = (diff * 255).astype("uint8")

    thresh = cv2.threshold(diff, 0, 255, cv2.THRESH_BINARY_INV | cv2.THRESH_OTSU)[1]
    contours = cv2.findContours(thresh.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    contours = contours[0] if len(contours) == 2 else contours[1]

    filled_after = image2.copy()

    for c in contours:
        cv2.drawContours(filled_after, [c], 0, (0, 255, 0), 2)

    return filled_after


def img_gray_pixel_diff(image1, image2):
    return util.compare_images(gray_image1, gray_image2, method="diff")


def img_pixel_diff(image1, image2):
    return 3 * util.compare_images(image1, image2, method="diff")


def img_checkerboard(image1, image2):
    image1_gray = cv2.cvtColor(image1, cv2.COLOR_BGR2GRAY)
    image2_gray = cv2.cvtColor(image2, cv2.COLOR_BGR2GRAY)

    return util.compare_images(image1_gray, image2_gray, method="checkerboard")


def img_blend(image1, image2):
    return util.compare_images(image1, image2, method="blend")


def filter_img_methods(methods_names):
    return {key: img_comp_methods[key] for key in methods_names}


def generate_comparison_images(image1_name, image2_name, comp_methods_names, show_input_images, image_names,
                               outfile="out.pgf"):
    comp_methods = filter_img_methods(comp_methods_names)

    image1 = read_image(image1_name)
    image2 = read_image(image2_name)

    diff_images = [fn(image1, image2) for _, fn in comp_methods.items()]

    output_comparison_images(image1, image2, diff_images, list(comp_methods.keys()), outfile, image_names,
                             show_input_images)


def make_table_output(ref_renders_names, gpus_renders_names, comp_metrics, implementations_names, scenes_names,
                      res_type, should_plot):
    ref_images, gpus_images = load_images(ref_renders_names, gpus_renders_names)
    dataframe = gen_res_dataframe(comp_metrics, ref_images, gpus_images, scenes_names, implementations_names)
    requested_dataframe = keyword_to_dataframe(dataframe, res_type)

    if should_plot:
        dataframe_latex = get_plot(requested_dataframe)
    else:
        dataframe_latex = get_latex_table(requested_dataframe)

    return dataframe_latex


def valid_comp_parameters(metrics_names, scenes_names, ref_renders_names, gpus_renders_names):
    val_metrics = valid_metrics(metrics_names)
    val_ref_renders_names = valid_renders_names(ref_renders_names)
    val_implementations_names = [names[0] for names in gpus_renders_names]
    val_gpus_renders_names = [valid_renders_names(names[1:]) for names in gpus_renders_names]
    val_scenes_names = valid_scenes_names(scenes_names, val_gpus_renders_names[0])

    return val_metrics, val_scenes_names, val_ref_renders_names, val_gpus_renders_names, val_implementations_names


def output_plot(dataframe_plot, outfile="out.png"):
    if outfile is None:
        outfile = "out.png"

    plt.savefig(outfile)
    print("Output saved in", outfile)


def output_comparisons(dataframe_latex, outfile="out.tex"):
    if outfile is None:
        outfile = "out.tex"

    print("Output saved in", outfile)
    print(dataframe_latex)
    write_table(dataframe_latex, outfile)


def generate_comparison_table(metrics_names, ref_renders_names, gpus_renders_names, scenes_names, res_type,
                              should_plot, outfile):
    val_metrics_names, val_scenes_names, val_ref_renders_names, val_gpus_renders_names, \
        val_implementations_names = valid_comp_parameters(
            metrics_names,
            scenes_names,
            ref_renders_names,
            gpus_renders_names)

    dataframe_latex = make_table_output(val_ref_renders_names, val_gpus_renders_names, val_metrics_names,
                                        val_implementations_names, val_scenes_names, res_type, should_plot)

    if should_plot:
        output_plot(dataframe_latex, outfile)
    else:
        output_comparisons(dataframe_latex, outfile)


def parser_generate_comparison_image(image_args):
    generate_comparison_images(image_args.image1, image_args.image2, image_args.methods, image_args.showinputimages,
                               image_args.imagenames, image_args.outfile)


def parser_generate_comparison_table(table_args):
    generate_comparison_table(table_args.metrics, table_args.reference, table_args.gpu, table_args.names,
                              table_args.view, table_args.plot, table_args.outfile)


def init_table_subparser(tableparser):
    tableparser.set_defaults(func=parser_generate_comparison_table)
    tableparser.add_argument(
        "reference",
        metavar="r",
        nargs="+",
        type=str,
        help="Directory or space-separated list of CPU-rendered PNGs"
    )
    tableparser.add_argument(
        "-g",
        "--gpu",
        nargs="+",
        type=str,
        action="append",
        help="AT LEAST ONE REQUIRED! Name of renderer followed by directory or space-separated list of GPU-rendered "
             "PNGs",
        required=True
    )
    tableparser.add_argument(
        "-n",
        "--names",
        nargs="*",
        type=str,
        help="Names of the scenes"
    )
    tableparser.add_argument(
        "-v",
        "--view",
        nargs="?",
        type=str,
        default="all",
        choices=["all", "metricsavg"],
        help="What data to output"
    )
    tableparser.add_argument(
        "-o",
        "--outfile",
        nargs="?",
        type=str,
        help="File to output latex table to"
    )
    tableparser.add_argument(
        "-m",
        "--metrics",
        nargs="*",
        type=str,
        default=["psnr", "mse", "ssim", "nrmse"],
        help="Metrics to be calculated"
    )
    tableparser.add_argument(
        "-p",
        "--plot",
        action=argparse.BooleanOptionalAction,
        help="Plot output",
        default=False
    )


def init_image_subparser(imageparser):
    imageparser.set_defaults(func=parser_generate_comparison_image)
    imageparser.add_argument(
        "image1",
        metavar="i1",
        nargs="?",
        type=str,
        help="Comparison image 1"
    )
    imageparser.add_argument(
        "image2",
        metavar="i2",
        nargs="?",
        type=str,
        help="Comparison image 2"
    )
    imageparser.add_argument(
        "-m",
        "--methods",
        nargs="*",
        type=str,
        default=["ssim", "pixeldiff", "graypixeldiff", "blend", "checkerboard", "diffcontours"],
        help="Methods for comparing the images"
    )
    imageparser.add_argument(
        "-o",
        "--outfile",
        nargs="?",
        type=str,
        default="out.pgf",
        help="File to output image to"
    )
    imageparser.add_argument(
        "-s",
        "--showinputimages",
        action=argparse.BooleanOptionalAction,
        help="Show input images in generated image"
    )
    imageparser.add_argument(
        "-n",
        "--imagenames",
        nargs=2,
        type=str,
        default=["Image 1", "Image 2"]
    )


def parse_args():
    cli = argparse.ArgumentParser(description="Render comparison script")
    subparsers = cli.add_subparsers(title="subcommands", help="Choose to output image or table")
    table_parser = subparsers.add_parser("table",
                                         description="Latex table of comparison metrics for reference & GPU renders",
                                         help="table help", formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    image_parser = subparsers.add_parser("image",
                                         description="Visual comparisons of reference & GPU renders",
                                         help="image help", formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    init_table_subparser(table_parser)
    init_image_subparser(image_parser)

    return cli.parse_args()


img_comp_methods = {
    "pixeldiff": img_pixel_diff,
    "blend": img_blend,
    "checkerboard": img_checkerboard,
    "graypixeldiff": img_gray_pixel_diff,
    "ssim": img_ssim_diff,
    "diffcontours": img_diff_contours
}

comparison_metrics = {
    "psnr": psnr,
    "mse": mse,
    "ssim": ssim,
    "nrmse": nrmse,
    "mae": mae,
    "cwssim": cwssim
}

if __name__ == "__main__":
    comp_metrs = comparison_metrics
    args = parse_args()
    args.func(args)
