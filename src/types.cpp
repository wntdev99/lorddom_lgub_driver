// SPDX-License-Identifier: MIT
#include "lorddom_lgub/types.hpp"

namespace lorddom {

const char* to_string(Status s) {
  switch (s) {
    case Status::Ok: return "Ok";
    case Status::NotOpen: return "NotOpen";
    case Status::SerialError: return "SerialError";
    case Status::Timeout: return "Timeout";
    case Status::CrcError: return "CrcError";
    case Status::ExceptionResponse: return "ExceptionResponse";
    case Status::InvalidResponse: return "InvalidResponse";
    case Status::InvalidArgument: return "InvalidArgument";
    case Status::NoTarget: return "NoTarget";
    case Status::OutOfRange: return "OutOfRange";
  }
  return "Unknown";
}

}  // namespace lorddom
