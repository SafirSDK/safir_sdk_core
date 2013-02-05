/******************************************************************************
*
* Copyright Saab AB, 2006-2008 (http://www.safirsdk.com)
* 
* Created by: Lars Hagström / stlrha
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

#ifndef __DOTS_MEMBERS_H__
#define __DOTS_MEMBERS_H__


#include <Safir/Dob/Typesystem/Defs.h>
#include <Safir/Dob/Typesystem/Exceptions.h>

namespace Safir
{
namespace Dob
{
namespace Typesystem
{
    /**
     * Functions for getting member information from types.
     *
     * With these operations you can get information on types regarding
     * their members. You can get member names and indexes. You can
     * get TypeIds of members etc.
     */
namespace Members
{
    /**
     * Get the number of members for a class or property.
     *
     * Parameters are not included.
     *
     * @param typeId [in] - TypeId of class or property.
     * @return Number of members in the class.
     * @throws IllegalValueException There is no such type defined.
     */
    DOTS_API Dob::Typesystem::Int32 GetNumberOfMembers(const Dob::Typesystem::TypeId typeId);

    /**
     * Get the member index of a named member.
     *
     * @param typeId [in] - TypeId of class or property.
     * @param memberName [in] - name of member as specified in xml description, case sensitive.
     * @return Member index of the member.
     * @throws IllegalValueException There is no such type defined or there is no such member
     *                               in the type.
     */
    DOTS_API Dob::Typesystem::MemberIndex GetIndex(const Dob::Typesystem::TypeId typeId,
                                                   const std::wstring & memberName);

    /**
     * Get the name of the specified member as it was defined in the xml description.
     *
     * @param typeId [in] - TypeId of class or property.
     * @param member [in] - Index of member.
     * @return Name of member.
     * @throws IllegalValueException There is no such type defined or there is no such member
     *                               in the type.
     */
    DOTS_API std::wstring GetName(const Dob::Typesystem::TypeId typeId,
                                  const Dob::Typesystem::MemberIndex member);

    /**
     * Get type id of object or enumeration member.
     *
     * If a member is of type object or enumeration, this method can be used to get the
     * typeId for the class or enum that the member is of.
     *
     * @param typeId [in] - TypeId of class or property.
     * @param member [in] - Index of member.
     * @return The TypeId for the object or enumeration member.
     * @throws IllegalValueException There is no such type defined or there is no such member
     *                               in the type or the member is not an enum or object.
     */
    DOTS_API Dob::Typesystem::TypeId GetTypeId(const Dob::Typesystem::TypeId typeId,
                                               const Dob::Typesystem::MemberIndex member);

    /**
     * Get information about a specific class member.
     *
     * @param typeId [in] - TypeId of class or property.
     * @param member [in] - Index of member.
     * @param memberType [out] - The type of the member.
     * @param memberName [out] - The name of the member.
     * @param memberTypeId [out] - if memberType is object or enumeration, this is the typeId of that type.
     *                             If memberType is something else the value is -1.
     * @param stringLength [out] - If memberType is string and the type is a class (not property) then this is the length of the string.
     * @param isArray [out] - True if member is an array. Not applicable if type id is a property.
     * @param arrayLength [out] - Maximum capacity of array if the member is an array (1 if not an array). Not applicable if type id is a property.
     * @throws IllegalValueException There is no such type defined or there is no such member
     *                               in the type.
     */
    DOTS_API void GetInfo(const Dob::Typesystem::TypeId typeId,
                          const Dob::Typesystem::MemberIndex member,
                          Dob::Typesystem::MemberType& memberType,
                          const char * & memberName,
                          Dob::Typesystem::TypeId & memberTypeId,
                          Dob::Typesystem::Int32 & stringLength,
                          bool & isArray,
                          Dob::Typesystem::Int32 & arrayLength);

    /**
     * Get the array size of a member.
     *
     * @param typeId [in] - TypeId of class.
     * @param member [in] - Index of member.
     * @return The array size of the member.
     * @throws IllegalValueException There is no such class defined or there is no such member
     *                               in the type.
     */
    DOTS_API Dob::Typesystem::Int32 GetArraySize(const Dob::Typesystem::TypeId typeId,
                                                 const Dob::Typesystem::MemberIndex member);

    /**
     * Get the maximum string length of a member.
     *
     * @param typeId [in] - TypeId of class.
     * @param member [in] - Index of member.
     * @return The maximum length of the string member.
     * @throws IllegalValueException There is no such class defined or there is no such member
     *                               in the type or the member is not a string.
     */
    DOTS_API Dob::Typesystem::Int32 GetMaxStringLength(const Dob::Typesystem::TypeId typeId,
                                                       const Dob::Typesystem::MemberIndex member);

    /**
     * Get the name of the type as it was defined in the xml description.
     *
     * @param typeId [in] - TypeId of class.
     * @param member [in] - Index of member.
     * @return The name of the type.
     * @throws IllegalValueException There is no such class defined or there is no such member
     *                               in the type or the member is not a string.
     */
    DOTS_API std::wstring GetTypeName(const Dob::Typesystem::TypeId typeId,
                                      const Dob::Typesystem::MemberIndex member);
}
}
}
}

#endif

