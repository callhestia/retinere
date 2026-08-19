CXX = g++
CXXFLAGS = -std=c++17 -O2 -I src $(shell pkg-config --cflags webkit2gtk-4.1)
LDFLAGS = $(shell pkg-config --libs webkit2gtk-4.1)

BUILD_DIR = build
ALL_SRC = $(shell find . -type f -name "*.cpp")
SRC = $(filter-out ./tests/test_engine.cpp ./src/main.cpp, $(ALL_SRC))
OBJ = $(patsubst ./%.cpp, $(BUILD_DIR)/%.o, $(SRC))
TARGET = ZoraFisz

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: ./%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)