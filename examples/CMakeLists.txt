find_package(LLVM REQUIRED)
find_package(Clang REQUIRED)

set(LOCIC "${CMAKE_BINARY_DIR}/tools/locic")

function(loci_module name output_name flags)
	set(_sources ${ARGN})
	set(_build_sources "")
	
	foreach(_file_source_name ${_sources})
		get_filename_component(_absolute_source_name "${_file_source_name}" REALPATH)
		list(APPEND _build_sources "${_absolute_source_name}")
	endforeach(_file_source_name)
	
	separate_arguments(flags)
	
	add_custom_command(OUTPUT ${output_name}
		COMMAND # Run compiler.
			${LOCIC} ${flags} -o ${output_name} ${_build_sources}
		DEPENDS ${LOCIC} ${_build_sources}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	
	add_custom_command(OUTPUT ${output_name}.unsafe.bc
		COMMAND # Run compiler.
			${LOCIC} ${flags} -o ${output_name}.unsafe.bc --unsafe ${_build_sources}
		DEPENDS ${LOCIC} ${_build_sources}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	
	add_custom_target(${name} ALL DEPENDS ${output_name} ${output_name}.unsafe.bc)
endfunction(loci_module)

set(RUNTIME_MODULE "${CMAKE_BINARY_DIR}/runtime/loci-runtime.bc")

function(loci_link name binary_name flags primary_module)
	set(${name}_Sources ${ARGN})
	
	separate_arguments(flags)
	
	add_custom_command(OUTPUT ${binary_name} ${binary_name}_optimised
		COMMAND
			# Create a file of all symbols in the primary module.
			${LLVM_NM_EXECUTABLE} -extern-only -defined-only "${primary_module}" | cut -c20- > "${primary_module}.symbols.txt"
		COMMAND
			# Create a file of all symbols in the primary module.
			${LLVM_NM_EXECUTABLE} -extern-only -defined-only "${primary_module}.unsafe.bc" | cut -c20- > "${primary_module}.unsafe.symbols.txt"
		COMMAND
			# Link bitcode files together.
			${LLVM_LINK_EXECUTABLE} "${primary_module}" ${${name}_Sources} "${RUNTIME_MODULE}" -o "${binary_name}_linked_raw.bc"
		COMMAND
			# Link bitcode files together.
			${LLVM_LINK_EXECUTABLE} "${primary_module}.unsafe.bc" ${${name}_Sources} "${RUNTIME_MODULE}" -o "${binary_name}_linked_unsafe_raw.bc"
		COMMAND
			# Internalize all dependency symbols in linked module.
			${LLVM_OPT_EXECUTABLE} -internalize -internalize-public-api-file="${primary_module}.symbols.txt" -o "${binary_name}_linked.bc" "${binary_name}_linked_raw.bc"
		COMMAND
			# Internalize all dependency symbols in linked module.
			${LLVM_OPT_EXECUTABLE} -internalize -internalize-public-api-file="${primary_module}.unsafe.symbols.txt" -o "${binary_name}_linked_unsafe.bc" "${binary_name}_linked_unsafe_raw.bc"
		COMMAND
			# Run 'opt' multiple times for maximum optimisation.
			${LLVM_OPT_EXECUTABLE} -O3 ${binary_name}_linked_unsafe.bc -o ${binary_name}_opt0.bc
		COMMAND
			${LLVM_OPT_EXECUTABLE} -O3 ${binary_name}_opt0.bc -o ${binary_name}_opt1.bc
		COMMAND
			${LLVM_OPT_EXECUTABLE} -O3 ${binary_name}_opt1.bc -o ${binary_name}_final.bc
		COMMAND
			# Disassemble for debugging.
			${LLVM_DIS_EXECUTABLE} ${binary_name}_final.bc -o ${binary_name}_final.ll
		COMMAND
			# Use 'llc' to generate non-optimised platform object file.
			${LLVM_LLC_EXECUTABLE} -filetype=obj -o ${binary_name}.o ${binary_name}_linked.bc
		COMMAND
			# Use 'llc' to generate optimised platform object file.
			${LLVM_LLC_EXECUTABLE} -filetype=obj -o ${binary_name}_optimised.o ${binary_name}_final.bc
		COMMAND
			# Use 'clang' to generate non-optimised platform binary.
			${CLANG_EXECUTABLE} -o ${binary_name} ${binary_name}.o -lm ${flags}
		COMMAND
			# Use 'clang' to generate optimised platform binary.
			${CLANG_EXECUTABLE} -s -o ${binary_name}_optimised ${binary_name}_optimised.o -lm ${flags}
		DEPENDS ${primary_module} ${primary_module}.unsafe.bc ${${name}_Sources} loci-runtime-bc
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	
	add_custom_target(${name} ALL DEPENDS ${binary_name})
endfunction(loci_link)

function(loci_example name binary_name flags)
	loci_module("${name}_Module" ${binary_name}.bc "" ${ARGN})
	loci_link("${name}" "${binary_name}" "${flags}" "${binary_name}.bc")
endfunction(loci_example)

add_subdirectory(Calculator)
add_subdirectory(ChainReversi)
add_subdirectory(CommandLine)
add_subdirectory(Events)
add_subdirectory(GUI)
add_subdirectory(Looper)
add_subdirectory(Network)
add_subdirectory(Simulations)
add_subdirectory(SplitModule)
