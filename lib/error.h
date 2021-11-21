// Copyright 2021 Google LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef ERROR_H
#define ERROR_H

#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include <zip.h>

#include "log.h"

// An exception carrying a libzip error code.
class ZipError : public std::runtime_error {
 public:
  ZipError(std::string message, zip_t* const archive)
      : std::runtime_error(MakeMessage(std::move(message), archive)),
        code_(zip_error_code_zip(zip_get_error(archive))) {}

  ZipError(std::string message, zip_file_t* const file)
      : std::runtime_error(MakeMessage(std::move(message), file)),
        code_(zip_error_code_zip(zip_file_get_error(file))) {}

  ZipError(std::string message, const int code)
      : std::runtime_error(MakeMessage(std::move(message), code)),
        code_(code) {}

  // Gets the libzip error code.
  int code() const { return code_; }

 private:
  static std::string MakeMessage(std::string message, zip_t* const archive) {
    message += ": ";
    message += zip_strerror(archive);
    return message;
  }

  static std::string MakeMessage(std::string message, zip_file_t* const file) {
    message += ": ";
    message += zip_file_strerror(file);
    return message;
  }

  static std::string MakeMessage(std::string message, const int code) {
    message += ": ";
    zip_error_t ze;
    zip_error_init_with_code(&ze, code);
    message += zip_error_strerror(&ze);
    zip_error_fini(&ze);
    return message;
  }

  // libzip error code
  const int code_;
};

// Throws an std::system_error with the current errno.
template <typename... Args>
[[noreturn]] void ThrowSystemError(Args&&... args) {
  const int err = errno;
  throw std::system_error(err, std::system_category(),
                          StrCat(std::forward<Args>(args)...));
}

#endif  // ERROR_H
