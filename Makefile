OUTPUT=main

build:
	gcc -Isrc -o $(OUTPUT) src/*.c -lavcodec -lavformat -lavutil -g

lint: build
	valgrind --tool=memcheck --leak-check=full --leak-resolution=med --track-origins=yes --show-leak-kinds=all --suppressions=testmem.supp ./$(OUTPUT)
