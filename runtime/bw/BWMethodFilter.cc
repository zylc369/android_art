/* Copyright 2016 buwai */

#include "stdafx.h"
#include "BWMethodFilter.h"

namespace art {

// 判断是否过滤方法。
bool BWMethodFilter::IsFilter(mirror::ArtMethod* method) {
  return false;
}

}  // namespace art
