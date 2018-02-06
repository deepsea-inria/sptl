
PACKAGE_PATH=../../
CMDLINE_HOME=$(PACKAGE_PATH)/cmdline/include
CHUNKEDSEQ_HOME=$(PACKAGE_PATH)/chunkedseq/include
SPTL_HOME=$(PACKAGE_PATH)/sptl/include

INCLUDE_FILES=$(wildcard $(CHUNKEDSEQ_HOME)/*.hpp) $(wildcard $(CMDLINE_HOME)/*.hpp) $(wildcard $(SPTL_HOME)/*.hpp)

INCLUDE_DIRECTIVES=-I $(CHUNKEDSEQ_HOME) -I $(CMDLINE_HOME) -I $(SPTL_HOME)

COMMON_PREFIX=-std=c++11 -g3 -Og -DTARGET_LINUX -Wno-subobject-linkage

%.bin: %.cpp $(INCLUDE_FILES)
	g++ $(COMMON_PREFIX) $(INCLUDE_DIRECTIVES) -o $@ $<

clean:
	rm -f *.bin
