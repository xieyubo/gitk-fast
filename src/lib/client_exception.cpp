module;

#include <stdexcept>
#include <string>

export module gitkf:client_exception;

export class ClientException : public std::runtime_error {
public:
    ClientException(int statusCode, std::string message)
        : std::runtime_error { std::move(message) }
        , m_statusCode { statusCode }
    {
    }

    int StatusCode() const { return m_statusCode; }

private:
    int m_statusCode { 500 };
};