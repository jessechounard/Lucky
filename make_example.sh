#!/bin/bash

if [[ $# -eq 0 ]] ; then
    echo 'Usage: ./make_example "The Example Name"'
    exit 1
fi


sed "s/EmptyExample/$1/" Projects/VisualStudio/Examples/EmptyExample.vcxproj > Projects/VisualStudio/Examples/"$1".vcxproj
cp Source/Examples/EmptyExample.cpp Source/Examples/"$1".cpp
