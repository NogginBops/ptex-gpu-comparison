#!/bin/bash
for foldername in */ ; do
	for filelocation in "$foldername"*.png ; do
		scenename=${foldername%/}
		renderername=${filelocation##*/}
		renderername=${renderername%.*}
		mkdir -p renders/"$renderername"
		cp "$filelocation" renders/"$renderername"/"$scenename".png
	done
done