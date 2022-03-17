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

	set destination_directory ./data/$current
	if test $current -lt 10
		set destination_directory ./data/0$current
	end

	if test ! -d $destination_directory
		mkdir $destination_directory
	end

	set prev (math $current - 1)
	set source_directory ./data/$prev
	if test $prev -lt 10
		set source_directory ./data/0$prev
	end


	echo "########################## Generation $current ##########################"
	echo "Reading from $source_directory and writing to $destination_directory"

	if test -e $destination_directory/boards
		echo "Already completed"
		continue
	end

	./bin/bfcf $source_directory/boards $destination_directory
	./bin/merge $destination_directory
	rm -f $destination_directory/*.block

	echo
end

echo -e "\nResults:"

# . stats.fish $NUM_GENERATIONS
