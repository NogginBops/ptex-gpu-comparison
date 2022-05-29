#!/bin/bash
declare -a metrics=("ssim" "psnr" "mse" "nrmse")
declare -a scenarios=("teapotside" "teapotabove" "teapotzoomed" "groundhorizon" "groundzoomed" "groundabove")
declare -a othermethods=("Discard" "Traverse")

mkdir -p out/plots
mkdir -p out/images
mkdir -p out/tables

# Images
for othermethod in "${othermethods[@]}"
do
	for scenario in "${scenarios[@]}"
	do
	  # mkdir -p out/images/"$othermethod"-Hybrid-"$scenario"
		python compare.py image in/"$othermethod"/"$scenario".png in/Hybrid/"$scenario".png -m ssim pixeldiff diffcontours -s -n "$othermethod" Hybrid -o out/images/"$othermethod"-Hybrid-"$scenario"-all.png
	done
done

# Tables
for metric in "${metrics[@]}"
do
  python compare.py table in/Traverse -g Discard in/Discard -g Hybrid in/Hybrid -m "$metric" -o out/tables/Traverse-"$metric".tex
done
python compare.py table in/Traverse -g Discard in/Discard -g Hybrid in/Hybrid -v metricsavg -o out/tables/average.tex

# Plots
for metric in "${metrics[@]}"
do
	python compare.py table in/Traverse -g Discard in/Discard -g Hybrid in/Hybrid -m "$metric" -p -o out/plots/nv-"$metric".png
done