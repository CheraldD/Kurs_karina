.PHONY: all doxy clean
CXX = g++
CXXFLAGS = -Wall -lboost_program_options -lcrypto++ -lboost_filesystem -fPIC
QT_CFLAGS = $(shell pkg-config --cflags Qt5Core Qt5Sql)
QT_LIBS = $(shell pkg-config --libs Qt5Core Qt5Sql)
TARGET = server
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:%.cpp=%.o)
DEPS = $(wildcard *.h)

all: $(TARGET)

doxy:
	doxygen

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(CXXFLAGS) $(QT_LIBS) -o $(TARGET)

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
