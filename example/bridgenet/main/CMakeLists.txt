idf_component_register(SRCS "espice.c" "connectivity.c")

if(CLIENT1)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "CLIENT1")
elseif(CLIENT2)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "CLIENT2")
endif()
