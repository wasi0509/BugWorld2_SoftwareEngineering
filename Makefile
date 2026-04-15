CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

all: client

client: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o client -lpthread

qt: bugworld.pro
	mkdir -p build_qt && cd build_qt && qmake ../bugworld.pro && $(MAKE)
	cp build_qt/bugworld .

test: test_client
	./test_client

test_client: client.cpp test_client.cpp parser.cpp
	$(CXX) $(CXXFLAGS) -DUNIT_TEST client.cpp parser.cpp test_client.cpp -o test_client -lpthread
	
clean:
	rm -f client test_client bugworld
	rm -rf build_qt
