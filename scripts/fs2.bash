#!/usr/bin/env bash
# autocomplete script for File System 2 (fs2)

red=$'\e[1;31m'
none=$'\e[0m'

_fs2()
{
	local cur prev options
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	options="$(fs2 -o)"
	file_list="$(fs2 -l)"

	if [[ ${cur} == -* ]] ; then
	    COMPREPLY=( $(compgen -W "${options}" -- ${cur}) )
	    return 0
	fi

	if [[ ${cur} == * ]] ; then
	    COMPREPLY=( $(compgen -W "${file_list}" -- ${cur}) )
	    return 0
	fi
}

complete -F _fs2 fs2