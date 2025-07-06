module;

#include <cstring>
#include <string>

export module gitkf:string_utils;

export template <typename T>
    requires requires(T&& t) {
        std::string {}.starts_with(t);
        strlen(t);
    }
std::string TrimLeft(std::string str, std::initializer_list<T> prefixes)
{
    for (auto s : prefixes) {
        if (str.starts_with(s)) {
            str = str.substr(strlen(s));
        }
    }
    return str;
}