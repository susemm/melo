cmake_minimum_required(VERSION 2.6)

PROJECT(melo_test)

SET(CMAKE_C_FLAGS "-g -std=gnu99 -pthread -fpic")

INCLUDE_DIRECTORIES(
	../../../libuv-v1.8.0/include
)

SET(SERVER_SOURCES
	../server.c
	../../melo.c
	../../melo_server.c
	../../utils/automem.c
	../../utils/linkhash.c
)

ADD_EXECUTABLE(server ${SERVER_SOURCES})
TARGET_LINK_LIBRARIES(server uv pthread rt)


SET(CLIENT_SOURCES
	../client.c
	../../melo.c
	../../melo_client.c
	../../utils/automem.c
	../../utils/linkhash.c
)

ADD_EXECUTABLE(client ${CLIENT_SOURCES})
TARGET_LINK_LIBRARIES(client uv pthread rt)


SET(UDPECHO_SOURCES
	../udp-echo.c
	../../melo.c
	../../melo_udp.c
	../../utils/automem.c
	../../utils/linkhash.c
)

ADD_EXECUTABLE(udpecho ${UDPECHO_SOURCES})
TARGET_LINK_LIBRARIES(udpecho uv pthread rt)

SET(LOGC_SOURCES
	../log-client.c
	../../melo.c
	../../melo_log.c
	../../melo_udp.c
	../../loge/loge.c
	../../utils/automem.c
	../../utils/linkhash.c
)

ADD_EXECUTABLE(logc ${LOGC_SOURCES})
TARGET_LINK_LIBRARIES(logc uv pthread rt)

SET(LOGS_SOURCES
	../log-server.c
	../../melo.c
	../../melo_udp.c
	../../loge/loge.c
	../../utils/automem.c
	../../utils/linkhash.c
)

ADD_EXECUTABLE(logs ${LOGS_SOURCES})
TARGET_LINK_LIBRARIES(logs uv pthread rt)
