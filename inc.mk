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

#
# includes
#
CXXINC     = -I $(LIBSK_SRC)

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
CXXFLAGS   = -g -fPIC -Wall -Werror -Wextra -rdynamic
CXXFLAGS  += -pipe -D_GNU_SOURCE -DREENTRANT
CXXFLAGS  += $(CXXINC)


%.d: %.cpp
	$(CXX) -MM $(CXXFLAGS) $< -o $@

%.o: %.cpp %.d
	$(CXX) -c $(CXXFLAGS) $< -o $@
