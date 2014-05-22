#ifndef __DALI_INTERNAL_ADAPTOR_TRIGGER_EVENT_FACTORY_H__
#define __DALI_INTERNAL_ADAPTOR_TRIGGER_EVENT_FACTORY_H__

/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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
#include <base/interfaces/trigger-event-factory-interface.h>

namespace Dali
{

namespace Internal
{

namespace Adaptor
{

/**
 * @brief Trigger interface factory class
 *
 */
class TriggerEventFactory : public TriggerEventFactoryInterface
{

public:

  /**
   * @brief Constructor
   */
  TriggerEventFactory()
  {
  }

  /**
   * @brief Destructor
   */
  virtual ~TriggerEventFactory()
  {
  }

  /**
   * @copydoc TriggerEventFactoryInterface::CreateTriggerEvent
   */
  virtual TriggerEventInterface* CreateTriggerEvent(  boost::function<void()> functor, TriggerEventInterface::Options options );


  /**
   * @copydoc TriggerEventFactoryInterface::DestroyTriggerEvent
   */
  virtual void DestroyTriggerEvent( TriggerEventInterface* triggerEventInterface );

};

} // namespace Internal

} // namespace Adaptor

} // namespace Dali

#endif // __DALI_INTERNAL_ADAPTOR_TRIGGER_EVENT_FACTORY_H__
