import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import os
import skimage
from skimage import data
from skimage import io
from skimage import metrics


def read_files(ref_name, res_name):
    ref_img = io.imread(ref_name + '.png')
    res_img = io.imread(res_name + '.png')

    return ref_img, res_img


def psnr(ref, res):
    return metrics.peak_signal_noise_ratio(ref, res)


def mse(ref, res):
    return metrics.mean_squared_error(ref, res)


def ssim(ref, res):
    return metrics.structural_similarity(ref, res, channel_axis=2)


def nrmse(ref, res):
    return metrics.normalized_root_mse(ref, res)


comp_metrics = [["Peak signal-to-noise ratio", psnr],
                ["Mean squared error", mse],
                ["Structural similarity index measure", ssim],
                ["Normalized root mean squared error", nrmse]
                ]


def compare(ref_name, res_name):
    ref, res = read_files(ref_name, res_name)

    print("Comparison of {} vs. {}".format(ref_name, res_name))
    for name, met in comp_metrics:
        print(name, met(ref, res))


compare('test_cpu', 'test_ref')
