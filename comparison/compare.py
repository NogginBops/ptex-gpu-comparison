import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import os
import argparse
import skimage
from skimage import data
from skimage import io
from skimage import metrics
from functools import partial


def psnr(ref, res):
    return metrics.peak_signal_noise_ratio(ref, res)


def mse(ref, res):
    return metrics.mean_squared_error(ref, res)


def ssim(ref, res):
    return metrics.structural_similarity(ref, res, channel_axis=2)


def nrmse(ref, res):
    return metrics.normalized_root_mse(ref, res)


def read_image(img_name):
    return io.imread(img_name + '.png')


def read_images(img_names):
    return [read_image(img_name) for img_name in img_names]


def load_images(ref_renders_names, gpus_renders_names):
    ref_images = read_images(ref_renders_names)
    gpus_images = [read_images(gpu_renders_names) for gpu_renders_names in gpus_renders_names]

    return ref_images, gpus_images


def comp_metrics_res(ref_img, res_img, comp_metrics):
    return [comp_metric(ref_img, res_img) for _, comp_metric, _ in comp_metrics]


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


def combine_res_dataframes(dataframes, comp_metrics, num_gpus):
    metric_names = [name for _, _, name in comp_metrics]
    implementation_names = gen_gpu_names(num_gpus)
    levels_names = ['Implementation', 'Scene']

    dataframe = pd.concat(dataframes, keys=implementation_names, names=levels_names)
    dataframe.columns = metric_names

    return dataframe


# Shape: (implementations, renders, metrics)
def gen_res_dataframe(comp_metrics, ref_images, gpus_images, scenes_names):
    dataframes = ref_gpu_dataframes(ref_images, gpus_images, comp_metrics, scenes_names)

    return combine_res_dataframes(dataframes, comp_metrics, len(gpus_images))


def gpus_metric_averages(dataframe):
    return dataframe.groupby(level='Implementation').mean()


def gpus_renders_ssim(dataframe):
    return dataframe["SSIM"].unstack()


def generate_latex_table(dataframe):
    return dataframe.style.to_latex()


comparison_metrics = [["Peak signal-to-noise ratio", psnr, "PSNR"],
                      ["Mean squared error", mse, "MSE"],
                      ["Structural similarity index measure", ssim, "SSIM"],
                      ["Normalized root mean squared error", nrmse, "NRMSE"]
                      ]


def compare(comp_metrics, ref_renders_names, gpus_renders_names, scenes_names, res_type, outfile):
    ref_images, gpus_images = load_images(ref_renders_names, gpus_renders_names)
    dataframe = gen_res_dataframe(comp_metrics, ref_images, gpus_images, scenes_names)

    dataframe = {
        "ssim": gpus_renders_ssim(dataframe),
        "avg": gpus_metric_averages(dataframe),
        "all": dataframe
    }.get(res_type, "all")

    dataframe_latex = generate_latex_table(dataframe)
    print(dataframe_latex)

    with open(outfile, 'w') as file:
        file.write(dataframe_latex)


def parse_args():
    cli = argparse.ArgumentParser(description="Comparison metrics for images produced by renderers")
    cli.add_argument(
        "-r",
        "--ref",
        nargs="*",
        type=str,
        required=True
    )
    cli.add_argument(
        "-g",
        "--gpu",
        nargs="*",
        type=str,
        required=True,
        action="append"
    )
    cli.add_argument(
        "-s",
        "--scenes",
        nargs="*",
        type=str,
        required=True
    )
    cli.add_argument(
        "-t",
        "--type",
        nargs="?",
        type=str,
        default="all"
    )
    cli.add_argument(
        "-o",
        "--outfile",
        nargs="?",
        type=str,
        default="out.tex"
    )
    return cli.parse_args()


if __name__ == "__main__":
    comp_metrics = comparison_metrics
    args = parse_args()
    compare(comp_metrics, args.ref, args.gpu, args.scenes, args.type, args.outfile)