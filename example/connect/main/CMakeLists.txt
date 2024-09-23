
#FILE(GLOB JUICE_SOURCES "../../../libjuice/src/*.c")

idf_component_register(SRCS "espice.c" "connectivity.c"
#                    ${JUICE_SOURCES}
#                    "../../src/agent.c" "../../src/juice.c"
                    INCLUDE_DIRS "../../../include" "../../../libjuice/include" "../../../libjuice/include/juice")
#"../../../include/juice")


target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")