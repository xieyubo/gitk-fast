module;

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

export module gitkf:line_reader;

export class LineReader {
public:
    void Append(const char* buffer, size_t num) { m_buffer.insert(m_buffer.end(), buffer, buffer + num); }

    std::optional<std::string> GetLine()
    {
        auto it = std::find(m_buffer.begin(), m_buffer.end(), '\n');
        if (it == m_buffer.end()) {
            return {};
        }
        auto res = std::string { m_buffer.begin(), it };
        m_buffer.erase(m_buffer.begin(), it + 1);
        return res;
    }

    std::string GetAll()
    {
        auto res = std::string { m_buffer.begin(), m_buffer.end() };
        m_buffer.clear();
        return res;
    }

private:
    std::vector<char> m_buffer {};
};