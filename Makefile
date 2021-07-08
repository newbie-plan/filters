MAIN := main
SRC := ./src/*.c
SRC += ./src/base/*.c
SRC += ./src/bctoolbox/*.c
SRC += ./src/ortp/*.c

INC := -I ./include/
INC += -I /usr/local/ffmpeg/include/
LIB := -L /usr/local/ffmpeg/lib/ -lavcodec -lavformat -lavutil -lswresample -lswscale -lavfilter
LIB += -lpthread

${MAIN} : ${SRC}
	gcc $^ -o $@ ${INC} ${LIB}
clean:
	rm ${MAIN}
