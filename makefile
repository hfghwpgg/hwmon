##### https://www.reddit.com/user/SeanRamey/ #####

CFLAGS =
CXXFLAGS = -Wpedantic -Wall -Wextra -g -std=c++17
CPPFLAGS = -DSFML
LDFLAGS = -static-libstdc++
LDLIBS =

SRC := src

cppdirs = entities util entities$(SLASH)weapons

pchfiles = 

BUILD := build

program = a


CFLAGS +=
CXXFLAGS += -I/usr/include $(addprefix -I,$(INCDIRS))
CPPFLAGS +=
LDFLAGS += -L/usr/lib
LDLIBS +=
RM := rm -f
RMDIR := rm -rf
MKDIR := mkdir -p
SLASH = /
CP := cp
PREFIX ?= /usr/local

CC := gcc
CXX := g++
CPP := g++
LD := g++


INCDIRS = $(SRC) $(addprefix $(SRC)/,$(cppdirs))
VPATH = $(INCDIRS)
cppsrc = $(wildcard $(SRC)/*.cpp $(addsuffix /*.cpp,$(INCDIRS)))
objects = $(patsubst $(SRC)/%.o,$(BUILD)/%.o,$(cppsrc:.cpp=.o))
depends = $(objects:.o=.d)
gchfiles = $(addsuffix .gch, $(pchfiles))
DESTDIR =

all: $(BUILD)/$(program)
	@echo build complete!

# build single file
single: $(BUILD)/$(in).o

$(BUILD)/$(program): $(objects)
	@$(LD) -o $@ $^ $(LDFLAGS) $(LDLIBS)
	@echo linking "$^" into "$@" using these libraries: "$(LDLIBS)"

$(objects): $(gchfiles)

$(BUILD)/%.o: %.cpp
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
# 	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@
	@echo compiling "$<" to "$@" with $(CXX)

%.hpp.gch: %.hpp
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) $<
	@echo precompiling header "$<" to "$@" with $(CXX)

$(objects) $(depends) $(gchfiles): | $(BUILD)

$(BUILD):
	@$(MKDIR) $(BUILD) $(addprefix $(BUILD)$(SLASH),$(cppdirs))
	@echo creating directories

# rule to generate a dependency file
$(BUILD)/%.d: %.cpp
	@$(CPP) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@
	@echo generating dependencies for "$<"

# include all dependency files in the makefile
-include $(depends)

.PHONY: clean install uninstall
clean:
#	$(RM) $(subst /,$(SLASH),$(objects)) $(subst /,$(SLASH),$(depends)) $(subst /,$(SLASH),$(BUILD)/$(program))
	@echo cleaning...
	@$(RMDIR) $(BUILD)
	@$(RM) $(SRC)$(SLASH)*.gch
	@echo done.


help:
	@echo Commands:
	@echo make all ---------------------------------- builds the program and puts all the output files in a directory called build
	@echo make single in="<path-to-file/file>" ------ builds only a single file (omit the .cpp or .c extension and ignore the quotes as well)
# 	@echo make install ------------------------------ builds the program just like make all and installs the final files to a directory
# 	@echo make uninstall ---------------------------- will remove all the files from the install directory
	@echo make clean -------------------------------- will remove the build directory