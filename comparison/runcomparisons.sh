#!/bin/bash
declare -a metrics=("ssim" "psnr" "mse" "nrmse")
declare -a scenarios=("teapotside" "teapotabove" "teapotzoomed" "groundhorizon" "groundzoomed" "groundabove")
declare -a othermethods=("Discard" "Traverse")

# Tables


# Images
for 
	for scenario in "${scenarios[@]}" do
		python compare.py image Intel/teapotside.png Hybrid/teapotside.png -m ssim pixeldiff diffcontours -s -n Intel Hybrid -o all.png
	done

# Plots

for metric in "${metrics[@]}" do
	python compare.py table in/Traverse -g Discard in/Discard -g Hybrid in/Hybrid -m "$metric" -o out/plots/nv-"$metric".png
done