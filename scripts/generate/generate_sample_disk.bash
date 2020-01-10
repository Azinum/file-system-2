#!/usr/bin/env bash

# generate_sample_disk.bash
# run this script to create a few sample files on a disk

function start() {
	./file_system
	info_file='data.txt'
	./file_system c ${info_file}
	shopt -s dotglob
	shopt -s nullglob
	files=(scripts/generate/sample/*)
	for file in ${files[@]}; do
		./file_system a ${info_file} ${file}"
"	# To add a newline
		if [ -d $file ]; then
			./file_system d "$(basename "$file")"
		elif [ -f $file ]; then
			./file_system w "$(basename "$file")" "$(cat ${file})"
		fi
	done
}


start