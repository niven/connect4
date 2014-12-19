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
A quite fancy (if I say so myself) index that is a B+ Tree index that is persistent on disk, but efficiently cached in memory and also very, very fast.

And todo many things so I though it was time to throw this onto github since it has turned into a medium sized software project (You can tell easily: you've started to worry about multiple files and have implemented a binary search)

# Overview

the main program takes a board, generates new boards (with associated data)
and then stores the boards in the database.

The database can be queried to see if a board is alrewady there, and it's on disk and stuff so we can use
a database of boards to generate the next generation of boards and eventually end up with a DAG that has all possible
board states in it, which is a pretty good result.

Since there is a database of board states for every generation we can run things independantly. This makes testing and tweaking easier.

When we actually have these databases we can also do a pruning step to arrive at (yet another) database of moves that play the perfect game. If there are mutliple branches in boardspace that lead to a win for white (which we'll assume is there since the game is solved) we can prune the branch with the largest amount of moves. All cool.

Now let's discuss the database.
Here we store a board. The way this happens is to create a unique key from the board state (encode as a board63) and store that in our on-disk binary+ tree. The associated data we store in another file.

The bpt is nice and balanced and makes searches easy (and fast, since we do billions of these this is important). The data is stored in a table file where we keep winlines around etc.

# Finding the smallest db to play the perfect game


So I think work this backwards:
Start with the last generation (42). These are all draws or wins for Black. (since the first wins for White are at move 7, so White only wins on odd moves, Black on even moves)

Definition: a board can be Good or Bad: Good is a win for White, Bad is a draw or Win for Black.

From the last generation, remove all boards that are Bad (this should be all of them, but good to check)
Go to the previous generation

So here we are at g41, Black moves. We don't care since we will allow Black to make any move they want (so even assume Black could make a draw, or just not take a possible win)

So go back one more gen g40, White's move.

Take each board, and generate (yes, again) and check for the presence of those in the next gen. Now there are some outcomes:
all Good (aka, all there)
all Bad (aka, none there)
some Good, some Bad
Now we essentially always go for a win, so any outcome that is a win is cool.
Record all board states + the move that delivers a win for White.






