connect4
========

I was reading "A Knowledge-based Approach of Connect-Four The Game is Solved: White Wins - Victor Allis"
http://www.informatik.uni-trier.de/~fernau/DSL0607/Masterthesis-Viergewinnt.pdf

Again for some reason, and I noted in the introduction:

"For the 7 x 6 board, this means that we must be able to hold
all positions of 36 and 37 men at the same time, a total of over 1,6.1013 positions. Appendix C contains
a table for the number of different positions for each number of men. We can store the value of
a position in 2 bits, since we have 4 possible states: win for White, win for Black, draw or not
checked (we can use the address of the 2 bits as identification for the position). This way we need at
least 4 Terabyte. Therefore making a database does not seem realistic yet."

And then I thought, "I have around a TB in my machine, so if I can make the board smaller to encode I could jsut bruteforce the whole thing!"

So then it spiralled out of control^W^W^W^W went like this:
- I could generate all possible boards for this game, no more upper limit, that would be cool.
- I can store this board in less that 2 bits per square (in fact, at most 63 bits) because I like cramming stuff into bits
- Having thought of a nice way to keep track of winlines on a board (with more bits) I could easily generate next board states and not have crappy for-loop code to check for wins
- Then I can generate all possible boards, but after 9 moves I was already at 800MB
- So then I thought:

I need to store all unique boards in something I'll call a "table", but it's going to be huge so it has to be stored on disk.
But searching millions of records in this "table" would be slow, so I need some other thing I can search quickly, like a B+ Tree. That is also going to be rather large, so it also has to be stored on disk. I'll call this file an "index".

So now I'm implementing a rudimentary thing I call a "database".

Anyway, source code so far:
encoding/decoding/writing/loading boards+winlines.
generating new boards from a move sequence, visualizing boards in amazing 2D ASCII art.
a shitty B+ Tree implementation that is broken in interesting ways.

And todo many things so I though it was time to throw this onto github since it has turned into a medium sized software project (You can tell easily: you've started to worry about multiple files and have implemented a binary search)


