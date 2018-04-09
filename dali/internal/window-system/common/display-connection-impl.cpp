/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
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

// INTERNAL INCLUDES
#include <dali/internal/window-system/common/display-connection-impl.h>
#include <dali/internal/window-system/common/display-connection-factory.h>

namespace Dali
{
namespace Internal
{
namespace Adaptor
{

void DisplayConnection::GetDpi(unsigned int& dpiHorizontal, unsigned int& dpiVertical)
{
  // Call static factory function
  DisplayConnectionFactoryGetDpi( dpiHorizontal, dpiVertical );
}

void DisplayConnection::GetDpi(Any nativeWindow, unsigned int& dpiHorizontal, unsigned int& dpiVertical)
{
  DisplayConnectionFactoryGetDpi( nativeWindow, dpiHorizontal, dpiVertical );
}

} // namespace Adaptor

} // namespace internal

} // namespace Dali