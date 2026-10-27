# ic3 -engine bitwuzla links Bitwuzla's C API when it is found (optional)
find_path(BITWUZLA_INCLUDE_DIR bitwuzla/c/bitwuzla.h)
find_library(BITWUZLA_LIBRARY bitwuzla)
if (BITWUZLA_INCLUDE_DIR AND BITWUZLA_LIBRARY AND NOT YOSYS_WITHOUT_BITWUZLA)
	message(STATUS "ic3: Bitwuzla found: ${BITWUZLA_LIBRARY}")
	yosys_pass(ic3
		ic3.cc
		aiger_cert.cc
		smt_bitwuzla.cc
		DEFINITIONS YOSYS_ENABLE_BITWUZLA
		INCLUDE_DIRS ${BITWUZLA_INCLUDE_DIR}
		LIBRARIES ${BITWUZLA_LIBRARY}
	)
else()
	yosys_pass(ic3
		ic3.cc
		aiger_cert.cc
		smt_bitwuzla.cc
	)
endif()
