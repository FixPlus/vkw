#ifndef VKRENDERER_EXCEPTION_HPP
#define VKRENDERER_EXCEPTION_HPP

#include <vkw/Containers.hpp>

#include <concepts>
#include <exception>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace vkw {

class Error : public std::runtime_error {
public:
  explicit Error(std::string_view what) noexcept
      : std::runtime_error(std::string(what)) {}

  virtual std::string_view codeString() const noexcept = 0;
};

class LogicError : public Error {
public:
  LogicError(std::string_view what) noexcept : Error(what) {}

  std::string_view codeString() const noexcept override {
    return "logic error";
  }
};

class PositionalError : public Error {
public:
  PositionalError(std::string_view what, std::string_view filename,
                  uint32_t line) noexcept
      : Error([&]() {
          std::stringstream ss;
          ss << what << "\n in file " << filename << " on line "
             << std::to_string(line);
          return std::move(ss).str();
        }()),
        m_filename(filename), m_line(line) {}

  std::string_view filename() const noexcept { return m_filename; };
  uint32_t line() const noexcept { return m_line; };

private:
  std::string m_filename;
  uint32_t m_line;
};

#ifdef VKW_ENABLE_EXCEPTIONS
static constexpr bool ExceptionsDisabled = false;
#else
static constexpr bool ExceptionsDisabled = true;
#endif

namespace __detail {
inline cntr::vector<std::function<void(const Error &)>, 10>
    irrecoverableCallbacks;
}

// In some cases exception cannot be thrown(e.g. in destructor or move
// constructors). irrecoverableError() is called then which will eventually call
// std::terminate. Application might want to do something at that point(e.g.
// display diagnostics) For that it can add its own callbacks via
// addIrrecoverableErrorCallback()
template <std::derived_from<Error> ErrT>
[[noreturn]] void irrecoverableError(const ErrT &e) noexcept {
  for (auto &callback : __detail::irrecoverableCallbacks)
    callback(e);

  std::terminate();
}
inline void addIrrecoverableErrorCallback(auto &&callback) {
  __detail::irrecoverableCallbacks.emplace_back(
      std::forward<decltype(callback)>(callback));
}

// postError may do two things in accordance to how project was configured
// If project was configured with VKW_ENABLE_EXCEPTIONS=On then it
// will throw instance of e. Otherwise, it will call irrecoverableError(e)
template <std::derived_from<Error> ErrT>
[[noreturn]] inline void postError(ErrT &&e) {
#ifdef VKW_ENABLE_EXCEPTIONS
  throw std::forward<decltype(e)>(e);
#else
  irrecoverableError(std::forward<decltype(e)>(e));
#endif
}

} // namespace vkw
#endif // VKRENDERER_EXCEPTION_HPP
