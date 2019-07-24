CC=g++
CFLAGS=-c -Wall -O2 -std=c++14
LDFLAGS=-pthread -lboost_program_options
SOURCES=hasher.cpp main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=hasher

# Boost library
BOOST=/usr/local/include/boost/

all: $(SOURCES) $(EXECUTABLE)
		
$(EXECUTABLE): $(OBJECTS) 
		$(CC) $(LDFLAGS) -I ${BOOST} $(OBJECTS) -o $@

.cpp.o:
		$(CC) $(CFLAGS) $< -o $@

clean:
		rm -rf *.o $(EXECUTABLE)



