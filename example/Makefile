
PACKAGE_PATH=../../
CMDLINE_HOME=$(PACKAGE_PATH)/cmdline/include
CHUNKEDSEQ_HOME=$(PACKAGE_PATH)/chunkedseq/include
SPTL_HOME=$(PACKAGE_PATH)/sptl/include

INCLUDE_FILES=$(wildcard $(CHUNKEDSEQ_HOME)/*.hpp) $(wildcard $(CMDLINE_HOME)/*.hpp) $(wildcard $(SPTL_HOME)/*.hpp)

INCLUDE_DIRECTIVES=-I $(CHUNKEDSEQ_HOME) -I $(CMDLINE_HOME) -I $(SPTL_HOME)

COMMON_PREFIX=-std=c++11 -g3 -Og -DSPTL_TARGET_LINUX -Wno-subobject-linkage

FIBRIL_PREFIX=-I/home/rainey/Scratch/fibril/include -L/home/rainey/Scratch/fibril/lib /home/rainey/Scratch/fibril/lib/libfibril.so -lm -pthread -g -fno-omit-frame-pointer -DSPTL_USE_FIBRIL -Wl,-rpath -Wl,/home/rainey/Scratch/fibril/lib -Wl,-rpath -Wl,/home/rainey/Scratch/fibril/lib

%.bin: %.cpp $(INCLUDE_FILES)
	g++ $(COMMON_PREFIX) $(INCLUDE_DIRECTIVES) $(FIBRIL_PREFIX) -o $@ $<

clean:
	rm -f *.bin
