cmake_minimum_required(VERSION 2.6)

PROJECT(melo)

SET(CMAKE_C_FLAGS "-O3 -DNDEBUG -std=gnu99 -pthread -fpic")

SET(MELO_SOURCES 
	../melo.c
	../melo_server.c
	../melo_client.c
	../melo_udp.c
	../melo_log.c
	../loge/loge.c
	../utils/automem.c
	../utils/linkhash.c
)

ADD_LIBRARY(melo ${MELO_SOURCES})

TARGET_LINK_LIBRARIES(melo uv pthread rt)
