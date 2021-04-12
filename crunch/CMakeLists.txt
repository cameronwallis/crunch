cmake_minimum_required(VERSION 3.18)

file(GLOB SOURCES "*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual
    -pedantic -fno-rtti -fPIE -fstack-protector-all -g
)


target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)