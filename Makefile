all:
	g++ -std=c++11 -Wall -Wextra -pedantic -I/usr/local/opt/openssl/include -o main main.cpp -lboost_system-mt -lcrypto -lssl
