#!/bin/bash
declare -a metrics=("ssim" "psnr" "mse" "nrmse")
declare -a scenarios=("teapotside" "teapotabove" "teapotzoomed" "groundhorizon" "groundzoomed" "groundabove")
declare -a othermethods=("Discard" "Traverse")

mkdir -p out/plots
mkdir -p out/images
mkdir -p out/tables

mv ../gpu-renderer/assets/screenshots/renders renders
mv renders/cpu CPU
mv renders/nvidia Nvidia
mv renders/intel Discard

# Images
for othermethod in "${othermethods[@]}"
do
	for scenario in "${scenarios[@]}"
	do
		python compare.py image renders/"$othermethod"/"$scenario".png renders/Hybrid/"$scenario".png -m ssim pixeldiff diffcontours -s -n "$othermethod" Hybrid -o out/images/"$othermethod"-Hybrid-"$scenario".png
		python compare.py image renders/"$othermethod"/"$scenario".png renders/reduced_traverse/"$scenario".png -m ssim pixeldiff diffcontours -s -n "$othermethod" "Reduced Traverse" -o out/images/"$othermethod"-Reduced_traverse-"$scenario".png
	  cp renders/reduced_traverse_viz/"$scenario".png out/images/Reduced_traverse-"$scenario"-viz.png
	done
done

# Tables
for metric in "${metrics[@]}"
do
  python compare.py table renders/Traverse -g Discard renders/Discard -g Hybrid renders/Hybrid -g "Reduced Traverse" renders/reduced_traverse -m "$metric" -o out/tables/Traverse-RT-"$metric".tex
done
python compare.py table renders/Traverse -g Discard renders/Discard -g "Reduced Traverse" renders/reduced_traverse -g Hybrid renders/Hybrid -v metricsavg -o out/tables/average.tex

# Plots
for metric in "${metrics[@]}"
do
	python compare.py table renders/Traverse -g Discard renders/Discard -g "Reduced Traverse" renders/reduced_traverse -g Hybrid renders/Hybrid -m "$metric" -p -o out/plots/Traverse-"$metric".png
done