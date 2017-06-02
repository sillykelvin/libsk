all: client server

client: tcp_echo_client.cpp
	g++ -o $@ $^ -g -std=c++11 -I../src -I../deps/spdlog/include -L../lib -lsk -Wall
server: tcp_echo_server.cpp
	g++ -o $@ $^ -g -std=c++11 -I../src -I../deps/spdlog/include -L../lib -lsk -Wall

clean:
	rm -f client client.log server server.log
