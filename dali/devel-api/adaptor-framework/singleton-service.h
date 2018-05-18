#ifndef __DALI_SINGELTON_SERVICE_H__
#define __DALI_SINGELTON_SERVICE_H__

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
#include <typeinfo>
#include <dali/public-api/object/base-handle.h>

// INTERNAL INCLUDES
#include <dali/public-api/dali-adaptor-common.h>

namespace Dali
{

namespace Internal DALI_INTERNAL
{
namespace Adaptor
{
class SingletonService;
}
}

/**
 * @brief Allows the registration of a class as a singleton
 *
 * @note This class is created by the Application class and is destroyed when the Application class is destroyed.
 *
 * @see Application
 */
class DALI_ADAPTOR_API SingletonService : public BaseHandle
{
public:

  /**
   * @brief Create an uninitialized handle.
   *
   * This can be initialized by calling SingletonService::Get().
   */
  SingletonService();

  /**
   * Create a SingletonService.
   * This should only be called once by the Application class.
   * @return A newly created SingletonService.
   */
  static Dali::SingletonService New();

  /**
   * @brief Retrieves a handle to the SingletonService.
   *
   * @return A handle to the SingletonService if it is available. This will be an empty handle if
   *         the service is not available.
   */
  static SingletonService Get();

  /**
   * @brief Destructor
   *
   * This is non-virtual since derived Handle types must not contain data or virtual methods.
   */
  ~SingletonService();

  /**
   * @brief Registers the singleton of Dali handle with its type info.
   *
   * The singleton will be kept alive for the lifetime of the service.
   *
   * @note This is not intended for application developers.
   * @param[in] info The type info of the Dali handle generated by the compiler.
   * @param[in] singleton The Dali handle to be registered
   */
  void Register( const std::type_info& info, BaseHandle singleton );

  /**
   * @brief Unregisters all singletons.
   *
   * @note This is not intended for application developers.
   */
  void UnregisterAll();

  /**
   * @brief Gets the singleton for the given type.
   *
   * @note This is not intended for application developers.
   * @param[in] info The type info of the given type.
   * @return the Dali handle if it is registered as a singleton or an uninitialized handle.
   */
  BaseHandle GetSingleton( const std::type_info& info ) const;

public: // Not intended for application developers

  /**
   * @brief This constructor is used by SingletonService::Get().
   * @param[in] singletonService A pointer to the internal singleton-service object.
   */
  explicit DALI_INTERNAL SingletonService( Internal::Adaptor::SingletonService* singletonService );
};

} // namespace Dali

#endif // __DALI_SINGELTON_SERVICE_H__
