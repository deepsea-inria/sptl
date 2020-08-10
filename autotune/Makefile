
PACKAGE_PATH=../../

CMDLINE_HOME=$(PACKAGE_PATH)/cmdline/include
CHUNKEDSEQ_HOME=$(PACKAGE_PATH)/chunkedseq/include
SPTL_HOME=$(PACKAGE_PATH)/sptl/include
CUSTOM_MALLOC_PREFIX=

####################################################################
# Makefile options

# Create a file called "settings.sh" in this folder if you want to
# configure particular options. See section below for options.

-include settings.sh

INCLUDE_FILES=$(wildcard $(CHUNKEDSEQ_HOME)/*.hpp) $(wildcard $(CMDLINE_HOME)/*.hpp) $(wildcard $(SPTL_HOME)/*.hpp)

INCLUDE_DIRECTIVES=-I $(CHUNKEDSEQ_HOME) -I $(CMDLINE_HOME) -I $(SPTL_HOME)

COMMON_PREFIX=-std=c++1y -O2 -march=native -DNDEBUG -DSPTL_TARGET_LINUX -Wno-subobject-linkage -lm -Wno-subobject-linkage -Wno-overflow $(CUSTOM_MALLOC_PREFIX) 

%.sptl: %.cpp $(INCLUDE_FILES)
	g++ $(COMMON_PREFIX) $(INCLUDE_DIRECTIVES) -o $@ $<

%.sptl_elision: %.cpp $(INCLUDE_FILES)
	g++ $(COMMON_PREFIX) $(INCLUDE_DIRECTIVES) -DSPTL_USE_SEQUENTIAL_ELISION_RUNTIME -o $@ $<

clean: 
	rm -rf *.sptl *.sptl_elision _results *.txt nb_cores kappa alpha
