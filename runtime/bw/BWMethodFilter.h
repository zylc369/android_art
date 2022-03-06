/* Copyright 2016 buwai */

#ifndef ART_RUNTIME_BW_BWMETHODFILTER_H_
#define ART_RUNTIME_BW_BWMETHODFILTER_H_

#include "base/macros.h"
#include "base/mutex.h"
#include "dex_file.h"
#include <BWNativeHelper/BWCommon.h>

namespace art {

class BWMethodFilter {
 public:
    /**
     * 判断是否过滤方法。
     * @param[in] method 方法。
     * @return 若过滤该方法，则返回true；否则，返回false。
     */
    bool IsFilter(mirror::ArtMethod* method);
};

}  // namespace art

#endif  // ART_RUNTIME_BW_BWMETHODFILTER_H_
