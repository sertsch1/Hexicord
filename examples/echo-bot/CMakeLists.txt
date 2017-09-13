cmake_minimum_required(VERSION 3.0)
project(echo-bot)

add_executable(echo-bot echo-bot.cpp)
target_link_libraries(echo-bot hexicord)
