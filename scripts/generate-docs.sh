#!/usr/bin/env bash
# @file scripts/generate-docs.sh
# @brief Generates the complete release documentation in HTML, XML, and PDF.

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"
documents_dir="${project_root}/Documents"
pdf_dir="${documents_dir}/pdf"
pdf_name="RTOS-1.0.0-reference.pdf"

if ! command -v doxygen >/dev/null 2>&1; then
    echo "error: Doxygen is required to generate release documentation." >&2
    exit 1
fi

rm -rf -- \
    "${documents_dir}/html" \
    "${documents_dir}/xml" \
    "${documents_dir}/latex" \
    "${documents_dir}/rtf" \
    "${pdf_dir}"
mkdir -p -- "${pdf_dir}"

cd -- "${project_root}"

if command -v epstopdf >/dev/null 2>&1 \
    && { command -v latexmk >/dev/null 2>&1 || command -v pdflatex >/dev/null 2>&1; }
then
    {
        cat Doxyfile
        printf '\nGENERATE_LATEX = YES\n'
    } | doxygen -
else
    doxygen Doxyfile
fi

if [[ -d "${documents_dir}/latex" ]] && command -v latexmk >/dev/null 2>&1; then
    latexmk -pdf -interaction=nonstopmode \
        -output-directory="${documents_dir}/latex" \
        "${documents_dir}/latex/refman.tex"
    cp -- "${documents_dir}/latex/refman.pdf" "${pdf_dir}/${pdf_name}"
elif [[ -d "${documents_dir}/latex" ]] && command -v pdflatex >/dev/null 2>&1; then
    make -C "${documents_dir}/latex" pdf
    cp -- "${documents_dir}/latex/refman.pdf" "${pdf_dir}/${pdf_name}"
elif command -v libreoffice >/dev/null 2>&1; then
    profile_dir="$(mktemp -d /tmp/rtos-docs-libreoffice.XXXXXX)"
    trap 'rm -rf -- "${profile_dir}"' EXIT
    if ! libreoffice --headless \
        "-env:UserInstallation=file://${profile_dir}" \
        --convert-to pdf \
        --outdir "${pdf_dir}" \
        "${documents_dir}/rtf/refman.rtf" >/dev/null
    then
        echo "error: LibreOffice could not convert the Doxygen RTF reference." >&2
        exit 1
    fi
    mv -- "${pdf_dir}/refman.pdf" "${pdf_dir}/${pdf_name}"
else
    echo "error: install LaTeX or LibreOffice to generate the PDF reference." >&2
    exit 1
fi

printf 'HTML: %s\nXML:  %s\nPDF:  %s\n' \
    "${documents_dir}/html/index.html" \
    "${documents_dir}/xml/index.xml" \
    "${pdf_dir}/${pdf_name}"
