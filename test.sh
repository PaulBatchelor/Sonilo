./t < input.txt | grep -v "^#" > gen.txt
diff gen.txt ref.txt
