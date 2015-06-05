#
# Makefile of the library.
#

LIBSK_HOME = ..

include ../inc.mk

SUB_MODULES = . detail
OUT_LIB     = $(LIBSK_LIB)/libsk.a
CXXFILE     = $(foreach dir, $(SUB_MODULES), $(wildcard $(dir)/*.cpp))
DEP         = $(CXXFILE:.cpp=.d)
OBJ         = $(CXXFILE:.cpp=.o)

.PHONY: all clean

all: $(OUT_LIB)

$(OUT_LIB): $(DEP) $(OBJ)
	ar rvs $@ $(OBJ)

clean:
	$(RM) $(OBJ) $(DEP) $(OUT_LIB)

-include $(DEP)
