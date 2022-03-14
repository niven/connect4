Because I keep forgetting.

Build for 32bit: make ARCH=32

Create a new empty board:

  echo -n -e '\x0\x0\x0\x0\x0\x0\x0\x0' > boards


Run the script that drives the whole thing. Start with . (dot) to execute in current shell
N is the number of generations to produce
. run.fish N

