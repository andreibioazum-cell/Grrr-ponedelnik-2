cmake_minimum_required(VERSION 3.16)
project(Game3D LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(game3d
    src/world.c
    src/player.c
    src/render.c
    src/game.c
    src/main.c
)
target_include_directories(game3d PRIVATE src)
target_link_libraries(game3d PRIVATE m)

enable_testing()
add_executable(test_game3d
    src/world.c
    src/player.c
    src/render.c
    src/game.c
    test/test_game3d.c
)
target_include_directories(test_game3d PRIVATE src)
target_link_libraries(test_game3d PRIVATE m)
add_test(NAME game3d COMMAND test_game3d)
