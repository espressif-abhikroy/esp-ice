set(JUICE_SOURCES   libjuice/src/addr.c
                    libjuice/src/agent.c
                    libjuice/src/base64.c
                    libjuice/src/conn.c
                    libjuice/src/conn_mux.c
                    libjuice/src/conn_poll.c
                    libjuice/src/conn_thread.c
                    libjuice/src/const_time.c
                    libjuice/src/crc32.c
                    libjuice/src/hash.c
                    libjuice/src/ice.c
                    libjuice/src/juice.c
                    libjuice/src/log.c
                    libjuice/src/server.c
                    libjuice/src/stun.c
                    libjuice/src/timestamp.c
                    libjuice/src/turn.c
                    libjuice/src/udp.c
# Use hmac from mbedtls and random numbers from esp_random:
#                    libjuice/src/hmac.c
#                    libjuice/src/random.c
        )

message(INFO ${JUICE_SOURCES})
idf_component_register(SRCS port/getnameinfo.c
                            port/ifaddrs.c
                            port/juice_random.c
                            ${JUICE_SOURCES}
                       INCLUDE_DIRS "include" "libjuice/include" "libjuice/include/juice"
                       REQUIRES esp_netif)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
