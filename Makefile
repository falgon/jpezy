BIN = jpezy
CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++1z
MAIN = src/main.cpp

jpezy: $(MAIN)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(MAIN)
clean:
	$(RM) $(BIN)
