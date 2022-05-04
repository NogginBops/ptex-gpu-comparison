import pandas as pd
import argparse
from skimage import io
from skimage import metrics
from skimage import util
# from sklearn import metrics as learnmetrics
from os import listdir
from os.path import isfile, join
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
from skimage import color
import numpy as np
import cv2


def psnr(ref, res):
    return metrics.peak_signal_noise_ratio(ref, res)


def mse(ref, res):
    return metrics.mean_squared_error(ref, res)


def ssim(ref, res):
    return metrics.structural_similarity(ref, res, channel_axis=2)


def nrmse(ref, res):
    return metrics.normalized_root_mse(ref, res)


def mae(ref, res):
    # learnmetrics.mean_absolute_error(ref, res)
    return 1337


def read_image(img_name):
    return io.imread(img_name)


def read_images(img_names):
    return [read_image(img_name) for img_name in img_names]


def dir_files(path):
    return [file for file in listdir(path) if isfile(join(path, file))]


def load_images(ref_renders_names, gpus_renders_names):
    ref_images = read_images(ref_renders_names)
    gpus_images = [read_images(gpu_renders_names) for gpu_renders_names in gpus_renders_names]

    return ref_images, gpus_images


def comp_metrics_res(ref_img, res_img, comp_metrics):
    return [comp_metric(ref_img, res_img) for _, comp_metric in comp_metrics.items()]


def ref_gpu_res(comp_metrics, img_pairs):
    return [comp_metrics_res(ref_img, res_img, comp_metrics) for ref_img, res_img in img_pairs]


def ref_gpu_dataframe(img_pairs, comp_metrics, scenes_names):
    return pd.DataFrame(ref_gpu_res(comp_metrics, img_pairs), index=scenes_names)


def ref_gpu_dataframes(ref_images, gpus_images, comp_metrics, scenes_names):
    ref_gpus_img_pairs = ref_gpus_lists(ref_images, gpus_images)

    return [ref_gpu_dataframe(img_pairs, comp_metrics, scenes_names) for img_pairs in ref_gpus_img_pairs]


def ref_gpus_lists(ref_images, gpus_images):
    return [zip(ref_images, gpu_images) for gpu_images in gpus_images]


def gen_gpu_names(num_gpus):
    return ["GPU_" + str(gpu_id) for gpu_id in range(0, num_gpus)]


def get_dataframe_parameters(comp_metrics, num_gpus):
    metric_names = [name.upper() for name in comp_metrics]
    implementation_names = gen_gpu_names(num_gpus)
    levels_names = ['Implementation', 'Scene']

    return metric_names, implementation_names, levels_names


def combine_res_dataframes(dataframes, comp_metrics, num_gpus):
    metric_names, implementation_names, levels_names = get_dataframe_parameters(comp_metrics, num_gpus)

    combined_dataframe = pd.concat(dataframes, keys=implementation_names, names=levels_names)
    combined_dataframe.columns = metric_names

    return combined_dataframe


def gen_res_dataframe(comp_metrics, ref_images, gpus_images, scenes_names):
    dataframes = ref_gpu_dataframes(ref_images, gpus_images, comp_metrics, scenes_names)

    return combine_res_dataframes(dataframes, comp_metrics, len(gpus_images))


def metrics_avg_dataframe(dataframe):
    return dataframe.groupby(level='Implementation').mean()


def single_metric_dataframe(dataframe):
    return dataframe.iloc[:, 0].unstack()


def get_latex_table(dataframe):
    return dataframe.style.to_latex()


def make_latex_table(ref_renders_names, gpus_renders_names, comp_metrics, scenes_names, res_type):
    ref_images, gpus_images = load_images(ref_renders_names, gpus_renders_names)
    dataframe = gen_res_dataframe(comp_metrics, ref_images, gpus_images, scenes_names)
    requested_dataframe = keyword_to_dataframe(dataframe, res_type)
    dataframe_latex = get_latex_table(requested_dataframe)

    return dataframe_latex


def keyword_to_dataframe(dataframe, res_type="all"):
    if res_type == "singlemetric":
        return single_metric_dataframe(dataframe)
    elif res_type == "metricsavg":
        return metrics_avg_dataframe(dataframe)
    else:
        return dataframe


def write_table(dataframe_latex, outfile):
    with open(outfile, 'w') as file:
        file.write(dataframe_latex)


def valid_metrics(metrics_names):
    return {key: comparison_metrics[key] for key in metrics_names}


def valid_scenes_names(scenes_names, ref_renders_names):
    if scenes_names is None:
        return range(0, len(ref_renders_names))
    else:
        return scenes_names


def valid_renders_names(renders_names):
    if not renders_names[-1].endswith(".png"):
        return listdir(renders_names[0])
    else:
        return renders_names


def valid_comp_parameters(metrics_names, scenes_names, ref_renders_names, gpus_renders_names):
    val_metrics = valid_metrics(metrics_names)
    val_scenes_names = valid_scenes_names(scenes_names, ref_renders_names)
    val_ref_renders_names = valid_renders_names(ref_renders_names)
    val_gpus_renders_names = [valid_renders_names(names) for names in gpus_renders_names]

    return val_metrics, val_scenes_names, val_ref_renders_names, val_gpus_renders_names


