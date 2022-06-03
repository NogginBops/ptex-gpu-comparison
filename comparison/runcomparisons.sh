#!/bin/bash
declare -a metrics=("ssim" "psnr" "mse" "nrmse")
declare -a othermethods=("Discard" "Traverse" "Traverse (8x MSAA)")

cd ../gpu-renderer/assets/screenshots/ || exit
sh renamerenders.sh
cd ../../../comparison || exit
rm -r renders
mv ../gpu-renderer/assets/screenshots/renders renders/
mv renders/cpu renders/CPU
mv renders/nvidia renders/Traverse
mv renders/intel renders/Discard
mv renders/nvidia_msaa_x8 renders/"Traverse (8x MSAA)"
#rm -r out
mkdir -p out/plots
mkdir -p out/images
mkdir -p out/tables
mkdir -p out/images-in
mkdir -p out/images-viz

# Images
for scenario in renders/Traverse/*.png;
do
  # Get scenario name
  scenario=${scenario##*/}
	scenario=${scenario%.png}

	# Copy relevant images
  cp renders/reduced_traverse_viz/"$scenario".png out/images-viz/"Reduced traverse-$scenario"-viz.png
	cp renders/hybrid_viz/"$scenario".png out/images-viz/Hybrid-"$scenario"-viz.png
	cp renders/Hybrid/"$scenario".png out/images-in/Hybrid-"$scenario".png
  cp renders/reduced_traverse/"$scenario".png out/images-in/"Reduced Traverse-$scenario".png
  cp renders/reduced_traverse_msaa_x8/"$scenario".png out/images-in/"Reduced Traverse (8x MSAA)-$scenario".png

  # Create comparison images
  for othermethod in "${othermethods[@]}"
  do
    cp renders/"$othermethod"/"$scenario".png out/images-in/"$othermethod-$scenario".png
		python compare.py image renders/"$othermethod"/"$scenario".png renders/Hybrid/"$scenario".png -m ssim pixeldiff diffcontours -s -n "$othermethod" Hybrid -o out/images/"$othermethod"-Hybrid-"$scenario".png
	  python compare.py image renders/"$othermethod"/"$scenario".png renders/reduced_traverse/"$scenario".png -m ssim pixeldiff diffcontours -s -n "$othermethod" "Reduced Traverse" -o out/images/"$othermethod-Reduced Traverse-$scenario".png
		python compare.py image renders/"$othermethod"/"$scenario".png renders/reduced_traverse_msaa_x8/"$scenario".png -m ssim pixeldiff diffcontours -s -n "$othermethod" "Reduced Traverse (8x MSAA)" -o out/images/"$othermethod-Reduced Traverse (8x MSAA)-$scenario".png
	done
done

# Tables
for metric in "${metrics[@]}"
do
  python compare.py table renders/Traverse -g "Traverse (8x MSAA)" renders/"Traverse (8x MSAA)" -g "Reduced Traverse (8x MSAA)" renders/reduced_traverse_msaa_x8 -g Discard renders/Discard -g Hybrid renders/Hybrid -g "Reduced Traverse" renders/reduced_traverse -m "$metric" -o out/tables/Traverse-"$metric".tex
  python compare.py table renders/"Traverse (8x MSAA)" -g Traverse renders/Traverse -g "Reduced Traverse (8x MSAA)" renders/reduced_traverse_msaa_x8 -g Discard renders/Discard -g Hybrid renders/Hybrid -g "Reduced Traverse" renders/reduced_traverse -m "$metric" -o out/tables/TraverseMSAA-"$metric".tex
done
# Averages
python compare.py table renders/Traverse -g "Traverse (8x MSAA)" renders/"Traverse (8x MSAA)" -g "Reduced Traverse (8x MSAA)" renders/reduced_traverse_msaa_x8 -g Discard renders/Discard -g "Reduced Traverse" renders/reduced_traverse -g Hybrid renders/Hybrid -v metricsavg -o out/tables/Traverse-average.tex
python compare.py table renders/"Traverse (8x MSAA)" -g Traverse renders/Traverse -g "Reduced Traverse (8x MSAA)" renders/reduced_traverse_msaa_x8 -g Discard renders/Discard -g "Reduced Traverse" renders/reduced_traverse -g Hybrid renders/Hybrid -v metricsavg -o out/tables/TraverseMSAA-average.tex

# Plots
for metric in "${metrics[@]}"
do
	python compare.py table renders/Traverse -g "Traverse (8x MSAA)" renders/"Traverse (8x MSAA)" -g "Reduced Traverse (8x MSAA)" renders/reduced_traverse_msaa_x8 -g Discard renders/Discard -g "Reduced Traverse" renders/reduced_traverse -g Hybrid renders/Hybrid -m "$metric" -p -o out/plots/Traverse-"$metric".png
	python compare.py table renders/"Traverse (8x MSAA)" -g Traverse renders/Traverse -g "Reduced Traverse (8x MSAA)" renders/reduced_traverse_msaa_x8 -g Discard renders/Discard -g "Reduced Traverse" renders/reduced_traverse -g Hybrid renders/Hybrid -m "$metric" -p -o out/plots/TraverseMSAA-"$metric".png
done