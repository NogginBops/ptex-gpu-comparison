import pandas as pd
import argparse
from skimage import io
from skimage import metrics
from os import listdir
from os.path import isfile, join


def psnr(ref, res):
    return metrics.peak_signal_noise_ratio(ref, res)


def mse(ref, res):
    return metrics.mean_squared_error(ref, res)


def ssim(ref, res):
    return metrics.structural_similarity(ref, res, channel_axis=2)


def nrmse(ref, res):
    return metrics.normalized_root_mse(ref, res)


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
    print(dataframe_latex)
    write_table(dataframe_latex, outfile)


def generate_comparisons(metrics_names, ref_renders_names, gpus_renders_names, scenes_names, res_type="all",
                         outfile="out.tex"):
    selected_metrics, scenes_names, ref_renders_names, gpus_renders_names = valid_comp_parameters(metrics_names,
                                                                                                  scenes_names,
                                                                                                  ref_renders_names,
                                                                                                  gpus_renders_names)

    dataframe_latex = make_latex_table(ref_renders_names, gpus_renders_names, selected_metrics, scenes_names, res_type)

    output_comparisons(dataframe_latex, outfile)


def parse_args():
    cli = argparse.ArgumentParser(description="Latex table of comparison metrics for reference & GPU renders",
                                  formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    cli.add_argument(
        "reference",
        metavar="r",
        nargs="+",
        type=str,
        help="Directory or space-separated list of CPU-rendered PNGs"
    )
    cli.add_argument(
        "-g",
        "--gpu",
        nargs="+",
        type=str,
        action="append",
        help="Directory or space-separated list of GPU-rendered PNGs",
        required=True
    )
    cli.add_argument(
        "-n",
        "--names",
        nargs="*",
        type=str,
        help="Names of the scenes"
    )
    cli.add_argument(
        "-v",
        "--view",
        nargs="?",
        type=str,
        default="all",
        choices=["singlemetric", "all", "metricsavg"],
        help="What data to output"
    )
    cli.add_argument(
        "-o",
        "--outfile",
        nargs="?",
        type=str,
        default="out.tex",
        help="File to output latex table to"
    )
    cli.add_argument(
        "-m",
        "--metrics",
        nargs="*",
        type=str,
        default=["psnr", "mse", "ssim", "nrmse"],
        help="Metrics to be calculated"
    )
    return cli.parse_args()


comparison_metrics = {
    "psnr": psnr,
    "mse": mse,
    "ssim": ssim,
    "nrmse": nrmse
}

if __name__ == "__main__":
    comp_metrs = comparison_metrics
    args = parse_args()
    generate_comparisons(args.metrics, args.reference, args.gpu, args.names, args.view, args.outfile)
