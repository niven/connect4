#!/usr/bin/fish


if test -z $argv;
	set NUM_GENERATIONS 8;
else;
	set NUM_GENERATIONS $argv;
end

echo "Generating up to $NUM_GENERATIONS generations."

make clean
make MODE=NDEBUG all

for current in (seq 1 $NUM_GENERATIONS)

	if test ! -d "./data/$current"
		mkdir "./data/$current"
	end

	rm -f "./data/$current/*"

	echo "########################## Generation $current ##########################"
	set prev (math $current - 1)

#	./bfcf --source=results/generation_$prev --destination=results/generation_$current --command=nextgen
#	mv gencounter.gc results/gencounter_$current.gc
	
	# save diskspace by not keeping old stuff
#	rm results/generation_$prev.c4_index
end

echo -e "\nResults:"

for current in (seq 1 $NUM_GENERATIONS)
	cat "./data/$current/stats.txt"
end
