CC ?= gcc
CFLAGS ?= -O3 -fopenmp -Wall -Wextra -std=c11 -Iengine/include -D_POSIX_C_SOURCE=200809L
LDFLAGS ?= -fopenmp -lm

SRCDIR = engine/src
INCDIR = engine/include
BINDIR = engine/bin
BUILDDIR = engine/build

OBJS = $(BUILDDIR)/graph.o $(BUILDDIR)/dijkstra_seq.o $(BUILDDIR)/dijkstra_omp.o $(BUILDDIR)/delta_stepping.o $(BUILDDIR)/subgraph.o

TARGETS = $(BINDIR)/test_correctness $(BINDIR)/bench_runner $(BINDIR)/engine_server

.PHONY: all clean test bench server sanitizers data

all: $(TARGETS)

data:
	./scripts/download_dataset.sh

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/test_correctness: engine/tests/test_correctness.c $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

$(BINDIR)/bench_runner: $(SRCDIR)/bench_main.c $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

$(BINDIR)/engine_server: $(SRCDIR)/server_main.c $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

test: $(BINDIR)/test_correctness
	$(BINDIR)/test_correctness

bench: $(BINDIR)/bench_runner
	$(BINDIR)/bench_runner engine/data/USA-road-d.BAY.gr

sanitizers: CFLAGS = -O1 -g -fsanitize=address,undefined -fopenmp -Wall -Wextra -std=c11 -Iengine/include -D_POSIX_C_SOURCE=200809L
sanitizers: LDFLAGS = -fsanitize=address,undefined -fopenmp -lm
sanitizers: clean $(BINDIR)/test_correctness
	$(BINDIR)/test_correctness

clean:
	rm -rf $(BUILDDIR) $(BINDIR)
