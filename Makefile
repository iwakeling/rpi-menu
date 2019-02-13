.DEFAULT_GOAL := help

SDL_LIBS := `pkg-config --libs SDL2_ttf`
GMOCK_LIBS := `pkg-config --libs gmock` -lgmock
GTEST_LIBS := `pkg-config --libs gtest`
FONTCONFIG_LIBS := `pkg-config --libs fontconfig`
BINARIES :=
UNITTESTS :=
HELPFRAGMENTS :=
CXXFLAGS := -Wall -Werror -pthread
LDFLAGS := -pthread
DEPS :=

ifeq ("$(strip $(RELEASE))","")
OBJDIR := debug
CXXFLAGS += -g
LDFLAGS += -g
else
OBJDIR := release
endif

ifeq ("$(strip $(VERBOSE))","")
QUIET :=@
endif

CPPFLAGS += -I$(OBJDIR) -I..

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.cpp Makefile | $(OBJDIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.d: %.cpp Makefile | $(OBJDIR)
	$(QUIET)$(CXX) -MM $(CPPFLAGS) $< | sed 's#\($*\)\.o[ :]*#$(OBJDIR)/\1.o $@ : #g' > $@

# Generate a list of object files from a list of source files
# $(1) space separated list of source files
define objects
$(addprefix $(OBJDIR)/,$(1:.cpp=.o))
endef

# Generate a list of .d files from a list of source files
# $(1) space separated list of source files
define deps
$(addprefix $(OBJDIR)/,$(1:.cpp=.d))
endef

# Generate a build target for the specified name based on the listed source files
# $(1) name of the target
# $(2) space separated list of source files
# $(3) space separated list of link options
define cmd_template
$(eval $(call cmd_template_,$(strip $(1)),$(2),$(3)))
endef

define cmd_template_
$(1): $(OBJDIR)/$(1)

$(OBJDIR)/$(1): $(call objects,$(2)) | $(OBJDIR)
	$(CXX) $(LDFLAGS) -o $$@ $(call objects,$(2)) $(3)

DEPS += $(call deps,$(2))
GENERATED_FILES += $(call objects,$(2))
BINARIES += $(OBJDIR)/$(1)
HELPFRAGMENTS += echo "  $(1): builds the $(1) command";

.PHONY: $(1)
endef

# Generate an output format flag for gtest
# $(1) target name of the test
define gtest_output_format
$(if $(filter xml,$(GTEST_FORMAT)),--gtest_output=xml:$(OBJDIR)/$(1).xml)
endef

# Generate a build target for the specified unit test based on the listed source files
# $(1) name of the target
# $(2) space separated list of source files
define unittest_template
$(eval $(call unittest_template_,$(strip $(1)),$(2)))
endef

define unittest_template_
$(1): $(OBJDIR)/$(1)
	$(OBJDIR)/$(1) $(call gtest_output_format,$(1))

$(OBJDIR)/$(1): $(call objects,$(2)) | $(OBJDIR)
	$(CXX) $(LDFLAGS) -o $$@  $(call objects,$(2)) $(GMOCK_LIBS) $(GTEST_LIBS)

DEPS += $(call deps,$(2))
GENERATED_FILES += $(call objects,$(2))
BINARIES += $(OBJDIR)/$(1)
UNITTESTS += $(1)
HELPFRAGMENTS += echo "  $(1): builds and runs the $(1) tests";

.PHONY: $(1)
endef

#######################
# The targets

$(call cmd_template, \
				rpi_menu, \
				main.cpp menu.cpp buttons.cpp, \
				$(SDL_LIBS) $(FONTCONFIG_LIBS))

help:
	@echo "make [DEBUG=1] [target]"
	@echo "  help: this help"
	@echo "  all: builds all binaries and runs all unittests"
	@echo "  test: builds and run all tests"
	@echo "  clean: removes all binaries and generated files"
	@echo
	@$(HELPFRAGMENTS)

all: $(BINARIES) $(UNITTESTS)

test: $(UNITTESTS)

clean:
	$(RM) $(BINARIES) $(GENERATED_FILES) $(DEPS)

.PHONY: help all test clean

########################
# Post amble: Must come after all targets.

# include deps files
ifneq ("$(filter-out clean help, $(MAKECMDGOALS))","")
include $(DEPS)
endif
