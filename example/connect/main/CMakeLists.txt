
FILE(GLOB JUICE_SOURCES "../../src/*.c")

idf_component_register(SRCS "espice.c" "connectivity.c" "getnameinfo.c"
                    ${JUICE_SOURCES}
#                    "../../src/agent.c" "../../src/juice.c"
                    INCLUDE_DIRS "." "../../include" "../../include/juice")
