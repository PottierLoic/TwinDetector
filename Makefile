64bit:
	g++ -std=c++17 -o mac_checker_64 main.cpp -pthread

32bit:
	g++ -m32 -std=c++17 -o mac_checker_32 main.cpp -pthread