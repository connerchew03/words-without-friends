# Words Without Friends

This is a program I created for my System Fundamentals (ICSI 333) class at the University at Albany. It includes three files:
- *2of12.txt*: A list of words that will be used to generate games.
- *webclient.c*: Enables players to clean up when they are done playing.
- *words.c*: Contains all the code responsible for generating, playing, and cleaning up games.

## How to play
1. Download all three files and put them in the same directory.
2. Compile *words.c* and name the output file *words*:
```
gcc words.c -o words
```
3. Run the compiled *words* and pass your working directory as an argument:
```
./words <pwd>
```
4. Open the web browser of your choice and type *127.0.0.1:8000/words* in the address bar.
5. You are now playing Words Without Friends in your browser! If at any point you want to reset the game, simply repeat the second half of step 4.

NOTE: The program will not run if the output file is named anything other than *words*, since the code depends on the file having that name in order to set up the game.

## How to clean up
1. Compile *webclient.c* and name the output file *webclient*:
```
gcc webclient.c -o webclient
```
2. Run the compiled *webclient*:
```
./webclient
```
3. When prompted for a command, type the word *QUIT* in all capital letters.

NOTE: The localhost may or may not shut down right away. If it doesn't, you'll need to update the Words Without Friends game, either by guessing another word or by resetting the game, in order to shut it down.
