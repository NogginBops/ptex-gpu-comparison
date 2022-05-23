#!/bin/bash
for foldername in */ ; do
	for filelocation in $foldername*.png ; do
		scenename=${foldername%/}
		renderername=${filelocation##*/}
		renderername=${renderername%.*}
		mkdir -p testimages2/$renderername
		mv $filelocation testimages2/$renderername/$scenename.png
	done
done