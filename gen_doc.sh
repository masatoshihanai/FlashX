#!/bin/bash

echo 'roxygen2::roxygenize("Rpkg/")' | R --no-save
mkdir -p docs/FlashR
for file in `ls Rpkg/man/`
do
	input_file="Rpkg/man/$file"
	output_file="docs/FlashR/${file}.html"
	echo "tools::Rd2HTML(\"$input_file\", out=\"$output_file\")" | R --no-save
done
