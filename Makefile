CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
TARGET ?= tomasulo
SRC := src/main.cpp
TEST ?= hennessy
MODE ?=

.PHONY: all clean test one

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

test: $(TARGET)
	sh ./run_tests.sh ./$(TARGET)

one: $(TARGET)
	sh ./run_one_test.sh $(TEST) ./$(TARGET) $(MODE)

clean:
	rm -f $(TARGET) $(TARGET).exe *.o *.obj *.out *.ilk *.pdb *.idb *.ipdb *.iobj
