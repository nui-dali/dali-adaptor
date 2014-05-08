//
// Copyright (c) 2014 Samsung Electronics Co., Ltd.
//
// Licensed under the Flora License, Version 1.0 (the License);
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://floralicense.org/license/
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an AS IS BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <internal/common/ecore-x/pixmap-render-surface.h>

namespace Dali
{

namespace Internal
{

namespace Adaptor
{

namespace ECoreX
{

DALI_EXPORT_API RenderSurface* CreatePixmapSurface(
  PositionSize       positionSize,
  Any                surface,
  Any                display,
  const std::string& name,
  bool               isTransparent )
{
  return new PixmapRenderSurface( positionSize, surface, display, name, isTransparent );
}

} // namespace ECoreX

} // namespace Adaptor

} // namespace Internal

} // namespace Dali



