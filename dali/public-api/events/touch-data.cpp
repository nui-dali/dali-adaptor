/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

// CLASS HEADER
#include <dali/public-api/events/touch-data.h>

// INTERNAL INCLUDES
#include <dali/public-api/actors/actor.h>
#include <dali/internal/event/events/touch-data-impl.h>

#include <cstdio>

namespace Dali
{

TouchData::TouchData()
: BaseHandle()
{
}

TouchData::TouchData( const TouchData& other )
: BaseHandle( other )
{
}

TouchData::~TouchData()
{
}

TouchData& TouchData::operator=( const TouchData& other )
{
  BaseHandle::operator=( other );
  return *this;
}

unsigned long TouchData::GetTime() const
{
  return GetImplementation( *this ).GetTime();
}

size_t TouchData::GetPointCount() const
{
  return GetImplementation( *this ).GetPointCount();
}

int32_t TouchData::GetDeviceId( size_t point ) const
{
  return GetImplementation( *this ).GetDeviceId( point );
}

PointState::Type TouchData::GetState( size_t point ) const
{
  return GetImplementation( *this ).GetState( point );
}

Actor TouchData::GetHitActor( size_t point ) const
{
  return GetImplementation( *this ).GetHitActor( point );
}

const Vector2& TouchData::GetLocalPosition( size_t point ) const
{
  return GetImplementation( *this ).GetLocalPosition( point );
}

const Vector2& TouchData::GetScreenPosition( size_t point ) const
{
  return GetImplementation( *this ).GetScreenPosition( point );
}

TouchData::TouchData( Internal::TouchData* internal )
: BaseHandle( internal )
{
}

} // namespace Dali
