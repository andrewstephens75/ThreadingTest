# ThreadingTest
A simple test program to show examples of different ways to create a thread safe data structure. Three different strategies are used on the same datastructure to compare performance.

# Building
ThreadingTest is a single C++ and should be easy enough to build with any compiler. It requires C++17 support for the shared_mutex test.

I have provided a CMakeLists.txt file for easy building via cmake:

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release  ..
cmake --build .  
./ThreadingTest
```
