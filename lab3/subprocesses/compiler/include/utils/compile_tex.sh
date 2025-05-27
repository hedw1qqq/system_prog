#!/bin/bash

if [ ! $# -eq 2 ];then
    exit 1
fi

filename="$1"

if [ -f "$filename" ]; then
    pdflatex -halt-on-error -interaction=nonstopmode -output-directory="$(dirname "$2")" -jobname="$(basename "$2")" "$filename"
    rm -f "$(dirname "$2")/$(basename "$2").log" \
          "$(dirname "$2")/$(basename "$2").toc" \
          "$(dirname "$2")/$(basename "$2").aux"
    exit 0
else
  echo "This file doesn't exist" >&2
  exit 1
fi