# Executables
progs       :=

# Source files that are explicitly compiled to .o
sources     :=

# Libraries/archives
libs        :=

# Other build outputs that aren't automatically collected into $(progs),
# $(objects), or $(libs)
extra_clean :=

# Test results and outputs
test_clean  :=

objects = $(subst .c,.o,$(sources))

modules := $(subst /module.mk,,$(shell find . -name module.mk))

# allow user to supplement CFLAGS (e.g. -m64), but always build with -Wall.
override CFLAGS += -Wall

all:

include $(addsuffix /module.mk,$(modules))

progs += ns_exec

.PHONY: all
all: $(progs)

.PHONY: libs
libs: $(libs)

.PHONY: clean
clean:
	$(RM) $(objects) $(progs) $(libs) $(extra_clean)

.PHONY: testclean
testclean:
	$(RM) -r $(test_clean)
