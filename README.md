# IR Logarithmic Merging

A C++17 implementation of logarithmic merging for a dynamic inverted index. New documents are indexed in a small in-memory auxiliary index, then flushed into disk generations whose capacities double at each level. This keeps the index searchable as documents arrive while reducing the repeated rewrite cost of a single-main-index strategy.

## What is included

- `src/logarithmic.cpp` implements `LogarithmicIndex`, including flush, lookup, finalization, generation bookkeeping, and cleanup.
- `src/aux_main.cpp` is the provided auxiliary-plus-main baseline used for comparison.
- `src/merge.cpp` provides the streaming k-way merge used for all disk merges.
- `tests/test_main.cpp` checks correctness, lookup during indexing, finalization, and write cost.
- `tools/` contains the index builders, verifier, lookup tool, and corpus generator.
- `scripts/run_experiments.sh` runs the assignment's E1 and E2 experiments.

## How logarithmic merging works

`I0`, `I1`, `I2`, ... behave like bits in a binary counter. A flush starts as a new carry. If generation `Ii` is occupied, the older generation is passed first to `merge_indexes`, both inputs are removed, and the merged result carries to `I(i+1)`. The first empty generation stores the carry. A lookup combines matching postings from the in-memory index and every occupied generation. During finalization, generations are merged once in oldest-to-newest order.

The implementation uses the provided streaming merge helper and reader cache. It does not load complete on-disk indexes into memory. Temporary generation files are removed when merged or when the object is destroyed.

## Build

Requires CMake 3.14+ and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The implementation is also warning-clean with the assignment's compiler flags:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude \
  src/index_io.cpp src/memory_index.cpp src/dynamic_base.cpp \
  src/merge.cpp src/aux_main.cpp src/logarithmic.cpp \
  tests/test_main.cpp -o test_main
```

## Run tests

```bash
./build/test_main data/tiny.txt work_tiny
```

The tests compare the dynamic index with a reference index, check lookup after every document in the small collection, verify generation-mask bookkeeping, and compare the measured write growth of both policies.

## Build and verify an index

```bash
./build/gen_corpus data/corpus_50k.txt 50000
./build/build_dynamic log data/corpus_50k.txt work 100000 out/log --trace results/trace_log_100000.csv
./build/verify data/corpus_50k.txt out/log
./build/lookup out/log a bz zzz
```

The collection is one document per line. Document IDs are line numbers, starting at 1. The auxiliary-index limit `n` counts distinct `(term, document)` postings rather than bytes. Repeated occurrences of a term in one document count once.

## Experiments

Run all required parameter settings for the auxiliary-plus-main baseline and logarithmic merging:

```bash
scripts/run_experiments.sh
```

Results are written to `results/dynamic.csv` and `results/trace_*.csv`. The experiment script verifies every resulting index against the input collection. Record the machine and compiler used when interpreting runtime results; write counts and write amplification are more portable measures of merge work.

## Design notes

- Older document ranges are always the first inputs to a merge, preserving append-friendly posting-list order.
- Every merge uses `merge_indexes`; its cursor-based implementation keeps memory use proportional to the number of inputs rather than the size of the full indexes.
- `generation_mask()` exposes the occupied generation levels as a binary number.
- Reader cache entries are forgotten before their backing files are renamed or deleted.

## References

- Christopher D. Manning, Prabhakar Raghavan, and Hinrich Schütze, *Introduction to Information Retrieval*, sections 4.5 and 5.3.1, Cambridge University Press, 2008.
- The C++17 standard library documentation for containers, algorithms, and filesystem operations used by the provided framework.