server: server.cpp
	g++ -std=c++14 -Ofast -pthread server.cpp -o server

run: server
	./server