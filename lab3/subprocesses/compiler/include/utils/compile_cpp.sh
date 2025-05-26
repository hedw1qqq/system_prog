#!/bin/bash


if [ ! $# -eq 2 ];then
    exit 1
fi

filename="$1"

if [ -f "$filename" ]; then
#  g++ main.cpp -o main
    g++ "$filename" "-o" "$2"
else
  exit 1
fi
