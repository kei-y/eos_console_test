#pragma once

#include <chrono>

/// @brief タイムアウト判定
/// @param old 以前の時間
/// @param ms タイムアウトと判断する時間、ミリ秒指定
/// @return true タイムアウト
static bool IsTimeout(const std::chrono::system_clock::time_point& old, int ms)
{
    std::chrono::duration<double, std::milli> elapsed = std::chrono::system_clock::now() - old;

    return elapsed.count() > ms;
}
