#ifndef DALI_GRAPHICS_VULKAN_DEBUG_H
#define DALI_GRAPHICS_VULKAN_DEBUG_H

/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
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
 */

// Don't do this - it can change struct sizes. E.g. graphics-api-render-command.h
// #undef DEBUG_ENABLED

#include <queue>
#include <string>
#include <sstream>

#if defined(DEBUG_ENABLED)

#include <iostream>
#include <cstdarg>
#include <string>

extern const char* LOG_VULKAN;


std::string ArgListToString( const char* format, va_list args );

std::string FormatToString( const char* format, ... );

#define DALI_LOG_STREAM( filter, level, stream )  \
  if( nullptr != LOG_VULKAN)                      \
  {                                               \
    std::cout << stream << std::endl;             \
  }

#define DALI_LOG_INFO( filter, level, format, ... ) \
  if( nullptr != LOG_VULKAN )                       \
  {                                                 \
    std::cout << FormatToString( (format), ##__VA_ARGS__ ); \
  }

#else
#define DALI_LOG_STREAM( filter, level, stream )
#define DALI_LOG_INFO( filter, level, format, ... )
#endif

namespace Dali
{
namespace Graphics
{
namespace Vulkan
{

/**
 * Helper function converting Vulkan C++ types into void* ( for logging )
 */
template<class T, class K>
const void* VkVoidCast( const K& o )
{
  return static_cast<T>(o);
}

#ifdef DEBUG_ENABLED
struct BlackBox
{
  std::queue<std::string> debugLog;

  template<class T>
  BlackBox& operator <<(const T& value )
  {
    stream << value;
    return *this;
  }

  BlackBox& log()
  {
    stream.str(std::string());
    return *this;
  }

  const std::string& end()
  {
    static const std::string terminator("");
    debugLog.push( stream.str() );
    if( debugLog.size() > 128 )
    {
      debugLog.pop();
    }
    return terminator;
  }

  void push();

  static void pop();

  static BlackBox& get();

  std::stringstream stream;
};
#endif


} // Namespace Vulkan

} // Namespace Graphics

} // Namespace Dali


#endif // DALI_GRAPHICS_VULKAN_DEBUG_H
