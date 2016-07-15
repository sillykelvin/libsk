#
# Common variables.
#

#
# directories
#
LIBSK_SRC  = $(LIBSK_HOME)/src
LIBSK_PERF = $(LIBSK_HOME)/perf
LIBSK_TEST = $(LIBSK_HOME)/test
LIBSK_LIB  = $(LIBSK_HOME)/lib
LIBSK_DEPS = $(LIBSK_HOME)/deps

#
# includes
#
CXXINC     = -I $(LIBSK_SRC)
CXXINC    += -I $(LIBSK_DEPS)/pugixml/include
CXXINC    += -I $(LIBSK_DEPS)/spdlog/include

#
# libs
#
LIBS       = $(LIBSK_LIB)/libsk.a
LIBS      += -lgtest -lpthread

#
# compilers
#
CXX        = g++
RM         = /bin/rm -rvf
CXXFLAGS   = -g -std=c++0x -fPIC -Wall -Werror -Wextra -rdynamic
CXXFLAGS  += -pipe -D_GNU_SOURCE -DREENTRANT
CXXFLAGS  += $(CXXINC)

debug = true
ifeq ($(debug), true)
CXXFLAGS  += -DDEBUG
endif


%.d: %.cpp
	$(CXX) -MM $(CXXFLAGS) $< -o $@

%.o: %.cpp %.d
	$(CXX) -c $(CXXFLAGS) $< -o $@
