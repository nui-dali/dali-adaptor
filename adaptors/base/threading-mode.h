#ifndef __DALI_INTERNAL_ADAPTOR_THREADING_MODE_H__
#define __DALI_INTERNAL_ADAPTOR_THREADING_MODE_H__

/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

namespace Dali
{

namespace Internal
{

namespace Adaptor
{

struct ThreadingMode
{
  enum Type
  {
    SEPARATE_UPDATE_RENDER = 0,  ///< Event, V-Sync, Update & Render on Separate threads.
    COMBINED_UPDATE_RENDER,      ///< Three threads: Event, V-Sync & a Joint Update/Render thread.
    SINGLE_THREADED,             ///< ALL functionality on the SAME thread.
  };
};

} // Adaptor

} // Internal

} // Dali

#endif // __DALI_INTERNAL_ADAPTOR_THREADING_MODE_H__