#!/usr/bin/env bash

# generate_sample_disk.bash
# run this script to create a few sample files on a disk

function start() {
	local program_name=fs2
	./${program_name}
	info_file='data.txt'
	./${program_name} -c ${info_file}
	shopt -s dotglob
	shopt -s nullglob
	files=(./scripts/sample/*)
	for file in ${files[@]}; do
		./${program_name} -a ${info_file} ${file}"
"	# To add a newline
		if [ -d $file ]; then
			./${program_name} -d "$(basename "$file")"
		elif [ -f $file ]; then
			./${program_name} -w "$(basename "$file")" "$(cat ${file})"
		fi
	done
}


start