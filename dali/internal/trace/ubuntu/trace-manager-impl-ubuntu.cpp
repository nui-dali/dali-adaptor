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

// EXTERNAL INCLUDES
#include <dali/internal/trace/ubuntu/trace-manager-impl-ubuntu.h>
#include <dali/internal/system/common/performance-interface.h>

// INTERNAL INCLUDES

namespace Dali
{


namespace Internal
{

namespace Adaptor
{

TraceManagerUbuntu* TraceManagerUbuntu::traceManagerUbuntu = nullptr;

TraceManagerUbuntu::TraceManagerUbuntu( PerformanceInterface* performanceInterface )
: TraceManager( performanceInterface )
{
  TraceManagerUbuntu::traceManagerUbuntu = this;
}

Dali::Integration::Trace::LogContextFunction TraceManagerUbuntu::GetLogContextFunction()
{
  return LogContext;
}

void TraceManagerUbuntu::LogContext( bool start, const char* tag )
{
  if( start )
  {
    unsigned short contextId = traceManagerUbuntu->mPerformanceInterface->AddContext( tag );
    traceManagerUbuntu->mPerformanceInterface->AddMarker( PerformanceInterface::START, contextId );
  }
  else
  {
    unsigned short contextId = traceManagerUbuntu->mPerformanceInterface->AddContext( tag );
    traceManagerUbuntu->mPerformanceInterface->AddMarker( PerformanceInterface::END, contextId );
  }
}

} // namespace Adaptor

} // namespace Internal

} // namespace Dali
