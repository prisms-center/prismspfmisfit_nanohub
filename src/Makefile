APPDIR = phaseField/applications/singlePrecipitate_conservedAC

all:
	cmake $(APPDIR)/CMakeLists.txt \
		-DDEAL_II_DIR=$${DEALII_HOME} \
		-DCMAKE_FIND_ROOT_PATH=/apps/share64/debian7/anaconda/anaconda2-5.1
	make -C $(APPDIR) release

install:
	. /etc/environ.sh; use -e -r phasefield-2.1.1; use -e -r cmake-3.6.3; make all
	mkdir -p ../bin
	install --mode 0755 -D $(APPDIR)/main ../bin

clean:
	rm -f $(APPDIR)/CMakeCache.txt
	rm -f $(APPDIR)/main
	rm -rf $(APPDIR)/CMakeFiles
	rm -f $(APPDIR)/Makefile
	rm -f $(APPDIR)/cmake_install.cmake

distclean: clean
	rm -f ../bin/main

.PHONY: all install clean distclean
