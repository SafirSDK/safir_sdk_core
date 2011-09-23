/******************************************************************************
*
* Copyright Saab AB, 2006-2008 (http://www.safirsdk.com)
* 
* Created by: Lars Hagstr�m / stlrha
*
*******************************************************************************
*
* This file is part of Safir SDK Core.
*
* Safir SDK Core is free software: you can redistribute it and/or modify
* it under the terms of version 3 of the GNU General Public License as
* published by the Free Software Foundation.
*
* Safir SDK Core is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Safir SDK Core.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/

#ifndef __DOTS_PARAMETERS_H__
#define __DOTS_PARAMETERS_H__

#include <Safir/Dob/Typesystem/Defs.h>
#include <Safir/Dob/Typesystem/Exceptions.h>
#include <Safir/Dob/Typesystem/EntityId.h>
#include <Safir/Dob/Typesystem/ChannelId.h>
#include <Safir/Dob/Typesystem/HandlerId.h>
#include <Safir/Dob/Typesystem/Object.h>

//Get rid of stupid define from WinGDI.h
#ifdef GetObject
#undef GetObject
#endif

namespace Safir
{
namespace Dob
{
namespace Typesystem
{
    /**
     * Functions for getting parameter information from types.
     *
     * With these operations you can get parameter values from types.
     * You can also get information about the parameters in a type, such as
     * parameter names and indexes, TypeIds of parameters etc.
     */
    namespace Parameters
    {
        /**
         * @name Information about parameters.
         */

        /** @{ */

        /**
         * Get the number of parameters defined in a class.
         *
         * @param typeId [in] - TypeId of class.
         * @return The number of parameters.
         * @throws IllegalValueException There is no such type defined.
         */
        DOTS_API Dob::Typesystem::Int32 GetNumberOfParameters(const Dob::Typesystem::TypeId typeId);

        /**
         * Gets index of a named parameter.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameterName [in] - Name of parameter.
         * @return index of the named parameter.
         * @throws IllegalValueException There is no such type or parameter defined.
         */
        DOTS_API Dob::Typesystem::ParameterIndex GetIndex(const Dob::Typesystem::TypeId typeId,
                                                          const std::wstring & parameterName);

        /**
         * Get the name of the specified parameter as it was defined in the xml description.
         *
         * If the parameter does not exist the returned value is undefined. Use
         * #GetIndex to get a valid ParameterIndex, which is guaranteed to exist.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @return The name of the parameter.
         */
        DOTS_API const std::wstring GetName(const Dob::Typesystem::TypeId typeId,
                                            const Dob::Typesystem::ParameterIndex parameter);

        /**
         * Get the type of a parameter.
         *
         * If the parameter does not exist the returned value is undefined. Use
         * #GetIndex to get a valid ParameterIndex, which is guaranteed to exist.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @return The type of the parameter.
         */
        DOTS_API MemberType GetType(const Dob::Typesystem::TypeId typeId,
                                    const Dob::Typesystem::ParameterIndex parameter);

        /**
         * Gets a string representation of the type of a parameter.
         *
         * If the parameter is not an object or enumeration the result is undefined.
         *
         * If the parameter does not exist the returned value is undefined. Use
         * #GetIndex to get a valid ParameterIndex, which is guaranteed to exist.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @return Name of the parameter type.
         */
        DOTS_API const std::wstring GetTypeName(const Dob::Typesystem::TypeId typeId,
                                                const Dob::Typesystem::ParameterIndex parameter);

        /**
         * Get the array size of a parameter.
         *
         * If the parameter does not exist the returned value is undefined. Use
         * #GetIndex to get a valid ParameterIndex, which is guaranteed to exist.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @return The array size of the parameter, or 1 if it is not an array.
         */
        DOTS_API Dob::Typesystem::Int32 GetArraySize(const Dob::Typesystem::TypeId typeId,
                                                     const Dob::Typesystem::ParameterIndex parameter);

        /** @} */

        /**
         * @name Parameter values.
         */

        /** @{ */

        /**
         * Get a boolean parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API bool GetBoolean(const Dob::Typesystem::TypeId typeId,
                                 const Dob::Typesystem::ParameterIndex parameter,
                                 const Dob::Typesystem::ArrayIndex index);

        /**
         * Get an enumeration parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API Dob::Typesystem::Int32 GetEnumeration(const Dob::Typesystem::TypeId typeId,
                                                       const Dob::Typesystem::ParameterIndex parameter,
                                                       const Dob::Typesystem::ArrayIndex index);

        /**
         * Get an Int32 parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API Dob::Typesystem::Int32 GetInt32(const Dob::Typesystem::TypeId typeId,
                                                 const Dob::Typesystem::ParameterIndex parameter,
                                                 const Dob::Typesystem::ArrayIndex index);

        /**
         * Get an Int64 parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API Dob::Typesystem::Int64 GetInt64(const Dob::Typesystem::TypeId typeId,
                                                 const Dob::Typesystem::ParameterIndex parameter,
                                                 const Dob::Typesystem::ArrayIndex index);

        /**
         * Get a Float32 parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API Dob::Typesystem::Float32 GetFloat32(const Dob::Typesystem::TypeId typeId,
                                                     const Dob::Typesystem::ParameterIndex parameter,
                                                     const Dob::Typesystem::ArrayIndex index);

        /**
         * Get a Float64 parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API Dob::Typesystem::Float64 GetFloat64(const Dob::Typesystem::TypeId typeId,
                                                     const Dob::Typesystem::ParameterIndex parameter,
                                                     const Dob::Typesystem::ArrayIndex index);
        /**
         * Get a string parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API const std::wstring GetString(const Dob::Typesystem::TypeId typeId,
                                              const Dob::Typesystem::ParameterIndex parameter,
                                              const Dob::Typesystem::ArrayIndex index);

        /**
         * Get a TypeId parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API Dob::Typesystem::TypeId GetTypeId(const Dob::Typesystem::TypeId typeId,
                                                   const Dob::Typesystem::ParameterIndex parameter,
                                                   const Dob::Typesystem::ArrayIndex index);

        /**
         * Get a InstanceId parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API const Dob::Typesystem::InstanceId GetInstanceId(const Dob::Typesystem::TypeId typeId,
                                                                 const Dob::Typesystem::ParameterIndex parameter,
                                                                 const Dob::Typesystem::ArrayIndex index);

        /**
         * Get an EntityId parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API const Dob::Typesystem::EntityId GetEntityId(const Dob::Typesystem::TypeId typeId,
                                                             const Dob::Typesystem::ParameterIndex parameter,
                                                             const Dob::Typesystem::ArrayIndex index);

        /**
         * Get a ChannelId parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API const Dob::Typesystem::ChannelId GetChannelId(const Dob::Typesystem::TypeId typeId,
                                                               const Dob::Typesystem::ParameterIndex parameter,
                                                               const Dob::Typesystem::ArrayIndex index);

        /**
         * Get a HandlerId parameter value.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API const Dob::Typesystem::HandlerId GetHandlerId(const Dob::Typesystem::TypeId typeId,
                                                               const Dob::Typesystem::ParameterIndex parameter,
                                                               const Dob::Typesystem::ArrayIndex index);

        /**
         * Get an Object parameter value.
         *
         * This method will return a smart pointer to a new copy of the parameter.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API const Dob::Typesystem::ObjectPtr GetObject(const Dob::Typesystem::TypeId typeId,
                                                            const Dob::Typesystem::ParameterIndex parameter,
                                                            const Dob::Typesystem::ArrayIndex index);

        /**
         * Get a Binary parameter value.
         *
         * This method will retrieve a pointer to the binary data and the number of bytes the binary data contains.
         *
         * @param typeId [in] - TypeId of class.
         * @param parameter [in] - Index of parameter.
         * @param index [in] - Array index. If parameter is not an array this shall be 0.
         * @return Parameter value.
         */
        DOTS_API const Dob::Typesystem::Binary GetBinary(const Dob::Typesystem::TypeId typeId,
                                                         const Dob::Typesystem::ParameterIndex parameter,
                                                         const Dob::Typesystem::ArrayIndex index);


        /** @} */
    }
}
}
}

#endif

