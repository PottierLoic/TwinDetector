all: 64bit 32bit

64bit:
	g++ -std=c++17 -o TwinDetector64Bits main.cpp -pthread

32bit:
	g++ -m32 -std=c++17 -o TwinDetector32Bits main.cpp -pthread