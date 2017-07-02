CXXFLAGS = -Wall -Wextra -pedantic -std=c++1z

jpezy: main.cpp
	$(CXX) -Wall $(CXXFLAGS) -o jpezy src/main.cpp
clean:
	$(RM) jpezy
