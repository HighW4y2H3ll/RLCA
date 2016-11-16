
#gcc -c mmap.c -w
#gcc -c ptraceutil.c -w
#gcc -c ptracemain.c -w
#
#gcc -o test ptracemain.o ptraceutil.o mmap.o -w
cmake -DCMAKE_TOOLCHAIN_FILE=../../Android/android-cmake/android.toolchain.cmake -DANDROID_NDK=../../Android/android-ndk-r12 -DCMAKE_BUILD_TYPE=Debug -DANDROID_ABI="armeabi-v7a" -DANDROID_NATIVE_API_LEVEL=19 -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-4.9 ./
