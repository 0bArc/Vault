cmake_minimum_required(VERSION 3.16)
project(vaultc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(vaultc
    src/compiler.cpp
    src/lexer.cpp
    src/parser.cpp
    src/interpreter.cpp
    src/crypto.cpp
)

target_include_directories(vaultc PRIVATE src)

if (WIN32)
    target_link_libraries(vaultc PRIVATE bcrypt)
endif()

if (MSVC)
    target_compile_options(vaultc PRIVATE /W4 /permissive-)
else()
    target_compile_options(vaultc PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(vaultdepend
    src/utils/depend.cpp
)

target_include_directories(vaultdepend PRIVATE src)

if (MSVC)
    target_compile_options(vaultdepend PRIVATE /W4 /permissive-)
else()
    target_compile_options(vaultdepend PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(vault
    src/vault_app.cpp
    src/lexer.cpp
    src/parser.cpp
    src/interpreter.cpp
    src/crypto.cpp
    src/compiler.cpp
)

target_include_directories(vault PRIVATE src)

target_compile_definitions(vault PRIVATE VAULT_NO_MAIN)

if (WIN32)
    target_link_libraries(vault PRIVATE bcrypt)
endif()

if (MSVC)
    target_compile_options(vault PRIVATE /W4 /permissive-)
else()
    target_compile_options(vault PRIVATE -Wall -Wextra -Wpedantic)
endif()
