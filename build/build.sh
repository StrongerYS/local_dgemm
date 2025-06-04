find . -mindepth 1 ! -name "build.sh" -exec rm -rf {} +

# cmake ..
cmake -DCMAKE_BUILD_TYPE=Release ..

make -j8

# ctest --output-on-failure

# ./tests/apc_local_dgemm_test