Steps to build & use:

1. Clone the repository into `<prefix>/llvm-projects/clang/tools/gather-records`.

2. Add the directory `gather-records` in <prefix>/llvm-projects/clang/tools/CMakeLists.txt

```cmke
add_clang_subdirectory(gather-records)
```

3. Build the plugin

In the building directory of llvm, run the command:

```bash
ninja gather-records
```

4. Invoke clang++ with the plugin

```shell
<llvm-build-dir>/bin/clang++ \
    -Xclang -load \
    -Xclang <llvm-build-dir>/lib/gather-records.so \
    -Xclang -plugin \
    -Xclang gather-records \
    -g -x cu \
    -std=c++17 \
    --cuda-gpu-arch=sm_50 \
    -Wno-unknown-cuda-version \
    -fsyntax-only \
    -c main.cu \
    -o main.cu.o
```
