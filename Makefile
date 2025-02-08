build:
	gcc -o main main.c verb.c -lm 

listen: build
	./main | sox -t raw -c 2 -b 16 -e signed -r 48000 - output.wav
	sox output.wav output2.wav 
	play output2.wav

leaks: build
	valgrind --track-origins=yes --tool=memcheck ./main > /dev/null

clean:
	rm -f main output.wav output2.wav