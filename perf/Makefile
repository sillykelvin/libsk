#
# Makefile of the performance test.
#

LIBSK_HOME = ..

include ../inc.mk

SUB_MODULES = .
APP_BIN     = sk
CXXFILE     = $(foreach dir, $(SUB_MODULES), $(wildcard $(dir)/*.cpp))
DEP         = $(CXXFILE:.cpp=.d)
OBJ         = $(CXXFILE:.cpp=.o)

.PHONY: all clean

all: $(APP_BIN)

$(APP_BIN): $(DEP) $(OBJ) $(LIBS)
	$(CXX) -o $@ $(CXXFLAGS) $(OBJ) $(LIBS)

clean:
	$(RM) $(OBJ) $(DEP) $(APP_BIN)
