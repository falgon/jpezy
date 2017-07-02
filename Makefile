CXXFLAGS = -Wall -Wextra -pedantic -std=c++1z

jpezy: src/main.cpp
	$(CXX) -Wall $(CXXFLAGS) -o jpezy src/main.cpp
clean:
	$(RM) jpezy