def output_comparisons(dataframe_latex, outfile):
    print("Output saved in", outfile)
    print(dataframe_latex)
    write_table(dataframe_latex, outfile)


# Partially taken from: https://scikit-image.org/docs/stable/auto_examples/applications/plot_image_comparison.html
def output_comparison_image(image1, image2, diff_image, outfile):
    fig = plt.figure(figsize=(8, 9))

    gs = GridSpec(3, 2)
    ax0 = fig.add_subplot(gs[0, 0])
    ax1 = fig.add_subplot(gs[0, 1])
    ax2 = fig.add_subplot(gs[1:, :])

    ax0.imshow(image1, cmap=plt.cm.gray)
    ax0.set_title('Image 1')
    ax1.imshow(image2, cmap=plt.cm.gray)
    ax1.set_title('Image 2')
    ax2.imshow(diff_image, cmap=plt.cm.viridis, vmin=0.0, vmax=0.8)
    ax2.set_title('Diff comparison')
    for a in (ax0, ax1, ax2):
        a.axis('off')
    plt.tight_layout()
    plt.plot()
    plt.savefig(outfile)
    print("Output saved in", outfile)


# Taken from: https://stackoverflow.com/questions/56183201/detect-and-visualize-differences-between-two-images-with-opencv-python
def ssim_diff(image1, image2):
    image1_gray = cv2.cvtColor(image1, cv2.COLOR_BGR2GRAY)
    image2_gray = cv2.cvtColor(image2, cv2.COLOR_BGR2GRAY)

    (score, diff) = metrics.structural_similarity(image1_gray, image2_gray, full=True)

    return diff


def diff_countours(image1, image2):
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


def comparison_image_using_method(image1, image2, comp_method):
    if comp_method == "diff":
        return util.compare_images(image1, image2, method="diff")
    elif comp_method == "blend":
        return util.compare_images(image1, image2, method="blend")
    elif comp_method == "checkerboard":
        return util.compare_images(image1, image2, method="checkerboard")
    elif comp_method == "graypixeldiff":
        gray_image1 = color.rgb2gray(image1)
        gray_image2 = color.rgb2gray(image2)
        return util.compare_images(gray_image1, gray_image2, method="diff")
    elif comp_method == "ssim":
        return ssim_diff(image1, image2)
    elif comp_method == "diffcontour":
        return diff_countours(image1, image2)
    else:
        return util.compare_images(image1, image2, method="diff")


def generate_comparison_image(image1_name, image2_name, comp_method, outfile="out.png"):
    image1 = read_image(image1_name)
    image2 = read_image(image2_name)

    diff_image = comparison_image_using_method(image1, image2, comp_method)

    output_comparison_image(image1, image2, diff_image, outfile)


def generate_comparison_table(metrics_names, ref_renders_names, gpus_renders_names, scenes_names, res_type="all",
                         outfile="out.tex"):
    val_metrics_names, val_scenes_names, val_ref_renders_names, val_gpus_renders_names = valid_comp_parameters(
        metrics_names,
        scenes_names,
        ref_renders_names,
        gpus_renders_names)

    dataframe_latex = make_latex_table(val_ref_renders_names, val_gpus_renders_names, val_metrics_names, val_scenes_names, res_type)

    output_comparisons(dataframe_latex, outfile)


def parser_generate_comparison_image(image_args):
    generate_comparison_image(image_args.image1, image_args.image2, image_args.method, image_args.outfile)


def parser_generate_comparison_table(table_args):
    generate_comparison_table(table_args.metrics, table_args.reference, table_args.gpu, table_args.names,
                              table_args.view, table_args.outfile)


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
        help="Directory or space-separated list of GPU-rendered PNGs",
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
        choices=["singlemetric", "all", "metricsavg"],
        help="What data to output"
    )
    tableparser.add_argument(
        "-o",
        "--outfile",
        nargs="?",
        type=str,
        default="out.tex",
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
        "--method",
        nargs="?",
        type=str,
        choices=["ssim", "pixeldiff", "graypixeldiff", "blend", "checkerboard", "diffcontour"],
        default="pixeldiff",
        help="Method of creating comparison image"
    )
    imageparser.add_argument(
        "-o",
        "--outfile",
        nargs="?",
        type=str,
        default="out.png",
        help="File to output image to"
    )


def parse_args():
    cli = argparse.ArgumentParser(description="Latex table of comparison metrics for reference & GPU renders",
                                  formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    subparsers = cli.add_subparsers(title="subcommands", help="Choose to output image or table")
    table_parser = subparsers.add_parser("table", help="table help")
    image_parser = subparsers.add_parser("image", help="image help")

    init_table_subparser(table_parser)
    init_image_subparser(image_parser)

    return cli.parse_args()


comparison_metrics = {
    "psnr": psnr,
    "mse": mse,
    "ssim": ssim,
    "nrmse": nrmse,
    "mae": mae
}

if __name__ == "__main__":
    comp_metrs = comparison_metrics
    args = parse_args()
    args.func(args)
