#pragma once

#include <boost/archive/detail/common_oarchive.hpp>
#include <boost/archive/detail/register_archive.hpp>

namespace llvm_ml {
class json_oarchive : boost::archive::detail::common_oarchive<json_oarchive> {};
} // namespace llvm_ml
