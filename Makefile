#
# Makefile of libsk.
#

CXX       = g++
CXXFLAGS  = -g -Wall -Werror -Wextra

LIB_HEADERS = $(wildcard *.h)
LIB_SOURCES = $(filter-out main.cpp, $(wildcard *.cpp))
LIB_OBJS    = $(LIB_SOURCES:%.cpp=%.o)

EXEC_SRCS = main.cpp
EXEC_OBJS = $(EXEC_SRCS:%.cpp=%.o)

TEST_DIR  = test
TEST_HDRS = $(wildcard $(TEST_DIR)/*.h)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS = $(TEST_SRCS:%.cpp=%.o)

CXX_INC   = -I .
LD_LIBS   = -lgtest -lpthread

EXEC      = sk
TEST_EXEC = sk-test


all: $(EXEC) $(TEST_EXEC)

test: $(TEST_EXEC)
	./$(TEST_EXEC)

clean:
	rm -f $(TEST_EXEC) $(TEST_OBJS) $(EXEC) $(EXEC_OBJS) $(LIB_OBJS)

$(EXEC): $(EXEC_OBJS) $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LD_LIBS)

$(TEST_EXEC): $(TEST_OBJS) $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LD_LIBS)

%.o: %.cpp
	$(CXX) $(CXX_INC) $(CXXFLAGS) -c $^ -o $@
