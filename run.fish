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

	./bin/bfcf "./data/$prev/boards" "./data/$current"
	./bin/merge "./data/$current"

end

echo -e "\nResults:"

for current in (seq 1 $NUM_GENERATIONS)
	echo "Generation $current"
	cat "./data/$current/stats.txt"
	ls -l ./data/$current/0000000000000000.block | awk '{print "Boards in first block " $5/8}'
	ls -l ./data/$current/boards | awk '{print "Total unique boards " $5/8}'
	echo
end
