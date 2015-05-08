#
# Makefile of libsk.
#

CXX       = g++
CXXFLAGS  = -g -Wall -Werror -Wextra

HEADERS   = $(wildcard *.h)
SOURCES   = $(wildcard *.cpp)
OBJS      = $(SOURCES:%.cpp=%.o)

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
	rm -f $(TEST_EXEC) $(TEST_OBJS) $(EXEC) $(OBJS)

$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LD_LIBS)

$(TEST_EXEC): $(TEST_OBJS) $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LD_LIBS)

%.o: %.cpp
	$(CXX) $(CXX_INC) $(CXXFLAGS) -c $^ -o $@
