#include "core/exception_words.hpp"

#include <cxxabi.h>

#include <cstdlib>
#include <typeinfo>

namespace uagfx {
namespace {

std::string demangled(const std::type_info* type) {
    if (!type) return "?";
    int status = 0;
    char* name = abi::__cxa_demangle(type->name(), nullptr, nullptr, &status);
    std::string out = status == 0 && name ? name : type->name();
    std::free(name);
    return out;
}

}  // namespace

std::string exceptionWords(std::exception_ptr e) {
    if (!e) return "no exception (std::terminate called by itself)";
    try {
        std::rethrow_exception(e);
    } catch (const std::exception& x) {
        return demangled(&typeid(x)) + ": " + x.what();
    } catch (...) {
        return "an exception of type " + demangled(abi::__cxa_current_exception_type());
    }
}

}  // namespace uagfx
