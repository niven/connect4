#!/usr/local/bin/fish

set NUM_GENERATIONS 8

echo "Generating up to $NUM_GENERATIONS generations."

rm -rf results
mkdir results

make all

# create gen 0 (empty board)
./bfcf -f results/generation_0.c4 -c e

for g in (seq $NUM_GENERATIONS)
	echo "Generation $g"
	set prev (math $g - 1)

	./bfcf -f results/generation_$prev.c4 -n 1
	mv next_generation.c4 results/generation_$g.c4

	./bfcf -f gencounter.gc -g
	mv gencounter.gc results/gencounter_$g.gc
	
	# save diskspace by not keeping old stuff
	rm results/generation_$prev.c4
end

echo -e "\nResults:"

for g in (seq $NUM_GENERATIONS)
	./bfcf -f results/gencounter_$g.gc -g
end
