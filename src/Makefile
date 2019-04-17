all:


install:
	echo 'n' | /etc/environ sh use phasefield-2.1.1
	echo 'n' | /etc/environ sh use visit-2.12.2
	cmake phaseField/applications/singlePrecipitate_conservedAC/CMakeLists.txt -DDEAL_II_DIR=/apps/share64/debian7/dealii/dealii-9.0.1
	bash install.sh

clean:
	rm phaseField/applications/singlePrecipitate_conservedAC/CMakeCache.txt
	rm phaseField/applications/singlePrecipitate_conservedAC/main

distclean: clean

.PHONY: all install clean distclean
