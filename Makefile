CXX      = g++
CXXFLAGS = -O2 -std=c++17 -march=native
TARGET   = buscar2.exe
SRC      = src/buscar_nodet.cpp

$(TARGET): $(SRC) src/nq_propagate.cpp src/nq_pipeline.cpp src/nq_modes.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
