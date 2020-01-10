#!/usr/bin/env bash
# autocomplete script for File System 2 (fs2)

_fs2()
{
	local cur prev options
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	options="$(fs2 -o)"

	if [[ ${cur} == -* ]] || [[ ${cur} == '' ]] ; then
	    COMPREPLY=( $(compgen -W "${options}" -- ${cur}) )
	    return 0
	fi
}

complete -F _fs2 fs2