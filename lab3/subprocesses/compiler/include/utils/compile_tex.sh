#!/bin/bash

INPUT_TEX_FILE="$1"
EXPECTED_OUTPUT_PDF_FILE="$2" 

TEMP_OUTPUT_DIR="/tmp"

INPUT_BASENAME=$(basename "${INPUT_TEX_FILE}" .tex)

DEFAULT_PDF_IN_TEMP_DIR="${TEMP_OUTPUT_DIR}/${INPUT_BASENAME}.pdf"

pdflatex -interaction=nonstopmode \
         -output-directory="${TEMP_OUTPUT_DIR}" \
         "${INPUT_TEX_FILE}" > /dev/null 2>&1

if [ -f "${DEFAULT_PDF_IN_TEMP_DIR}" ]; then
  
  mv "${DEFAULT_PDF_IN_TEMP_DIR}" "${EXPECTED_OUTPUT_PDF_FILE}"

  if [ -f "${EXPECTED_OUTPUT_PDF_FILE}" ]; then
    
    rm -f "${TEMP_OUTPUT_DIR}/${INPUT_BASENAME}.aux" "${TEMP_OUTPUT_DIR}/${INPUT_BASENAME}.log"
    exit 0 
  else

    rm -f "${DEFAULT_PDF_IN_TEMP_DIR}" 
    rm -f "${TEMP_OUTPUT_DIR}/${INPUT_BASENAME}.aux" "${TEMP_OUTPUT_DIR}/${INPUT_BASENAME}.log"
    exit 3 
  fi
else
  
  
  
  
  
  
  
  rm -f "${TEMP_OUTPUT_DIR}/${INPUT_BASENAME}.aux" "${TEMP_OUTPUT_DIR}/${INPUT_BASENAME}.log"
  exit 1 
fi