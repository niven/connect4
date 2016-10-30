#!/usr/local/bin/fish

set NUM_GENERATIONS 2

echo "Generating up to $NUM_GENERATIONS generations."

rm -rf results
mkdir results

make clean
make MODE=NDEBUG all

# create gen 0 (database with single, empty board)
 ./store_boards --destination=results/generation_0

for current in (seq $NUM_GENERATIONS)
	echo "########################## Generation $current ##########################"
	set prev (math $current - 1)

	./bfcf --source=results/generation_$prev --destination=results/generation_$current --command=nextgen
	mv gencounter.gc results/gencounter_$current.gc
	
	# save diskspace by not keeping old stuff
	rm results/generation_$prev.c4_index
end

echo -e "\nResults:"

./bfcf --command=stats --source=results
./bfcf --command=stats --source=results > latest_results.txt
