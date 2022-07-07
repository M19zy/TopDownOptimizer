mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))

CC := clang++
INCLUDE := -Ior-tools/include/
LIB := -L$(mkfile_dir)/or-tools/lib/ -lortools
CFLAGS := -std=c++20 -O2

target: LoadFile.o GenericJoin.o Relation.o Optimizer.o Estimator.o Plan.o
	$(CC) $(CFLAGS) LoadFile.o GenericJoin.o Relation.o Estimator.o Optimizer.o Plan.o main.cc $(LIB) -o main

testLarge: Optimizer.o optest.cc Relation.o Estimator.o
	$(CC) $(CFLAGS) Optimizer.o Relation.o Estimator.o optest.cc -lstdc++fs $(LIB) -o testLarge

LoadFile.o: LoadFile.cc
	$(CC) $(CFLAGS) -c LoadFile.cc -o LoadFile.o

GenericJoin.o: GenericJoin.cc
	$(CC) $(CFLAGS) -c GenericJoin.cc -o GenericJoin.o

Relation.o: Relation.cc
	$(CC) $(CFLAGS) -c Relation.cc -o Relation.o

Optimizer.o: Optimizer.cc
	$(CC) $(CFLAGS) $(INCLUDE) -c Optimizer.cc -o Optimizer.o

Estimator.o: Estimator.cc
	$(CC) $(CFLAGS) $(INCLUDE) -c Estimator.cc -o Estimator.o

Plan.o: Plan.cc
	$(CC) $(CFLAGS) $(INCLUDE) -c Plan.cc -o Plan.o

clean:
	rm -f *.o main