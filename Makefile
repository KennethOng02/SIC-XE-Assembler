all:
	g++ -std=c++11 main.cpp -o main
	./main input.txt

clean:
	rm output.txt main