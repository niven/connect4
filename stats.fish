#!/usr/bin/fish


for current in (seq 1 42)

	set directory ./data/$current
	if test $current -lt 10
		set directory ./data/0$current
	end

    if test ! -d $directory
        break;
    end

	echo "Generation $current"
	cat $directory/stats.txt
	echo
end

echo -e "\nSize report:"
du -h data/ | sort -k 2
