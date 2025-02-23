/******************************************************************************
*
* Copyright Saab AB, 2005-2013 (http://safirsdkcore.com)
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

#include <iostream>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <Safir/Dob/Typesystem/Internal/Kernel.h>
#include <Safir/Dob/Typesystem/ObjectFactory.h>
#include <Safir/Dob/Typesystem/ToolSupport/Serialization.h>


#include <Safir/Dob/Typesystem/Serialization.h>
#include <Safir/Dob/Typesystem/Exceptions.h>
#include <Safir/Dob/Typesystem/LibraryExceptions.h>
#include <DotsTest/ParameterTypes.h>
#include <DotsTest/ParameterArrays.h>
#include <DotsTest/ParameterDictionaries.h>
#include <DotsTest/EmptyObject.h>
#include <DotsTest/AnotherEmptyObject.h>
#include <DotsTest/MemberTypes.h>
#include <DotsTest/MemberArrays.h>
#include <DotsTest/MemberSequences.h>
#include <DotsTest/MemberDictionaries.h>
#include <DotsTest/MemberItems.h>
#include <DotsTest/MemberItemsArray.h>
#include <DotsTest/MemberTypesProperty.h>
#include <DotsTest/MemberArraysProperty.h>
#include <DotsTest/TestEnum.h>
#include <DotsTest/TestException.h>
#include <DotsTest/TypesItemInherited.h>
#include <Safir/Dob/Typesystem/Parameters.h>
#include <Safir/Dob/Typesystem/Internal/BlobOperations.h>
#include <Safir/Dob/Typesystem/Members.h>
#include <Safir/Dob/Typesystem/Properties.h>
#include <Safir/Dob/Typesystem/Internal/InternalOperations.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/operations.hpp>

#define BOOST_TEST_MODULE DotsTestCpp
#include <boost/test/unit_test.hpp>


#ifdef _MSC_VER
//disable warning about hiding previous declaration.
#pragma warning (disable:4456)
#endif

namespace ts = Safir::Dob::Typesystem;

std::wstring CollectionTypeStr(Safir::Dob::Typesystem::CollectionType ct)
{
    switch (ct)
    {
    case SingleValueCollectionType: return L"Single";
    case ArrayCollectionType: return L"Array";
    case SequenceCollectionType: return L"Sequence";
    case DictionaryCollectionType: return L"Dictionary";
    }
    return L"";
}

std::wstring MappingKindStr(Safir::Dob::Typesystem::PropertyMappingKind k)
{
    switch (k)
    {
    case MappedToNull: return L"MappedToNull";
    case MappedToMember: return L"MappedToMember";
    case MappedToParameter: return L"MappedToParameter";
    }
    return L"";
}

std::wstring utf8_to_wstr( const std::string& str )
{
    int left = 0;
    wchar_t *pwszBuf = new wchar_t[str.length() + 1];
    wchar_t *pwsz;
    unsigned long pos;

    pwsz = pwszBuf;

    std::string::const_iterator it;
    for( it = str.begin(); it != str.end(); ++it)
    {
        pos = (unsigned char) *it;
        if ((left == 0) ^ ((pos & 0xC0) != 0x80)) // Continuation byte mismatch
        {
            left = 0;
            *pwsz++ = L'#';
        }

        if (pos < 0x80) // 7-bit ASCII
        {
            *pwsz++ = (wchar_t) pos;
        }
        else if ((pos & 0xC0) == (0x80)) // Correct continuation
        {
            left--;
            *pwsz = (*pwsz << 6) + (wchar_t) (pos & 0x3F);
            if (left == 0)
                pwsz++;
        }
        else if ((pos & 0xE0) == (0xC0)) // First of 2
        {
            *pwsz = (wchar_t) (pos & 0x1F);
            left = 1;
        }
        else if ((pos & 0xF0) == (0xE0)) // First of 3
        {
            *pwsz = (wchar_t) (pos & 0x0F);
            left = 2;
        }
        else // Only the BMP is supported.
        {
            left = 0;
            *pwsz++ = L'#';
        }

    }

    std::wstring wstr( pwszBuf, pwsz - pwszBuf );

    delete [] pwszBuf;
    return wstr;
}

namespace std
{
    //needed by boost test, and must be in std for it to work.
    std::ostream& operator <<(std::ostream& out, const std::wstring& str)
    {
        return out << Safir::Dob::Typesystem::Utilities::ToUtf8(str);
    }

    //needed by boost test, and must be in std for it to work.
    std::ostream& operator <<(std::ostream& out, const wchar_t* str)
    {
        return out << Safir::Dob::Typesystem::Utilities::ToUtf8(str);
    }

    //needed by boost test, and must be in std for it to work.
    std::ostream& operator<<(std::ostream& out, const std::vector<char>& coll)
    {
        for (const auto c: coll)
        {
            std::wcout << std::hex << static_cast<int>(c) << " ";
        }
        return out;
    }

}


std::wostream & operator<<(std::wostream & out, const Safir::Dob::Typesystem::MemberType value)
{
    static const char *  typenames[] =
        {
            "BOOLEANMEMBERTYPE",
            "ENUMERATIONMEMBERTYPE",
            "INT32MEMBERTYPE",
            "INT64MEMBERTYPE",
            "FLOAT32MEMBERTYPE",
            "FLOAT64MEMBERTYPE",
            "TYPEIDMEMBERTYPE",
            "INSTANCEIDMEMBERTYPE",
            "ENTITYIDMEMBERTYPE",
            "CHANNELIDMEMBERTYPE",
            "HANDLERIDMEMBERTYPE",
            "STRINGMEMBERTYPE",
            "OBJECTMEMBERTYPE",
            "BINARYMEMBERTYPE",

            //  SI32 TYPES
            "AMPERE32MEMBERTYPE",
            "CUBICMETER32MEMBERTYPE",
            "HERTZ32MEMBERTYPE",
            "JOULE32MEMBERTYPE",
            "KELVIN32MEMBERTYPE",
            "KILOGRAM32MEMBERTYPE",
            "METER32MEMBERTYPE",
            "METERPERSECOND32MEMBERTYPE",
            "METERPERSECONDSQUARED32MEMBERTYPE",
            "NEWTON32MEMBERTYPE",
            "PASCAL32MEMBERTYPE",
            "RADIAN32MEMBERTYPE",
            "RADIANPERSECOND32MEMBERTYPE",
            "RADIANPERSECONDSQUARED32MEMBERTYPE",
            "SECOND32MEMBERTYPE",
            "SQUAREMETER32MEMBERTYPE",
            "STERADIAN32MEMBERTYPE",
            "VOLT32MEMBERTYPE",
            "WATT32MEMBERTYPE",

            //  SI LONG TYPES
            "AMPERE64MEMBERTYPE",
            "CUBICMETER64MEMBERTYPE",
            "HERTZ64MEMBERTYPE",
            "JOULE64MEMBERTYPE",
            "KELVIN64MEMBERTYPE",
            "KILOGRAM64MEMBERTYPE",
            "METER64MEMBERTYPE",
            "METERPERSECOND64MEMBERTYPE",
            "METERPERSECONDSQUARED64MEMBERTYPE",
            "NEWTON64MEMBERTYPE",
            "PASCAL64MEMBERTYPE",
            "RADIAN64MEMBERTYPE",
            "RADIANPERSECOND64MEMBERTYPE",
            "RADIANPERSECONDSQUARED64MEMBERTYPE",
            "SECOND64MEMBERTYPE",
            "SQUAREMETER64MEMBERTYPE",
            "STERADIAN64MEMBERTYPE",
            "VOLT64MEMBERTYPE",
            "WATT64MEMBERTYPE",
        };
    out << typenames[value];
    return out;
}

void Header(const std::wstring & label)
{
    std::wcout<<std::endl<<std::endl<<std::endl;
    std::wcout<<L"====================================================="<<std::endl;
    std::wcout<<L"Testing: "<<label<<std::endl;
    std::wcout<<L"====================================================="<<std::endl;
}

//Disable an incorrect warning for VS2015. See more info here:
//https://stackoverflow.com/questions/34013930/error-c4592-symbol-will-be-dynamically-initialized-vs2015-1-static-const-std
#if _MSC_VER == 1900
#pragma warning (push)
#pragma warning (disable:4592)
#endif

static DotsTest::MemberTypesPtr MT1;
static DotsTest::MemberTypesPtr MT2;
static DotsTest::MemberArraysPtr MA1;
static DotsTest::MemberArraysPtr MA2;
static DotsTest::MemberSequencesPtr MS1;
static DotsTest::MemberItemsPtr MI;
static DotsTest::MemberItemsArrayPtr MIA;
static DotsTest::EmptyObjectPtr EO;
static DotsTest::AnotherEmptyObjectPtr AEO;

#if _MSC_VER == 1900
#pragma warning (pop)
#endif


void Test_Has_Property()
{
    Header(L"Has Property");
    std::wcout<<"MemberTypes - MemberTypesProperty: "<<DotsTest::MemberTypesProperty::HasProperty(MT1)<<std::endl;
    std::wcout<<"MemberTypes - MemberArraysProperty: "<<DotsTest::MemberArraysProperty::HasProperty(MT1)<<std::endl;
    std::wcout<<"MemberArrays - MemberTypesProperty: "<<DotsTest::MemberTypesProperty::HasProperty(MA1)<<std::endl;
    std::wcout<<"MemberArrays - MemberArraysProperty: "<<DotsTest::MemberArraysProperty::HasProperty(MA1)<<std::endl;
    std::wcout<<"MemberItems - MemberTypesProperty: "<<DotsTest::MemberTypesProperty::HasProperty(MI)<<std::endl;
    std::wcout<<"MemberItems - MemberArraysProperty: "<<DotsTest::MemberArraysProperty::HasProperty(MI)<<std::endl;
    std::wcout<<"MemberItemsArray - MemberTypesProperty: "<<DotsTest::MemberTypesProperty::HasProperty(MIA)<<std::endl;
    std::wcout<<"MemberItemsArray - MemberArraysProperty: "<<DotsTest::MemberArraysProperty::HasProperty(MIA)<<std::endl;
    std::wcout<<"EmptyObject - MemberTypesProperty: "<<DotsTest::MemberTypesProperty::HasProperty(EO)<<std::endl;
    std::wcout<<"EmptyObject - MemberArraysProperty: "<<DotsTest::MemberArraysProperty::HasProperty(EO)<<std::endl;
    std::wcout<<"AnotherEmptyObject - MemberTypesProperty: "<<DotsTest::MemberTypesProperty::HasProperty(AEO)<<std::endl;
    std::wcout<<"AnotherEmptyObject - MemberArraysProperty: "<<DotsTest::MemberArraysProperty::HasProperty(AEO)<<std::endl;
}

void Test_PropertyMappingKind()
{
    Header(L"Property Mapping Kind");
    std::wcout << L"EmptyObject: " << MappingKindStr(Safir::Dob::Typesystem::Properties::GetMappingKind(DotsTest::EmptyObject::ClassTypeId, DotsTest::MemberTypesProperty::ClassTypeId, 0)) << std::endl;
    std::wcout << L"AnotherEmptyObject: " << MappingKindStr(Safir::Dob::Typesystem::Properties::GetMappingKind(DotsTest::AnotherEmptyObject::ClassTypeId, DotsTest::MemberTypesProperty::ClassTypeId, 0)) << std::endl;
    std::wcout << L"MemberItems: " << MappingKindStr(Safir::Dob::Typesystem::Properties::GetMappingKind(DotsTest::MemberItems::ClassTypeId, DotsTest::MemberTypesProperty::ClassTypeId, 0)) << std::endl;
}

void Test_Property_GetParameterReference()
{
    Header(L"Property Get Parameter Reference");

    Safir::Dob::Typesystem::TypeId paramTid;
    Safir::Dob::Typesystem::ParameterIndex paramIx;
    Safir::Dob::Typesystem::ArrayIndex paramArrIx;

    Safir::Dob::Typesystem::Properties::GetParameterReference(DotsTest::EmptyObject::ClassTypeId, DotsTest::MemberTypesProperty::ClassTypeId, 6, 0, paramTid, paramIx, paramArrIx);
    std::wcout << L"EmptyObject: " << Safir::Dob::Typesystem::Parameters::GetString(paramTid, paramIx, paramArrIx) << std::endl;
    Safir::Dob::Typesystem::Properties::GetParameterReference(DotsTest::AnotherEmptyObject::ClassTypeId, DotsTest::MemberTypesProperty::ClassTypeId, 9, 0, paramTid, paramIx, paramArrIx);
    std::wcout << L"AnotherEmptyObject: " << Safir::Dob::Typesystem::Parameters::GetTypeId(paramTid, paramIx, paramArrIx) << std::endl;

    try
    {
        Safir::Dob::Typesystem::Properties::GetParameterReference(DotsTest::MemberItems::ClassTypeId, DotsTest::MemberTypesProperty::ClassTypeId, 0, 0, paramTid, paramIx, paramArrIx);
    }
    catch (const Safir::Dob::Typesystem::IllegalValueException&)
    {
        std::wcout << L"Not mapped to parameter" << std::endl;
    }
}

void Test_GetName()
{
    Header(L"Get Name");
    std::wcout<<"MemberTypes          - " << Safir::Dob::Typesystem::Operations::GetName(DotsTest::MemberTypes::ClassTypeId)<<std::endl;
    std::wcout<<"MemberArrays         - " << Safir::Dob::Typesystem::Operations::GetName(DotsTest::MemberArrays::ClassTypeId)<<std::endl;
    std::wcout<<"MemberTypesProperty  - " << Safir::Dob::Typesystem::Operations::GetName(DotsTest::MemberTypesProperty::ClassTypeId)<<std::endl;
    std::wcout<<"MemberArraysProperty - " << Safir::Dob::Typesystem::Operations::GetName(DotsTest::MemberArraysProperty::ClassTypeId)<<std::endl;
    std::wcout<<"MemberItems          - " << Safir::Dob::Typesystem::Operations::GetName(DotsTest::MemberItems::ClassTypeId)<<std::endl;
    std::wcout<<"MemberItemsArray     - " << Safir::Dob::Typesystem::Operations::GetName(DotsTest::MemberItemsArray::ClassTypeId)<<std::endl;
    std::wcout<<"EmptyObject          - " << Safir::Dob::Typesystem::Operations::GetName(DotsTest::EmptyObject::ClassTypeId)<<std::endl;
}

void Test_GetNumberOfMembers()
{
    Header(L"Get Number Of Members");
    std::wcout<<"MemberTypes          - " << Safir::Dob::Typesystem::Members::GetNumberOfMembers(DotsTest::MemberTypes::ClassTypeId)<<std::endl;
    std::wcout<<"MemberArrays         - " << Safir::Dob::Typesystem::Members::GetNumberOfMembers(DotsTest::MemberArrays::ClassTypeId)<<std::endl;
    std::wcout<<"MemberTypesProperty  - " << Safir::Dob::Typesystem::Members::GetNumberOfMembers(DotsTest::MemberTypesProperty::ClassTypeId)<<std::endl;
    std::wcout<<"MemberArraysProperty - " << Safir::Dob::Typesystem::Members::GetNumberOfMembers(DotsTest::MemberArraysProperty::ClassTypeId)<<std::endl;
    std::wcout<<"MemberItems          - " << Safir::Dob::Typesystem::Members::GetNumberOfMembers(DotsTest::MemberItems::ClassTypeId)<<std::endl;
    std::wcout<<"MemberItemsArray     - " << Safir::Dob::Typesystem::Members::GetNumberOfMembers(DotsTest::MemberItemsArray::ClassTypeId)<<std::endl;
    std::wcout<<"EmptyObject          - " << Safir::Dob::Typesystem::Members::GetNumberOfMembers(DotsTest::EmptyObject::ClassTypeId)<<std::endl;
    std::wcout<<"ParameterTypes       - " << Safir::Dob::Typesystem::Members::GetNumberOfMembers(DotsTest::ParameterTypes::ClassTypeId)<<std::endl;
    std::wcout<<"ParameterArrays      - " << Safir::Dob::Typesystem::Members::GetNumberOfMembers(DotsTest::ParameterArrays::ClassTypeId)<<std::endl;
}

void Test_GetNumberOfParameters()
{
    Header(L"Get Number Of Parameters");
    std::wcout<<"MemberTypes          - " << Safir::Dob::Typesystem::Parameters::GetNumberOfParameters(DotsTest::MemberTypes::ClassTypeId)<<std::endl;
    std::wcout<<"MemberArrays         - " << Safir::Dob::Typesystem::Parameters::GetNumberOfParameters(DotsTest::MemberArrays::ClassTypeId)<<std::endl;
    std::wcout<<"MemberItems          - " << Safir::Dob::Typesystem::Parameters::GetNumberOfParameters(DotsTest::MemberItems::ClassTypeId)<<std::endl;
    std::wcout<<"MemberItemsArray     - " << Safir::Dob::Typesystem::Parameters::GetNumberOfParameters(DotsTest::MemberItemsArray::ClassTypeId)<<std::endl;
    std::wcout<<"EmptyObject          - " << Safir::Dob::Typesystem::Parameters::GetNumberOfParameters(DotsTest::EmptyObject::ClassTypeId)<<std::endl;
    std::wcout<<"ParameterTypes       - " << Safir::Dob::Typesystem::Parameters::GetNumberOfParameters(DotsTest::ParameterTypes::ClassTypeId)<<std::endl;
    std::wcout<<"ParameterArrays      - " << Safir::Dob::Typesystem::Parameters::GetNumberOfParameters(DotsTest::ParameterArrays::ClassTypeId)<<std::endl;
}

void Test_Create_Routines()
{
    Header(L"Create routines (Types)");
    std::wcout<< "Create_ParameterTypes: " <<
        Safir::Dob::Typesystem::Serialization::ToXml(DotsTest::MemberTypes::CreateParameterTypes
                                                     (DotsTest::ParameterTypes::Int32Parameter(),
                                                      DotsTest::ParameterTypes::EnumerationParameter(),
                                                      DotsTest::ParameterTypes::TestClassParameter()))<<std::endl;
    std::wcout<< "CreateValueTypes     : " << Safir::Dob::Typesystem::Serialization::ToXml(DotsTest::MemberTypes::CreateValueTypes())<<std::endl;
    std::wcout<< "Create_ValueArrays   : " << Safir::Dob::Typesystem::Serialization::ToXml(DotsTest::MemberTypes::CreateValueArrays())<<std::endl;

    std::wcout<< "CreateValues         : " << Safir::Dob::Typesystem::Serialization::ToXml(DotsTest::TypesItem::CreateCreateRoutineValues())<<std::endl;
    std::wcout<< "CreateInheritedValues: " << Safir::Dob::Typesystem::Serialization::ToXml(DotsTest::TypesItemInherited::CreateCreateRoutineValues2())<<std::endl;    

    auto createParameters = DotsTest::TypesItem::CreateCreateRoutineParameters(
            10,
            20,
            30.1f,
            40.1,
            false,
            DotsTest::TestEnum::MySecond,
            L"Hello",
            Safir::Dob::Typesystem::EntityId(Safir::Dob::Entity::ClassTypeId, Safir::Dob::Typesystem::InstanceId(1)),
            Safir::Dob::Typesystem::EntityId(Safir::Dob::Entity::ClassTypeId, Safir::Dob::Typesystem::InstanceId(2)),
            Safir::Dob::Entity::ClassTypeId,
            Safir::Dob::Typesystem::InstanceId(1),
            Safir::Dob::Typesystem::InstanceId(2),
            Safir::Dob::Typesystem::ChannelId(1),
            Safir::Dob::Typesystem::ChannelId(2),
            Safir::Dob::Typesystem::HandlerId(1),
            Safir::Dob::Typesystem::HandlerId(2),
            Safir::Dob::Entity::Create(),
            {'a', 'b', 'c'},
            DotsTest::TestItem::Create(),
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5);

    auto createParametersInherited = DotsTest::TypesItemInherited::CreateCreateRoutineParameters2(
            L"Local Member",
            10,
            20,
            30.1f,
            40.1,
            false,
            DotsTest::TestEnum::MySecond,
            L"Hello",
            Safir::Dob::Typesystem::EntityId(Safir::Dob::Entity::ClassTypeId, Safir::Dob::Typesystem::InstanceId(1)),
            Safir::Dob::Typesystem::EntityId(Safir::Dob::Entity::ClassTypeId, Safir::Dob::Typesystem::InstanceId(2)),
            Safir::Dob::Entity::ClassTypeId,
            Safir::Dob::Typesystem::InstanceId(1),
            Safir::Dob::Typesystem::InstanceId(2),
            Safir::Dob::Typesystem::ChannelId(1),
            Safir::Dob::Typesystem::ChannelId(2),
            Safir::Dob::Typesystem::HandlerId(1),
            Safir::Dob::Typesystem::HandlerId(2),
            Safir::Dob::Entity::Create(),
            {'a', 'b', 'c'},
            DotsTest::TestItem::Create(),
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            1.5f,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5,
            2.5);
    std::wcout<< "CreateParam          : " << Safir::Dob::Typesystem::Serialization::ToXml(createParameters)<<std::endl;
    std::wcout<< "CreateParamInherited : " << Safir::Dob::Typesystem::Serialization::ToXml(createParametersInherited)<<std::endl;
}



void Test_Int32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;

    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Int32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Int32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Int32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Int32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Int32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Int32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Int32Member().IsNull();
    In_Req_Ok  = !MT1->Int32Member().IsChanged();
    MT1->Int32Member().SetVal(DotsTest::ParameterTypes::Int32Parameter());
    Null_Ok  = Null_Ok && !MT1->Int32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Int32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Int32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " <<lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " << Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   MT1->Int32MemberMemberIndex())<<std::endl;

    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId, DotsTest::MemberTypes::Int32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                          Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                       utf8_to_wstr("Int32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                          Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                       utf8_to_wstr("Int32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                  Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                               utf8_to_wstr("Int32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullInt32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt32Member (MT2);
    DotsTest::MemberTypesProperty::SetInt32Member (MT2, MT1->Int32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetInt32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInt32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullInt32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInt32Member(MI);
    DotsTest::MemberTypesProperty::SetInt32Member(MI,MT2->Int32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetInt32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInt32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullInt32Member(MIA);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetInt32Member(MIA,MT2->Int32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Int32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInt32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetInt32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetInt32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Int32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Int32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Int32Member()[ix].IsChanged();
        MA1->Int32Member()[ix].SetVal(DotsTest::ParameterArrays::Int32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Int32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Int32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullInt32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt32Member (MA2, ix);
        MA2->Int32Member()[ix].SetVal(MA1->Int32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt32Member (MA2, ix);

        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetInt32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullInt32Member (MI,ix);
        if (ix == 0)
            In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt32Member (MI,ix);
        else //Second time around we will get a changed flag since level above ref is changed
            In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Int32Member()[ix].SetVal(MA1->Int32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetInt32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullInt32Member (MIA,ix);
        if (ix == 0)
            In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt32Member (MIA,ix);
        else //Second time around we will get a changed flag since level above ref is changed
            In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetInt32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetInt32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetInt32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt32Member (AEO,ix);
    }

    // SetNull test
    MT1->Int32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullInt32Member(MT2);
    MA1->Int32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullInt32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Int32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullInt32Member (MT2) &&
        MA1->Int32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullInt32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Int64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Int64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Int64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Int64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Int64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Int64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Int64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Int64Member().IsNull();
    In_Req_Ok  = !MT1->Int64Member().IsChanged();
    MT1->Int64Member().SetVal(DotsTest::ParameterTypes::Int64Parameter());
    Null_Ok  = Null_Ok && !MT1->Int64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Int64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Int64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Int64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId, DotsTest::MemberTypes::Int64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                          Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                       utf8_to_wstr("Int64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Int64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Int64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;

    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullInt64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt64Member (MT2);
    DotsTest::MemberTypesProperty::SetInt64Member(MT2, MT1->Int64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetInt64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInt64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullInt64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInt64Member(MI);
    DotsTest::MemberTypesProperty::SetInt64Member(MI,MT2->Int64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetInt64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInt64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullInt64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInt64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetInt64Member(MIA,MT2->Int64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Int64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInt64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetInt64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetInt64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInt64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInt64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Int64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Int64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Int64Member()[ix].IsChanged();
        MA1->Int64Member()[ix].SetVal(DotsTest::ParameterArrays::Int64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Int64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Int64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullInt64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt64Member (MA2, ix);
        MA2->Int64Member()[ix].SetVal(MA1->Int64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetInt64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullInt64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Int64Member()[ix].SetVal(MA1->Int64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetInt64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullInt64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetInt64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInt64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetInt64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetInt64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInt64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInt64Member (AEO,ix);
    }

    // SetNull test
    MT1->Int64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullInt64Member(MT2);
    MA1->Int64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullInt64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Int64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullInt64Member (MT2) &&
        MA1->Int64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullInt64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Float32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Float32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Float32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Float32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Float32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Float32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Float32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Float32Member().IsNull();
    In_Req_Ok  = !MT1->Float32Member().IsChanged();
    MT1->Float32Member().SetVal(DotsTest::ParameterTypes::Float32Parameter());
    Null_Ok  = Null_Ok && !MT1->Float32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Float32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Float32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                       DotsTest::MemberTypes::Float32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                               DotsTest::MemberTypes::Float32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                          Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                       utf8_to_wstr("Float32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                          Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                       utf8_to_wstr("Float32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Float32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;

    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullFloat32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat32Member (MT2);
    DotsTest::MemberTypesProperty::SetFloat32Member(MT2, MT1->Float32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetFloat32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullFloat32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat32Member(MI);
    DotsTest::MemberTypesProperty::SetFloat32Member(MI,MT2->Float32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetFloat32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullFloat32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetFloat32Member(MIA,MT2->Float32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Float32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetFloat32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetFloat32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Float32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Float32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Float32Member()[ix].IsChanged();
        MA1->Float32Member()[ix].SetVal(DotsTest::ParameterArrays::Float32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Float32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Float32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullFloat32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat32Member (MA2, ix);
        MA2->Float32Member()[ix].SetVal(MA1->Float32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetFloat32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullFloat32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Float32Member()[ix].SetVal(MA1->Float32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetFloat32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullFloat32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetFloat32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetFloat32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetFloat32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat32Member (AEO,ix);
    }

    // SetNull test
    MT1->Float32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullFloat32Member(MT2);
    MA1->Float32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullFloat32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Float32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullFloat32Member (MT2) &&
        MA1->Float32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullFloat32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;


}


void Test_Float64()
{

    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Float64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Float64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Float64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Float64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Float64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Float64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Float64Member().IsNull();
    In_Req_Ok  = !MT1->Float64Member().IsChanged();
    MT1->Float64Member().SetVal(DotsTest::ParameterTypes::Float64Parameter());
    Null_Ok  = Null_Ok && !MT1->Float64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Float64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Float64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                       DotsTest::MemberTypes::Float64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Float64MemberMemberIndex()) <<std::endl;

std::wcout<<"----Parameters---- " <<std::endl;
std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                   Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Float64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Float64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Float64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullFloat64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat64Member (MT2);
    DotsTest::MemberTypesProperty::SetFloat64Member(MT2, MT1->Float64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetFloat64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullFloat64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat64Member(MI);
    DotsTest::MemberTypesProperty::SetFloat64Member(MI,MT2->Float64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetFloat64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullFloat64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetFloat64Member(MIA,MT2->Float64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Float64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedFloat64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetFloat64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetFloat64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullFloat64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedFloat64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Float64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Float64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Float64Member()[ix].IsChanged();
        MA1->Float64Member()[ix].SetVal(DotsTest::ParameterArrays::Float64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Float64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Float64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullFloat64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat64Member (MA2, ix);
        MA2->Float64Member()[ix].SetVal(MA1->Float64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetFloat64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullFloat64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Float64Member()[ix].SetVal(MA1->Float64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetFloat64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullFloat64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetFloat64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedFloat64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetFloat64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetFloat64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullFloat64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedFloat64Member (AEO,ix);
    }

    // SetNull test
    MT1->Float64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullFloat64Member(MT2);
    MA1->Float64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullFloat64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Float64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullFloat64Member (MT2) &&
        MA1->Float64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullFloat64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Boolean()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Boolean");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::BooleanMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::BooleanMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::BooleanParameterArraySize() == 2 &&
         DotsTest::MemberArrays::BooleanMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::BooleanMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->BooleanMember().IsNull();
    In_Req_Ok  = !MT1->BooleanMember().IsChanged();
    MT1->BooleanMember().SetVal(DotsTest::ParameterTypes::BooleanParameter());
    Null_Ok  = Null_Ok && !MT1->BooleanMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->BooleanMember().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::BooleanMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::BooleanMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::BooleanMemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("BooleanParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("BooleanParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("BooleanParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullBooleanMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBooleanMember (MT2);
    DotsTest::MemberTypesProperty::SetBooleanMember(MT2, MT1->BooleanMember().GetVal());
    std::wcout<<"Val: " <<DotsTest::MemberTypesProperty::GetBooleanMember (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBooleanMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBooleanMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullBooleanMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBooleanMember(MI);
    DotsTest::MemberTypesProperty::SetBooleanMember(MI,MT2->BooleanMember().GetVal());
    std::wcout<<"Item Val: " <<DotsTest::MemberTypesProperty::GetBooleanMember(MI) <<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBooleanMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBooleanMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullBooleanMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBooleanMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetBooleanMember(MIA,MT2->BooleanMember().GetVal());
    std::wcout<<"Item Array Val: " << MIA->TypesItemArray()[1]->BooleanMember().GetVal() <<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBooleanMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBooleanMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBooleanMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBooleanMember(EO);
    std::wcout<<"Property Parameter Val: "  << DotsTest::MemberTypesProperty::GetBooleanMember(EO) <<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBooleanMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBooleanMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBooleanMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBooleanMember(AEO);
    std::wcout<<"Property Parameter Val: "  << DotsTest::MemberTypesProperty::GetBooleanMember(AEO) <<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBooleanMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBooleanMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::BooleanParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->BooleanMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->BooleanMember()[ix].IsChanged();
        MA1->BooleanMember()[ix].SetVal(DotsTest::ParameterArrays::BooleanParameter (ix));
        Null_Ok  = Null_Ok && !MA1->BooleanMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->BooleanMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullBooleanMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBooleanMember (MA2, ix);
        MA2->BooleanMember()[ix].SetVal(MA1->BooleanMember()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBooleanMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBooleanMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": " <<DotsTest::MemberArraysProperty::GetBooleanMember (MA2, ix) <<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullBooleanMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBooleanMember (MI,ix);

        DotsTest::ArraysItem item;
        item.BooleanMember()[ix].SetVal(MA1->BooleanMember()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": " <<DotsTest::MemberArraysProperty::GetBooleanMember (MI,ix) <<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBooleanMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBooleanMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullBooleanMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBooleanMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": " <<
            DotsTest::MemberArraysProperty::GetBooleanMember (MIA,ix) <<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBooleanMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBooleanMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBooleanMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBooleanMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": "  <<
            DotsTest::MemberArraysProperty::GetBooleanMember(EO,ix) <<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBooleanMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBooleanMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBooleanMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBooleanMember (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": "  <<
            DotsTest::MemberArraysProperty::GetBooleanMember(AEO,ix) <<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBooleanMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBooleanMember (AEO,ix);
    }

    // SetNull test
    MT1->BooleanMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullBooleanMember(MT2);
    MA1->BooleanMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullBooleanMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->BooleanMember().IsNull() && DotsTest::MemberTypesProperty::IsNullBooleanMember (MT2) &&
        MA1->BooleanMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullBooleanMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Enumeration()
{

    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Enumeration");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::EnumerationMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::EnumerationMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::EnumerationParameterArraySize() == 2 &&
         DotsTest::MemberArrays::EnumerationMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::EnumerationMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->EnumerationMember().IsNull();
    In_Req_Ok  = !MT1->EnumerationMember().IsChanged();
    MT1->EnumerationMember().SetVal(DotsTest::ParameterTypes::EnumerationParameter());
    Null_Ok  = Null_Ok && !MT1->EnumerationMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->EnumerationMember().IsChanged();
    std::wcout<<"First: "<<DotsTest::TestEnum::First()<<std::endl;
    std::wcout<<"Last: "<<DotsTest::TestEnum::Last()<<std::endl;
    std::wcout<<"Size: "<<DotsTest::TestEnum::Size()<<std::endl;
    std::wcout<<"Test ToString (0): "<<DotsTest::TestEnum::ToString(DotsTest::TestEnum::MyFirst)<<std::endl;
    std::wcout<<"Test ToValue (MySecond): "<<DotsTest::TestEnum::ToValue(DotsTest::TestEnum::ToString(DotsTest::TestEnum::MySecond))<<std::endl;

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::EnumerationMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::EnumerationMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::EnumerationMemberMemberIndex()) <<std::endl;

    std::wcout<<"GetTypeId: " <<Safir::Dob::Typesystem::Members::GetTypeId(DotsTest::MemberTypes::ClassTypeId,
                                                                          DotsTest::MemberTypes::EnumerationMemberMemberIndex())<<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("EnumerationParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("EnumerationParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<
        Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                     Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                  utf8_to_wstr("EnumerationParameter")))<<std::endl;

    std::wcout<<"------------------ " <<std::endl;



    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullEnumerationMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEnumerationMember (MT2);
    DotsTest::MemberTypesProperty::SetEnumerationMember(MT2, MT1->EnumerationMember().GetVal());
    std::wcout<<"Val: "<<Safir::Dob::Typesystem::Operations::GetEnumerationValueName(DotsTest::TestEnum::EnumerationTypeId, DotsTest::MemberTypesProperty::GetEnumerationMember (MT2))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEnumerationMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEnumerationMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullEnumerationMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEnumerationMember(MI);
    DotsTest::MemberTypesProperty::SetEnumerationMember(MI,MT2->EnumerationMember().GetVal());
    std::wcout<<"Item Val: " << Safir::Dob::Typesystem::Operations::GetEnumerationValueName
                                                 (DotsTest::TestEnum::EnumerationTypeId,
                                                  DotsTest::MemberTypesProperty::GetEnumerationMember(MI))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEnumerationMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEnumerationMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullEnumerationMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEnumerationMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetEnumerationMember(MIA,MT2->EnumerationMember().GetVal());
    std::wcout<<"Item Array Val: " << Safir::Dob::Typesystem::Operations::GetEnumerationValueName
                                                 (DotsTest::TestEnum::EnumerationTypeId,
                                                  DotsTest::MemberTypesProperty::GetEnumerationMember(MIA))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEnumerationMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEnumerationMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEnumerationMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEnumerationMember(EO);
    std::wcout<<"Property Parameter Val: " << Safir::Dob::Typesystem::Operations::GetEnumerationValueName
                                                 (DotsTest::TestEnum::EnumerationTypeId,
                                                  DotsTest::MemberTypesProperty::GetEnumerationMember(EO))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEnumerationMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEnumerationMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEnumerationMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEnumerationMember(AEO);
    std::wcout<<"Property Parameter Val: " << Safir::Dob::Typesystem::Operations::GetEnumerationValueName
                                                 (DotsTest::TestEnum::EnumerationTypeId,
                                                  DotsTest::MemberTypesProperty::GetEnumerationMember(AEO))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEnumerationMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEnumerationMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::EnumerationParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->EnumerationMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->EnumerationMember()[ix].IsChanged();
        MA1->EnumerationMember()[ix].SetVal(DotsTest::ParameterArrays::EnumerationParameter (ix));
        Null_Ok  = Null_Ok && !MA1->EnumerationMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->EnumerationMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullEnumerationMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEnumerationMember (MA2, ix);
        MA2->EnumerationMember()[ix].SetVal(MA1->EnumerationMember()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEnumerationMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEnumerationMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<Safir::Dob::Typesystem::Operations::GetEnumerationValueName(DotsTest::TestEnum::EnumerationTypeId,
                                                                                                  DotsTest::MemberArraysProperty::GetEnumerationMember (MA2, ix))<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullEnumerationMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEnumerationMember (MI,ix);

        DotsTest::ArraysItem item;
        item.EnumerationMember()[ix].SetVal(MA1->EnumerationMember()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": " << Safir::Dob::Typesystem::Operations::GetEnumerationValueName
                                                                (DotsTest::TestEnum::EnumerationTypeId,
                                                                 DotsTest::MemberArraysProperty::GetEnumerationMember(MI,ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEnumerationMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEnumerationMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullEnumerationMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEnumerationMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<Safir::Dob::Typesystem::Operations::GetEnumerationValueName
                                                                    (DotsTest::TestEnum::EnumerationTypeId,
                                                                     DotsTest::MemberArraysProperty::GetEnumerationMember(MIA,ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEnumerationMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEnumerationMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEnumerationMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEnumerationMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << Safir::Dob::Typesystem::Operations::GetEnumerationValueName
                                                                     (DotsTest::TestEnum::EnumerationTypeId,
                                                                      DotsTest::MemberArraysProperty::GetEnumerationMember(EO,ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEnumerationMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEnumerationMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEnumerationMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEnumerationMember (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << Safir::Dob::Typesystem::Operations::GetEnumerationValueName
                                                                     (DotsTest::TestEnum::EnumerationTypeId,
                                                                      DotsTest::MemberArraysProperty::GetEnumerationMember(AEO,ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEnumerationMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEnumerationMember (AEO,ix);
    }

    // SetNull test
    MT1->EnumerationMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullEnumerationMember(MT2);
    MA1->EnumerationMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullEnumerationMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->EnumerationMember().IsNull() && DotsTest::MemberTypesProperty::IsNullEnumerationMember (MT2) &&
        MA1->EnumerationMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullEnumerationMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}

const std::wstring SafirR_str = std::wstring(L"Safir") + (wchar_t)0x00ae;
const std::wstring RrifaS_str = std::wstring(SafirR_str.rbegin(),SafirR_str.rend());

const std::wstring CheckStr(const std::wstring& str)
{
    if (str == SafirR_str)
    {
        return L"<correct>";
    }
    else
    {
        return L"<INCORRECT!>";
    }
}

const std::wstring CheckStrReverse(const std::wstring& str)
{
    if (str == RrifaS_str)
    {
        return L"<correct>";
    }
    else
    {
        return L"<INCORRECT!>";
    }
}






void Test_String()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"String");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::StringMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::StringMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::StringParameterArraySize() == 2 &&
         DotsTest::MemberArrays::StringMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::StringMemberArraySize(MA1) == 2)<<std::endl;
    std::wcout<<"MaxStringLength Size Ok (10): "<<
        (DotsTest::MemberTypes::StringMemberMaxStringLength() == 10 &&
         DotsTest::MemberArrays::StringMemberMaxStringLength() == 10 )<<std::endl;
    Null_Ok  = MT1->StringMember().IsNull();
    In_Req_Ok  = !MT1->StringMember().IsChanged();
    MT1->StringMember().SetVal(DotsTest::ParameterTypes::StringParameter());
    Null_Ok  = Null_Ok && !MT1->StringMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->StringMember().IsChanged();


    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::StringMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::StringMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::StringMemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("StringParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("StringParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("StringParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullStringMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedStringMember (MT2);
    DotsTest::MemberTypesProperty::SetStringMember(MT2, MT1->StringMember().GetVal());
    std::wcout<<"Val: "<<CheckStr(DotsTest::MemberTypesProperty::GetStringMember (MT2))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullStringMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedStringMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullStringMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedStringMember(MI);
    DotsTest::MemberTypesProperty::SetStringMember(MI,MT2->StringMember().GetVal());
    std::wcout<<"Item Val: "<<CheckStr(DotsTest::MemberTypesProperty::GetStringMember(MI))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullStringMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedStringMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullStringMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedStringMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetStringMember(MIA,MT2->StringMember().GetVal());
    std::wcout<<"Item Array Val: "<<CheckStr(MIA->TypesItemArray()[1]->StringMember().GetVal())<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullStringMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedStringMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullStringMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedStringMember(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetStringMember(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullStringMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedStringMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullStringMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedStringMember(AEO);
    std::wcout<<"Property Parameter Val: " << CheckStr(DotsTest::MemberTypesProperty::GetStringMember(AEO))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullStringMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedStringMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::StringParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->StringMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->StringMember()[ix].IsChanged();
        MA1->StringMember()[ix].SetVal(DotsTest::ParameterArrays::StringParameter (ix));
        Null_Ok  = Null_Ok && !MA1->StringMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->StringMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullStringMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedStringMember (MA2, ix);
        MA2->StringMember()[ix].SetVal(MA1->StringMember()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullStringMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedStringMember (MA2, ix);

        std::wstring val = ix == 0?
            CheckStr(DotsTest::MemberArraysProperty::GetStringMember(MA2, ix)) :
            CheckStrReverse(DotsTest::MemberArraysProperty::GetStringMember(MA2, ix));
        std::wcout<<"Val "<<ix<<": "<<val<<std::endl;
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullStringMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedStringMember (MI,ix);

        DotsTest::ArraysItem item;
        item.StringMember()[ix].SetVal(MA1->StringMember()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());

        val = ix == 0?
            CheckStr(DotsTest::MemberArraysProperty::GetStringMember(MI, ix)) :
            CheckStrReverse(DotsTest::MemberArraysProperty::GetStringMember(MI, ix));
        std::wcout<<"Array Item Val "<<ix <<": "<<val<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullStringMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedStringMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullStringMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedStringMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());

        val = ix == 0?
            CheckStr(DotsTest::MemberArraysProperty::GetStringMember (MIA,ix)) :
            CheckStrReverse(DotsTest::MemberArraysProperty::GetStringMember (MIA,ix));

        std::wcout<<"Array Item Array Val "<<ix <<": "<<val<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullStringMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedStringMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullStringMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedStringMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetStringMember(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullStringMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedStringMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullStringMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedStringMember (AEO,ix);
        val = ix == 0?
            CheckStr(DotsTest::MemberArraysProperty::GetStringMember(AEO,ix)) :
            CheckStrReverse(DotsTest::MemberArraysProperty::GetStringMember(AEO,ix));
        std::wcout<<"Parameter Array Val "<<ix <<": " << val<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullStringMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedStringMember (AEO,ix);
    }

    // SetNull test
    MT1->StringMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullStringMember(MT2);
    MA1->StringMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullStringMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->StringMember().IsNull() && DotsTest::MemberTypesProperty::IsNullStringMember (MT2) &&
        MA1->StringMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullStringMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;


}


void Test_EntityId()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"EntityId");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::EntityIdMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::EntityIdMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::EntityIdParameterArraySize() == 2 &&
         DotsTest::MemberArrays::EntityIdMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::EntityIdMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->EntityIdMember().IsNull();
    In_Req_Ok  = !MT1->EntityIdMember().IsChanged();
    MT1->EntityIdMember().SetVal(DotsTest::ParameterTypes::EntityIdParameter());
    Null_Ok  = Null_Ok && !MT1->EntityIdMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->EntityIdMember().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::EntityIdMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::EntityIdMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::EntityIdMemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("EntityIdParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("EntityIdParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("EntityIdParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullEntityIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEntityIdMember (MT2);
    DotsTest::MemberTypesProperty::SetEntityIdMember(MT2, MT1->EntityIdMember().GetVal());
    std::wcout<<"Val: " << DotsTest::MemberTypesProperty::GetEntityIdMember (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEntityIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEntityIdMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullEntityIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEntityIdMember(MI);
    DotsTest::MemberTypesProperty::SetEntityIdMember(MI,MT2->EntityIdMember().GetVal());
    std::wcout<<"Item Val: " << DotsTest::MemberTypesProperty::GetEntityIdMember(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEntityIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEntityIdMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullEntityIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEntityIdMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetEntityIdMember(MIA,MT2->EntityIdMember().GetVal());
    std::wcout<<"Item Array Val:"
              << MIA->TypesItemArray()[1]->EntityIdMember().GetVal() << std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEntityIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedEntityIdMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEntityIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEntityIdMember(EO);
    std::wcout<<"Property Parameter Val: "<<DotsTest::MemberTypesProperty::GetEntityIdMember(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEntityIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEntityIdMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEntityIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEntityIdMember(AEO);
    std::wcout<<"Property Parameter Val: "<<DotsTest::MemberTypesProperty::GetEntityIdMember(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullEntityIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedEntityIdMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::EntityIdParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->EntityIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->EntityIdMember()[ix].IsChanged();
        MA1->EntityIdMember()[ix].SetVal(DotsTest::ParameterArrays::EntityIdParameter (ix));
        Null_Ok  = Null_Ok && !MA1->EntityIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->EntityIdMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullEntityIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEntityIdMember (MA2, ix);
        MA2->EntityIdMember()[ix].SetVal(MA1->EntityIdMember()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEntityIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEntityIdMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetEntityIdMember (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullEntityIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEntityIdMember (MI,ix);

        DotsTest::ArraysItem item;
        item.EntityIdMember()[ix].SetVal(MA1->EntityIdMember()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix<<": "<< DotsTest::MemberArraysProperty::GetEntityIdMember(MI, ix) <<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEntityIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEntityIdMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullEntityIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEntityIdMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix<<": " << DotsTest::MemberArraysProperty::GetEntityIdMember (MIA, ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEntityIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedEntityIdMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEntityIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEntityIdMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetEntityIdMember(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEntityIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEntityIdMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEntityIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEntityIdMember (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetEntityIdMember(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullEntityIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedEntityIdMember (AEO,ix);
    }

    // SetNull test
    MT1->EntityIdMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullEntityIdMember(MT2);
    MA1->EntityIdMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullEntityIdMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->EntityIdMember().IsNull() && DotsTest::MemberTypesProperty::IsNullEntityIdMember (MT2) &&
        MA1->EntityIdMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullEntityIdMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_TypeId()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"TypeId");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::TypeIdMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::TypeIdMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::TypeIdParameterArraySize() == 2 &&
         DotsTest::MemberArrays::TypeIdMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::TypeIdMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->TypeIdMember().IsNull();
    In_Req_Ok  = !MT1->TypeIdMember().IsChanged();
    MT1->TypeIdMember().SetVal(DotsTest::ParameterTypes::TypeIdParameter());
    Null_Ok  = Null_Ok && !MT1->TypeIdMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->TypeIdMember().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::TypeIdMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::TypeIdMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::TypeIdMemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("TypeIdParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("TypeIdParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("TypeIdParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullTypeIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTypeIdMember (MT2);
    DotsTest::MemberTypesProperty::SetTypeIdMember(MT2, MT1->TypeIdMember().GetVal());
    std::wcout<<"Val: "<<Safir::Dob::Typesystem::Operations::GetName(
                                                                                 DotsTest::MemberTypesProperty::GetTypeIdMember (MT2))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTypeIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTypeIdMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullTypeIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTypeIdMember(MI);
    DotsTest::MemberTypesProperty::SetTypeIdMember(MI,MT2->TypeIdMember().GetVal());
    std::wcout<<"Item Val: "<<Safir::Dob::Typesystem::Operations::GetName(
                                                                                      DotsTest::MemberTypesProperty::GetTypeIdMember(MI))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTypeIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTypeIdMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullTypeIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTypeIdMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetTypeIdMember(MIA,MT2->TypeIdMember().GetVal());
    std::wcout<<"Item Array Val: "
             <<Safir::Dob::Typesystem::Operations::GetName(MIA->TypesItemArray()[1]->TypeIdMember().GetVal())<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTypeIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTypeIdMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTypeIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTypeIdMember(EO);
    std::wcout<<"Property Parameter Val: " << Safir::Dob::Typesystem::Operations::GetName(
                                                                                             DotsTest::MemberTypesProperty::GetTypeIdMember(EO))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTypeIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTypeIdMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTypeIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTypeIdMember(AEO);
    std::wcout<<"Property Parameter Val: " << Safir::Dob::Typesystem::Operations::GetName(
                                                                                             DotsTest::MemberTypesProperty::GetTypeIdMember(AEO))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTypeIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTypeIdMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::TypeIdParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->TypeIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->TypeIdMember()[ix].IsChanged();
        MA1->TypeIdMember()[ix].SetVal(DotsTest::ParameterArrays::TypeIdParameter (ix));
        Null_Ok  = Null_Ok && !MA1->TypeIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->TypeIdMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullTypeIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTypeIdMember (MA2, ix);
        MA2->TypeIdMember()[ix].SetVal(MA1->TypeIdMember()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTypeIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTypeIdMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<Safir::Dob::Typesystem::Operations::GetName(
                                                                                              DotsTest::MemberArraysProperty::GetTypeIdMember (MA2, ix))<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullTypeIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTypeIdMember (MI,ix);

        DotsTest::ArraysItem item;
        item.TypeIdMember()[ix].SetVal(MA1->TypeIdMember()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix<<": "<<Safir::Dob::Typesystem::Operations::GetName(
                                                                                                         DotsTest::MemberArraysProperty::GetTypeIdMember (MI, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTypeIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTypeIdMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullTypeIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTypeIdMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix<<": "<<Safir::Dob::Typesystem::Operations::GetName(
                                                                                                               DotsTest::MemberArraysProperty::GetTypeIdMember (MIA, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTypeIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTypeIdMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTypeIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTypeIdMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix<<": "<<Safir::Dob::Typesystem::Operations::GetName(
                                                                                                              DotsTest::MemberArraysProperty::GetTypeIdMember (EO, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTypeIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTypeIdMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTypeIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTypeIdMember (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix<<": "<<Safir::Dob::Typesystem::Operations::GetName(
                                                                                                              DotsTest::MemberArraysProperty::GetTypeIdMember (AEO, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTypeIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTypeIdMember (AEO,ix);
    }

    // SetNull test
    MT1->TypeIdMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullTypeIdMember(MT2);
    MA1->TypeIdMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullTypeIdMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->TypeIdMember().IsNull() && DotsTest::MemberTypesProperty::IsNullTypeIdMember (MT2) &&
        MA1->TypeIdMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullTypeIdMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;


}



void Test_InstanceId()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"InstanceId");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::InstanceIdMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::InstanceIdMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::InstanceIdParameterArraySize() == 2 &&
         DotsTest::MemberArrays::InstanceIdMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::InstanceIdMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->InstanceIdMember().IsNull();
    In_Req_Ok  = !MT1->InstanceIdMember().IsChanged();
    MT1->InstanceIdMember().SetVal(DotsTest::ParameterTypes::InstanceIdParameter());
    Null_Ok  = Null_Ok && !MT1->InstanceIdMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->InstanceIdMember().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::InstanceIdMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::InstanceIdMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::InstanceIdMemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("InstanceIdParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("InstanceIdParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("InstanceIdParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullInstanceIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInstanceIdMember (MT2);
    DotsTest::MemberTypesProperty::SetInstanceIdMember(MT2, MT1->InstanceIdMember().GetVal());
    std::wcout<<"Val: " << DotsTest::MemberTypesProperty::GetInstanceIdMember (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInstanceIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInstanceIdMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullInstanceIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInstanceIdMember(MI);
    DotsTest::MemberTypesProperty::SetInstanceIdMember(MI,MT2->InstanceIdMember().GetVal());
    std::wcout<<"Item Val: " << DotsTest::MemberTypesProperty::GetInstanceIdMember(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInstanceIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInstanceIdMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullInstanceIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInstanceIdMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetInstanceIdMember(MIA,MT2->InstanceIdMember().GetVal());
    std::wcout<<"Item Array Val:"
              << MIA->TypesItemArray()[1]->InstanceIdMember().GetVal() << std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInstanceIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedInstanceIdMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInstanceIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInstanceIdMember(EO);
    std::wcout<<"Property Parameter Val: "<<DotsTest::MemberTypesProperty::GetInstanceIdMember(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInstanceIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInstanceIdMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInstanceIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInstanceIdMember(AEO);
    std::wcout<<"Property Parameter Val: "<<DotsTest::MemberTypesProperty::GetInstanceIdMember(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullInstanceIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedInstanceIdMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::InstanceIdParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->InstanceIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->InstanceIdMember()[ix].IsChanged();
        MA1->InstanceIdMember()[ix].SetVal(DotsTest::ParameterArrays::InstanceIdParameter (ix));
        Null_Ok  = Null_Ok && !MA1->InstanceIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->InstanceIdMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullInstanceIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (MA2, ix);
        MA2->InstanceIdMember()[ix].SetVal(MA1->InstanceIdMember()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInstanceIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetInstanceIdMember (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullInstanceIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (MI,ix);

        DotsTest::ArraysItem item;
        item.InstanceIdMember()[ix].SetVal(MA1->InstanceIdMember()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix<<": "<< DotsTest::MemberArraysProperty::GetInstanceIdMember(MI, ix) <<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInstanceIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullInstanceIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix<<": " << DotsTest::MemberArraysProperty::GetInstanceIdMember (MIA, ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInstanceIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInstanceIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetInstanceIdMember(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInstanceIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInstanceIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetInstanceIdMember(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullInstanceIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedInstanceIdMember (AEO,ix);
    }

    // SetNull test
    MT1->InstanceIdMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullInstanceIdMember(MT2);
    MA1->InstanceIdMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullInstanceIdMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->InstanceIdMember().IsNull() && DotsTest::MemberTypesProperty::IsNullInstanceIdMember (MT2) &&
        MA1->InstanceIdMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullInstanceIdMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}
void Test_ChannelId()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"ChannelId");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::ChannelIdMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::ChannelIdMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::ChannelIdParameterArraySize() == 2 &&
         DotsTest::MemberArrays::ChannelIdMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::ChannelIdMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->ChannelIdMember().IsNull();
    In_Req_Ok  = !MT1->ChannelIdMember().IsChanged();
    MT1->ChannelIdMember().SetVal(DotsTest::ParameterTypes::ChannelIdParameter());
    Null_Ok  = Null_Ok && !MT1->ChannelIdMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->ChannelIdMember().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::ChannelIdMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::ChannelIdMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::ChannelIdMemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("ChannelIdParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("ChannelIdParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("ChannelIdParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullChannelIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedChannelIdMember (MT2);
    DotsTest::MemberTypesProperty::SetChannelIdMember(MT2, MT1->ChannelIdMember().GetVal());
    std::wcout<<"Val: " << DotsTest::MemberTypesProperty::GetChannelIdMember (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullChannelIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedChannelIdMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullChannelIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedChannelIdMember(MI);
    DotsTest::MemberTypesProperty::SetChannelIdMember(MI,MT2->ChannelIdMember().GetVal());
    std::wcout<<"Item Val: " << DotsTest::MemberTypesProperty::GetChannelIdMember(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullChannelIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedChannelIdMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullChannelIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedChannelIdMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetChannelIdMember(MIA,MT2->ChannelIdMember().GetVal());
    std::wcout<<"Item Array Val:"
              << MIA->TypesItemArray()[1]->ChannelIdMember().GetVal() << std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullChannelIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedChannelIdMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullChannelIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedChannelIdMember(EO);
    std::wcout<<"Property Parameter Val: "<<DotsTest::MemberTypesProperty::GetChannelIdMember(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullChannelIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedChannelIdMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullChannelIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedChannelIdMember(AEO);
    std::wcout<<"Property Parameter Val: "<<DotsTest::MemberTypesProperty::GetChannelIdMember(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullChannelIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedChannelIdMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::ChannelIdParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->ChannelIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->ChannelIdMember()[ix].IsChanged();
        MA1->ChannelIdMember()[ix].SetVal(DotsTest::ParameterArrays::ChannelIdParameter (ix));
        Null_Ok  = Null_Ok && !MA1->ChannelIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->ChannelIdMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullChannelIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedChannelIdMember (MA2, ix);
        MA2->ChannelIdMember()[ix].SetVal(MA1->ChannelIdMember()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullChannelIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedChannelIdMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetChannelIdMember (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullChannelIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedChannelIdMember (MI,ix);

        DotsTest::ArraysItem item;
        item.ChannelIdMember()[ix].SetVal(MA1->ChannelIdMember()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix<<": "<< DotsTest::MemberArraysProperty::GetChannelIdMember(MI, ix) <<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullChannelIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedChannelIdMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullChannelIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedChannelIdMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix<<": " << DotsTest::MemberArraysProperty::GetChannelIdMember (MIA, ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullChannelIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedChannelIdMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullChannelIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedChannelIdMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetChannelIdMember(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullChannelIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedChannelIdMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullChannelIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedChannelIdMember (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetChannelIdMember(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullChannelIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedChannelIdMember (AEO,ix);
    }

    // SetNull test
    MT1->ChannelIdMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullChannelIdMember(MT2);
    MA1->ChannelIdMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullChannelIdMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->ChannelIdMember().IsNull() && DotsTest::MemberTypesProperty::IsNullChannelIdMember (MT2) &&
        MA1->ChannelIdMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullChannelIdMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_HandlerId()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"HandlerId");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::HandlerIdMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::HandlerIdMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::HandlerIdParameterArraySize() == 2 &&
         DotsTest::MemberArrays::HandlerIdMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::HandlerIdMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->HandlerIdMember().IsNull();
    In_Req_Ok  = !MT1->HandlerIdMember().IsChanged();
    MT1->HandlerIdMember().SetVal(DotsTest::ParameterTypes::HandlerIdParameter());
    Null_Ok  = Null_Ok && !MT1->HandlerIdMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->HandlerIdMember().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::HandlerIdMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::HandlerIdMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::HandlerIdMemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("HandlerIdParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("HandlerIdParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("HandlerIdParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullHandlerIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHandlerIdMember (MT2);
    DotsTest::MemberTypesProperty::SetHandlerIdMember(MT2, MT1->HandlerIdMember().GetVal());
    std::wcout<<"Val: " << DotsTest::MemberTypesProperty::GetHandlerIdMember (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHandlerIdMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHandlerIdMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullHandlerIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHandlerIdMember(MI);
    DotsTest::MemberTypesProperty::SetHandlerIdMember(MI,MT2->HandlerIdMember().GetVal());
    std::wcout<<"Item Val: " << DotsTest::MemberTypesProperty::GetHandlerIdMember(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHandlerIdMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHandlerIdMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullHandlerIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHandlerIdMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetHandlerIdMember(MIA,MT2->HandlerIdMember().GetVal());
    std::wcout<<"Item Array Val:"
              << MIA->TypesItemArray()[1]->HandlerIdMember().GetVal() << std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHandlerIdMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHandlerIdMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHandlerIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHandlerIdMember(EO);
    std::wcout<<"Property Parameter Val: "<<DotsTest::MemberTypesProperty::GetHandlerIdMember(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHandlerIdMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHandlerIdMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHandlerIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHandlerIdMember(AEO);
    std::wcout<<"Property Parameter Val: "<<DotsTest::MemberTypesProperty::GetHandlerIdMember(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHandlerIdMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHandlerIdMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::HandlerIdParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->HandlerIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->HandlerIdMember()[ix].IsChanged();
        MA1->HandlerIdMember()[ix].SetVal(DotsTest::ParameterArrays::HandlerIdParameter (ix));
        Null_Ok  = Null_Ok && !MA1->HandlerIdMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->HandlerIdMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullHandlerIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (MA2, ix);
        MA2->HandlerIdMember()[ix].SetVal(MA1->HandlerIdMember()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHandlerIdMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetHandlerIdMember (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullHandlerIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (MI,ix);

        DotsTest::ArraysItem item;
        item.HandlerIdMember()[ix].SetVal(MA1->HandlerIdMember()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix<<": "<< DotsTest::MemberArraysProperty::GetHandlerIdMember(MI, ix) <<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHandlerIdMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullHandlerIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix<<": " << DotsTest::MemberArraysProperty::GetHandlerIdMember (MIA, ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHandlerIdMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHandlerIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetHandlerIdMember(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHandlerIdMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHandlerIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetHandlerIdMember(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHandlerIdMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHandlerIdMember (AEO,ix);
    }

    // SetNull test
    MT1->HandlerIdMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullHandlerIdMember(MT2);
    MA1->HandlerIdMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullHandlerIdMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->HandlerIdMember().IsNull() && DotsTest::MemberTypesProperty::IsNullHandlerIdMember (MT2) &&
        MA1->HandlerIdMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullHandlerIdMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}

void Test_Object()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Object");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::ObjectMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::ObjectMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::ObjectParameterArraySize() == 2 &&
         DotsTest::MemberArrays::ObjectMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::ObjectMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->ObjectMember().IsNull();
    In_Req_Ok  = !MT1->ObjectMember().IsChanged();

    Safir::Dob::Typesystem::ObjectPtr op=DotsTest::ParameterTypes::ObjectParameter();

    MT1->ObjectMember().SetPtr(DotsTest::ParameterTypes::ObjectParameter());
    Null_Ok  = Null_Ok && !MT1->ObjectMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->ObjectMember().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::ObjectMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::ObjectMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::ObjectMemberMemberIndex()) <<std::endl;
    std::wcout<<"GetTypeId: " <<Safir::Dob::Typesystem::Members::GetTypeId(DotsTest::MemberTypes::ClassTypeId, DotsTest::MemberTypes::ObjectMemberMemberIndex())<<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("ObjectParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("ObjectParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("ObjectParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;

    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullObjectMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedObjectMember (MT2);
    DotsTest::MemberTypesProperty::SetObjectMember(MT2, MT1->ObjectMember().GetPtr());
    std::wcout<<"Val: "<<Safir::Dob::Typesystem::Serialization::ToXml
                                     (DotsTest::MemberTypesProperty::GetObjectMember (MT2))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullObjectMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedObjectMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullObjectMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedObjectMember(MI);
    DotsTest::MemberTypesProperty::SetObjectMember(MI, MT2->ObjectMember().GetPtr());
    std::wcout<<"Item Val: "<<Safir::Dob::Typesystem::Serialization::ToXml
                                          (DotsTest::MemberTypesProperty::GetObjectMember(MI))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullObjectMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedObjectMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullObjectMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedObjectMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetObjectMember(MIA,MT2->ObjectMember().GetPtr());
    std::wcout<<"Item Array Val: "<<Safir::Dob::Typesystem::Serialization::ToXml
                                                (MIA->TypesItemArray()[1]->ObjectMember().GetPtr())<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullObjectMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedObjectMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullObjectMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedObjectMember(EO);
    std::wcout<<"Property Parameter Val: "<<Safir::Dob::Typesystem::Serialization::ToXml
                                               (DotsTest::MemberTypesProperty::GetObjectMember(EO))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullObjectMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedObjectMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullObjectMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedObjectMember(AEO);
    std::wcout<<"Property Parameter Val: "<<Safir::Dob::Typesystem::Serialization::ToXml
                                               (DotsTest::MemberTypesProperty::GetObjectMember(AEO))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullObjectMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedObjectMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::ObjectParameterArraySize(); ix++)
    {

        //MemberArray
        Null_Ok  = Null_Ok && MA1->ObjectMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->ObjectMember()[ix].IsChanged();
        MA1->ObjectMember()[ix].SetPtr(DotsTest::ParameterArrays::ObjectParameter (ix));
        Null_Ok  = Null_Ok && !MA1->ObjectMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->ObjectMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullObjectMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedObjectMember (MA2, ix);
        MA2->ObjectMember()[ix].SetPtr(MA1->ObjectMember()[ix].GetPtr());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullObjectMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedObjectMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                               DotsTest::MemberArraysProperty::GetObjectMember (MA2, ix))<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullObjectMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedObjectMember (MI,ix);

        DotsTest::ArraysItem item;
        item.ObjectMember()[ix].SetPtr(MA1->ObjectMember()[ix].GetPtr());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                                          DotsTest::MemberArraysProperty::GetObjectMember (MI, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullObjectMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedObjectMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullObjectMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedObjectMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                                                DotsTest::MemberArraysProperty::GetObjectMember (MIA, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullObjectMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedObjectMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullObjectMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedObjectMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                                               DotsTest::MemberArraysProperty::GetObjectMember (EO, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullObjectMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedObjectMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullObjectMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedObjectMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                                               DotsTest::MemberArraysProperty::GetObjectMember (EO, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullObjectMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedObjectMember (EO,ix);
    }

    // SetNull test
    MT1->ObjectMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullObjectMember(MT2);
    MA1->ObjectMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullObjectMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->ObjectMember().IsNull() && DotsTest::MemberTypesProperty::IsNullObjectMember (MT2) &&
        MA1->ObjectMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullObjectMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Binary()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Binary");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::BinaryMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::BinaryMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::BinaryParameterArraySize() == 2 &&
         DotsTest::MemberArrays::BinaryMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::BinaryMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->BinaryMember().IsNull();
    In_Req_Ok  = !MT1->BinaryMember().IsChanged();
    MT1->BinaryMember().SetVal(DotsTest::ParameterTypes::BinaryParameter());
    Null_Ok  = Null_Ok && !MT1->BinaryMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->BinaryMember().IsChanged();


    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::BinaryMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::BinaryMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::BinaryMemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("BinaryParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("BinaryParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("BinaryParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullBinaryMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBinaryMember (MT2);
    DotsTest::MemberTypesProperty::SetBinaryMember(MT2, MT1->BinaryMember().GetVal());
    std::wcout<<"Val: "<<Safir::Dob::Typesystem::Utilities::BinaryToBase64(DotsTest::MemberTypesProperty::GetBinaryMember (MT2)).c_str()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBinaryMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBinaryMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullBinaryMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBinaryMember(MI);
    DotsTest::MemberTypesProperty::SetBinaryMember(MI,MT2->BinaryMember().GetVal());
    std::wcout<<"Item Val: "<<Safir::Dob::Typesystem::Utilities::BinaryToBase64(DotsTest::MemberTypesProperty::GetBinaryMember(MI)).c_str()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBinaryMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBinaryMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullBinaryMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBinaryMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetBinaryMember(MIA,MT2->BinaryMember().GetVal());
    std::wcout<<"Item Array Val: "<<Safir::Dob::Typesystem::Utilities::BinaryToBase64(MIA->TypesItemArray()[1]->BinaryMember().GetVal()).c_str()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBinaryMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedBinaryMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBinaryMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBinaryMember(EO);
    std::wcout<<"Property Parameter Val: " << Safir::Dob::Typesystem::Utilities::BinaryToBase64(DotsTest::MemberTypesProperty::GetBinaryMember(EO)).c_str()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBinaryMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBinaryMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBinaryMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBinaryMember(AEO);
    std::wcout<<"Property Parameter Val: " << Safir::Dob::Typesystem::Utilities::BinaryToBase64(DotsTest::MemberTypesProperty::GetBinaryMember(AEO)).c_str()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullBinaryMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedBinaryMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::BinaryParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->BinaryMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->BinaryMember()[ix].IsChanged();
        MA1->BinaryMember()[ix].SetVal(DotsTest::ParameterArrays::BinaryParameter (ix));
        Null_Ok  = Null_Ok && !MA1->BinaryMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->BinaryMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullBinaryMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBinaryMember (MA2, ix);
        MA2->BinaryMember()[ix].SetVal(MA1->BinaryMember()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBinaryMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBinaryMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<Safir::Dob::Typesystem::Utilities::BinaryToBase64(DotsTest::MemberArraysProperty::GetBinaryMember(MA2, ix)).c_str()<<std::endl;
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullBinaryMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBinaryMember (MI,ix);

        DotsTest::ArraysItem item;
        item.BinaryMember()[ix].SetVal(MA1->BinaryMember()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<Safir::Dob::Typesystem::Utilities::BinaryToBase64(DotsTest::MemberArraysProperty::GetBinaryMember(MI, ix)).c_str()<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBinaryMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBinaryMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullBinaryMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBinaryMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<Safir::Dob::Typesystem::Utilities::BinaryToBase64(DotsTest::MemberArraysProperty::GetBinaryMember (MIA,ix)).c_str()<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBinaryMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedBinaryMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBinaryMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBinaryMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << Safir::Dob::Typesystem::Utilities::BinaryToBase64(DotsTest::MemberArraysProperty::GetBinaryMember(EO,ix)).c_str()<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBinaryMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBinaryMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBinaryMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBinaryMember (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << Safir::Dob::Typesystem::Utilities::BinaryToBase64(DotsTest::MemberArraysProperty::GetBinaryMember(AEO,ix)).c_str()<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullBinaryMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedBinaryMember (AEO,ix);
    }

    // SetNull test
    MT1->BinaryMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullBinaryMember(MT2);
    MA1->BinaryMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullBinaryMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->BinaryMember().IsNull() && DotsTest::MemberTypesProperty::IsNullBinaryMember (MT2) &&
        MA1->BinaryMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullBinaryMember (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;


}
void Test_TestClass()
{
    // Locals
    bool Null_Ok, In_Req_Ok;

    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"TestClass");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::TestClassMemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::TestClassMemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::TestClassParameterArraySize() == 2 &&
         DotsTest::MemberArrays::TestClassMemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::TestClassMemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->TestClassMember().IsNull();
    In_Req_Ok  = !MT1->TestClassMember().IsChanged();
    MT1->TestClassMember().SetPtr(DotsTest::ParameterTypes::TestClassParameter());
    Null_Ok  = Null_Ok && !MT1->TestClassMember().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->TestClassMember().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::TestClassMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::TestClassMemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::TestClassMemberMemberIndex()) <<std::endl;
    std::wcout<<"GetTypeId: " <<Safir::Dob::Typesystem::Members::GetTypeId(DotsTest::MemberTypes::ClassTypeId, DotsTest::MemberTypes::TestClassMemberMemberIndex())<<std::endl;
    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("TestClassParameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("TestClassParameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("TestClassParameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;

    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullTestClassMember (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTestClassMember (MT2);
    DotsTest::MemberTypesProperty::SetTestClassMember(MT2, MT1->TestClassMember().GetPtr());
    std::wcout<<"Val: "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                  DotsTest::MemberTypesProperty::GetTestClassMember (MT2))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTestClassMember (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTestClassMember (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullTestClassMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTestClassMember(MI);
    DotsTest::MemberTypesProperty::SetTestClassMember(MI,MT2->TestClassMember().GetPtr());
    std::wcout<<"Item Val: "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                       DotsTest::MemberTypesProperty::GetTestClassMember(MI))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTestClassMember(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTestClassMember(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullTestClassMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTestClassMember(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetTestClassMember(MIA,MT2->TestClassMember().GetPtr());
    std::wcout<<"Item Array Val: "
             <<Safir::Dob::Typesystem::Serialization::ToXml(MIA->TypesItemArray()[1]->TestClassMember().GetPtr())<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTestClassMember(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedTestClassMember(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTestClassMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTestClassMember(EO);
    std::wcout<<"Property Parameter Val: "<<Safir::Dob::Typesystem::Serialization::ToXml(DotsTest::MemberTypesProperty::GetTestClassMember(EO))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTestClassMember(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTestClassMember(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTestClassMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTestClassMember(AEO);
    std::wcout<<"Property Parameter Val: "<<Safir::Dob::Typesystem::Serialization::ToXml(DotsTest::MemberTypesProperty::GetTestClassMember(AEO))<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullTestClassMember(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedTestClassMember(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::TestClassParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->TestClassMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->TestClassMember()[ix].IsChanged();
        MA1->TestClassMember()[ix].SetPtr(DotsTest::ParameterArrays::TestClassParameter (ix));
        Null_Ok  = Null_Ok && !MA1->TestClassMember()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->TestClassMember()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullTestClassMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTestClassMember (MA2, ix);
        MA2->TestClassMember()[ix].SetPtr(MA1->TestClassMember()[ix].GetPtr());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTestClassMember (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTestClassMember (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                     DotsTest::MemberArraysProperty::GetTestClassMember (MA2, ix))<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullTestClassMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTestClassMember (MI,ix);

        DotsTest::ArraysItem item;
        item.TestClassMember()[ix].SetPtr(MA1->TestClassMember()[ix].GetPtr());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                                DotsTest::MemberArraysProperty::GetTestClassMember (MI, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTestClassMember (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTestClassMember (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullTestClassMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTestClassMember (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                                      DotsTest::MemberArraysProperty::GetTestClassMember (MIA, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTestClassMember (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedTestClassMember (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTestClassMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTestClassMember (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                                     DotsTest::MemberArraysProperty::GetTestClassMember (EO, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTestClassMember (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTestClassMember (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTestClassMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTestClassMember (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix<<": "<<Safir::Dob::Typesystem::Serialization::ToXml(
                                                                                                     DotsTest::MemberArraysProperty::GetTestClassMember (AEO, ix))<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullTestClassMember (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedTestClassMember (AEO,ix);
    }

    // SetNull test
    MT1->TestClassMember().SetNull();
    DotsTest::MemberTypesProperty::SetNullTestClassMember(MT2);
    MA1->TestClassMember()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullTestClassMember(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->TestClassMember().IsNull() && DotsTest::MemberTypesProperty::IsNullTestClassMember (MT2) &&
        MA1->TestClassMember()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullTestClassMember (MA2, 1);

    // Makesome tests concerning Set/GetChangedHere
    MT1->TestClassMember().SetPtr(DotsTest::ParameterTypes::TestClassParameter());
    MT1->TestClassMember().SetChanged(false);
    MT1->TestClassMember()->MyInt().SetVal(3);
    In_Req_Ok = In_Req_Ok && MT1->TestClassMember().IsChanged();
    In_Req_Ok = In_Req_Ok && !MT1->TestClassMember().IsChangedHere();
    MT1->TestClassMember().SetChanged(false);
    MT1->TestClassMember().SetChangedHere(true);
    In_Req_Ok = In_Req_Ok && MT1->TestClassMember().IsChangedHere();
    In_Req_Ok = In_Req_Ok && !MT1->TestClassMember()->MyInt().IsChanged();

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Ampere32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;

    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Ampere32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Ampere32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Ampere32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Ampere32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Ampere32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Ampere32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Ampere32Member().IsNull();
    In_Req_Ok  = !MT1->Ampere32Member().IsChanged();
    MT1->Ampere32Member().SetVal(DotsTest::ParameterTypes::Ampere32Parameter());
    Null_Ok  = Null_Ok && !MT1->Ampere32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Ampere32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Ampere32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Ampere32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Ampere32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Ampere32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Ampere32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Ampere32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullAmpere32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere32Member (MT2);
    DotsTest::MemberTypesProperty::SetAmpere32Member(MT2, MT1->Ampere32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetAmpere32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullAmpere32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere32Member(MI);
    DotsTest::MemberTypesProperty::SetAmpere32Member(MI,MT2->Ampere32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetAmpere32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullAmpere32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetAmpere32Member(MIA,MT2->Ampere32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Ampere32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetAmpere32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetAmpere32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Ampere32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Ampere32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Ampere32Member()[ix].IsChanged();
        MA1->Ampere32Member()[ix].SetVal(DotsTest::ParameterArrays::Ampere32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Ampere32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Ampere32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullAmpere32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere32Member (MA2, ix);
        MA2->Ampere32Member()[ix].SetVal(MA1->Ampere32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetAmpere32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullAmpere32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Ampere32Member()[ix].SetVal(MA1->Ampere32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetAmpere32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullAmpere32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetAmpere32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetAmpere32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetAmpere32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere32Member (AEO,ix);
    }

    // SetNull test
    MT1->Ampere32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullAmpere32Member(MT2);
    MA1->Ampere32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullAmpere32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Ampere32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullAmpere32Member (MT2) &&
        MA1->Ampere32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullAmpere32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_CubicMeter32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"CubicMeter32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::CubicMeter32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::CubicMeter32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::CubicMeter32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::CubicMeter32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::CubicMeter32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->CubicMeter32Member().IsNull();
    In_Req_Ok  = !MT1->CubicMeter32Member().IsChanged();
    MT1->CubicMeter32Member().SetVal(DotsTest::ParameterTypes::CubicMeter32Parameter());
    Null_Ok  = Null_Ok && !MT1->CubicMeter32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->CubicMeter32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::CubicMeter32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::CubicMeter32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::CubicMeter32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("CubicMeter32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("CubicMeter32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("CubicMeter32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullCubicMeter32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member (MT2);
    DotsTest::MemberTypesProperty::SetCubicMeter32Member(MT2, MT1->CubicMeter32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetCubicMeter32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullCubicMeter32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member(MI);
    DotsTest::MemberTypesProperty::SetCubicMeter32Member(MI,MT2->CubicMeter32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetCubicMeter32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullCubicMeter32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetCubicMeter32Member(MIA,MT2->CubicMeter32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->CubicMeter32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetCubicMeter32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetCubicMeter32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::CubicMeter32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->CubicMeter32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->CubicMeter32Member()[ix].IsChanged();
        MA1->CubicMeter32Member()[ix].SetVal(DotsTest::ParameterArrays::CubicMeter32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->CubicMeter32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->CubicMeter32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (MA2, ix);
        MA2->CubicMeter32Member()[ix].SetVal(MA1->CubicMeter32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetCubicMeter32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.CubicMeter32Member()[ix].SetVal(MA1->CubicMeter32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetCubicMeter32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetCubicMeter32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetCubicMeter32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetCubicMeter32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter32Member (AEO,ix);
    }

    // SetNull test
    MT1->CubicMeter32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullCubicMeter32Member(MT2);
    MA1->CubicMeter32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullCubicMeter32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->CubicMeter32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullCubicMeter32Member (MT2) &&
        MA1->CubicMeter32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullCubicMeter32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Hertz32()
{

    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Hertz32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Hertz32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Hertz32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Hertz32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Hertz32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Hertz32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Hertz32Member().IsNull();
    In_Req_Ok  = !MT1->Hertz32Member().IsChanged();
    MT1->Hertz32Member().SetVal(DotsTest::ParameterTypes::Hertz32Parameter());
    Null_Ok  = Null_Ok && !MT1->Hertz32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Hertz32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Hertz32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Hertz32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Hertz32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Hertz32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Hertz32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Hertz32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullHertz32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz32Member (MT2);
    DotsTest::MemberTypesProperty::SetHertz32Member(MT2, MT1->Hertz32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetHertz32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullHertz32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz32Member(MI);
    DotsTest::MemberTypesProperty::SetHertz32Member(MI,MT2->Hertz32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetHertz32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullHertz32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetHertz32Member(MIA,MT2->Hertz32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Hertz32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetHertz32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetHertz32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Hertz32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Hertz32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Hertz32Member()[ix].IsChanged();
        MA1->Hertz32Member()[ix].SetVal(DotsTest::ParameterArrays::Hertz32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Hertz32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Hertz32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullHertz32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz32Member (MA2, ix);
        MA2->Hertz32Member()[ix].SetVal(MA1->Hertz32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetHertz32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullHertz32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Hertz32Member()[ix].SetVal(MA1->Hertz32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetHertz32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullHertz32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetHertz32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetHertz32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetHertz32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz32Member (AEO,ix);
    }

    // SetNull test
    MT1->Hertz32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullHertz32Member(MT2);
    MA1->Hertz32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullHertz32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Hertz32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullHertz32Member (MT2) &&
        MA1->Hertz32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullHertz32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Joule32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Joule32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Joule32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Joule32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Joule32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Joule32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Joule32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Joule32Member().IsNull();
    In_Req_Ok  = !MT1->Joule32Member().IsChanged();
    MT1->Joule32Member().SetVal(DotsTest::ParameterTypes::Joule32Parameter());
    Null_Ok  = Null_Ok && !MT1->Joule32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Joule32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Joule32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Joule32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Joule32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Joule32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Joule32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Joule32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullJoule32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule32Member (MT2);
    DotsTest::MemberTypesProperty::SetJoule32Member(MT2, MT1->Joule32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetJoule32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullJoule32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule32Member(MI);
    DotsTest::MemberTypesProperty::SetJoule32Member(MI,MT2->Joule32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetJoule32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullJoule32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetJoule32Member(MIA,MT2->Joule32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Joule32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetJoule32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetJoule32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Joule32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Joule32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Joule32Member()[ix].IsChanged();
        MA1->Joule32Member()[ix].SetVal(DotsTest::ParameterArrays::Joule32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Joule32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Joule32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullJoule32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule32Member (MA2, ix);
        MA2->Joule32Member()[ix].SetVal(MA1->Joule32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetJoule32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullJoule32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Joule32Member()[ix].SetVal(MA1->Joule32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetJoule32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullJoule32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetJoule32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetJoule32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetJoule32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule32Member (AEO,ix);
    }

    // SetNull test
    MT1->Joule32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullJoule32Member(MT2);
    MA1->Joule32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullJoule32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Joule32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullJoule32Member (MT2) &&
        MA1->Joule32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullJoule32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Kelvin32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Kelvin32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Kelvin32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Kelvin32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Kelvin32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Kelvin32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Kelvin32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Kelvin32Member().IsNull();
    In_Req_Ok  = !MT1->Kelvin32Member().IsChanged();
    MT1->Kelvin32Member().SetVal(DotsTest::ParameterTypes::Kelvin32Parameter());
    Null_Ok  = Null_Ok && !MT1->Kelvin32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Kelvin32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Kelvin32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Kelvin32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Kelvin32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Kelvin32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Kelvin32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Kelvin32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKelvin32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin32Member (MT2);
    DotsTest::MemberTypesProperty::SetKelvin32Member(MT2, MT1->Kelvin32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetKelvin32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKelvin32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin32Member(MI);
    DotsTest::MemberTypesProperty::SetKelvin32Member(MI,MT2->Kelvin32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetKelvin32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKelvin32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetKelvin32Member(MIA,MT2->Kelvin32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Kelvin32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetKelvin32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetKelvin32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Kelvin32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Kelvin32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Kelvin32Member()[ix].IsChanged();
        MA1->Kelvin32Member()[ix].SetVal(DotsTest::ParameterArrays::Kelvin32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Kelvin32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Kelvin32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKelvin32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin32Member (MA2, ix);
        MA2->Kelvin32Member()[ix].SetVal(MA1->Kelvin32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetKelvin32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKelvin32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Kelvin32Member()[ix].SetVal(MA1->Kelvin32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetKelvin32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKelvin32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetKelvin32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetKelvin32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetKelvin32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin32Member (AEO,ix);
    }

    // SetNull test
    MT1->Kelvin32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullKelvin32Member(MT2);
    MA1->Kelvin32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullKelvin32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Kelvin32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullKelvin32Member (MT2) &&
        MA1->Kelvin32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullKelvin32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Kilogram32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Kilogram32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Kilogram32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Kilogram32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Kilogram32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Kilogram32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Kilogram32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Kilogram32Member().IsNull();
    In_Req_Ok  = !MT1->Kilogram32Member().IsChanged();
    MT1->Kilogram32Member().SetVal(DotsTest::ParameterTypes::Kilogram32Parameter());
    Null_Ok  = Null_Ok && !MT1->Kilogram32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Kilogram32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Kilogram32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Kilogram32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Kilogram32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Kilogram32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Kilogram32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Kilogram32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKilogram32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram32Member (MT2);
    DotsTest::MemberTypesProperty::SetKilogram32Member(MT2, MT1->Kilogram32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetKilogram32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKilogram32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram32Member(MI);
    DotsTest::MemberTypesProperty::SetKilogram32Member(MI,MT2->Kilogram32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetKilogram32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKilogram32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetKilogram32Member(MIA,MT2->Kilogram32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Kilogram32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetKilogram32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetKilogram32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Kilogram32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Kilogram32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Kilogram32Member()[ix].IsChanged();
        MA1->Kilogram32Member()[ix].SetVal(DotsTest::ParameterArrays::Kilogram32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Kilogram32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Kilogram32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKilogram32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram32Member (MA2, ix);
        MA2->Kilogram32Member()[ix].SetVal(MA1->Kilogram32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetKilogram32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKilogram32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Kilogram32Member()[ix].SetVal(MA1->Kilogram32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetKilogram32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKilogram32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetKilogram32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetKilogram32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetKilogram32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram32Member (AEO,ix);
    }

    // SetNull test
    MT1->Kilogram32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullKilogram32Member(MT2);
    MA1->Kilogram32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullKilogram32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Kilogram32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullKilogram32Member (MT2) &&
        MA1->Kilogram32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullKilogram32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Meter32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Meter32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Meter32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Meter32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Meter32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Meter32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Meter32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Meter32Member().IsNull();
    In_Req_Ok  = !MT1->Meter32Member().IsChanged();
    MT1->Meter32Member().SetVal(DotsTest::ParameterTypes::Meter32Parameter());
    Null_Ok  = Null_Ok && !MT1->Meter32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Meter32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Meter32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Meter32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Meter32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Meter32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Meter32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Meter32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeter32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter32Member (MT2);
    DotsTest::MemberTypesProperty::SetMeter32Member(MT2, MT1->Meter32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetMeter32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeter32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter32Member(MI);
    DotsTest::MemberTypesProperty::SetMeter32Member(MI,MT2->Meter32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetMeter32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeter32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetMeter32Member(MIA,MT2->Meter32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Meter32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeter32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeter32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Meter32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Meter32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Meter32Member()[ix].IsChanged();
        MA1->Meter32Member()[ix].SetVal(DotsTest::ParameterArrays::Meter32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Meter32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Meter32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeter32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter32Member (MA2, ix);
        MA2->Meter32Member()[ix].SetVal(MA1->Meter32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetMeter32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeter32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Meter32Member()[ix].SetVal(MA1->Meter32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeter32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeter32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeter32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeter32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeter32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter32Member (AEO,ix);
    }

    // SetNull test
    MT1->Meter32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullMeter32Member(MT2);
    MA1->Meter32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullMeter32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Meter32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullMeter32Member (MT2) &&
        MA1->Meter32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullMeter32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_MeterPerSecond32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"MeterPerSecond32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::MeterPerSecond32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::MeterPerSecond32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::MeterPerSecond32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::MeterPerSecond32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::MeterPerSecond32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->MeterPerSecond32Member().IsNull();
    In_Req_Ok  = !MT1->MeterPerSecond32Member().IsChanged();
    MT1->MeterPerSecond32Member().SetVal(DotsTest::ParameterTypes::MeterPerSecond32Parameter());
    Null_Ok  = Null_Ok && !MT1->MeterPerSecond32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->MeterPerSecond32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::MeterPerSecond32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::MeterPerSecond32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::MeterPerSecond32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("MeterPerSecond32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("MeterPerSecond32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("MeterPerSecond32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member (MT2);
    DotsTest::MemberTypesProperty::SetMeterPerSecond32Member(MT2, MT1->MeterPerSecond32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetMeterPerSecond32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member(MI);
    DotsTest::MemberTypesProperty::SetMeterPerSecond32Member(MI,MT2->MeterPerSecond32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetMeterPerSecond32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetMeterPerSecond32Member(MIA,MT2->MeterPerSecond32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->MeterPerSecond32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeterPerSecond32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeterPerSecond32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::MeterPerSecond32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->MeterPerSecond32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->MeterPerSecond32Member()[ix].IsChanged();
        MA1->MeterPerSecond32Member()[ix].SetVal(DotsTest::ParameterArrays::MeterPerSecond32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->MeterPerSecond32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->MeterPerSecond32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (MA2, ix);
        MA2->MeterPerSecond32Member()[ix].SetVal(MA1->MeterPerSecond32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecond32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.MeterPerSecond32Member()[ix].SetVal(MA1->MeterPerSecond32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecond32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecond32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeterPerSecond32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeterPerSecond32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond32Member (AEO,ix);
    }

    // SetNull test
    MT1->MeterPerSecond32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullMeterPerSecond32Member(MT2);
    MA1->MeterPerSecond32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullMeterPerSecond32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->MeterPerSecond32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullMeterPerSecond32Member (MT2) &&
        MA1->MeterPerSecond32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullMeterPerSecond32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_MeterPerSecondSquared32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"MeterPerSecondSquared32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::MeterPerSecondSquared32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::MeterPerSecondSquared32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::MeterPerSecondSquared32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::MeterPerSecondSquared32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::MeterPerSecondSquared32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->MeterPerSecondSquared32Member().IsNull();
    In_Req_Ok  = !MT1->MeterPerSecondSquared32Member().IsChanged();
    MT1->MeterPerSecondSquared32Member().SetVal(DotsTest::ParameterTypes::MeterPerSecondSquared32Parameter());
    Null_Ok  = Null_Ok && !MT1->MeterPerSecondSquared32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->MeterPerSecondSquared32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::MeterPerSecondSquared32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::MeterPerSecondSquared32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::MeterPerSecondSquared32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("MeterPerSecondSquared32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("MeterPerSecondSquared32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("MeterPerSecondSquared32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member (MT2);
    DotsTest::MemberTypesProperty::SetMeterPerSecondSquared32Member(MT2, MT1->MeterPerSecondSquared32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetMeterPerSecondSquared32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member(MI);
    DotsTest::MemberTypesProperty::SetMeterPerSecondSquared32Member(MI,MT2->MeterPerSecondSquared32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetMeterPerSecondSquared32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetMeterPerSecondSquared32Member(MIA,MT2->MeterPerSecondSquared32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->MeterPerSecondSquared32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeterPerSecondSquared32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeterPerSecondSquared32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::MeterPerSecondSquared32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->MeterPerSecondSquared32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->MeterPerSecondSquared32Member()[ix].IsChanged();
        MA1->MeterPerSecondSquared32Member()[ix].SetVal(DotsTest::ParameterArrays::MeterPerSecondSquared32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->MeterPerSecondSquared32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->MeterPerSecondSquared32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (MA2, ix);
        MA2->MeterPerSecondSquared32Member()[ix].SetVal(MA1->MeterPerSecondSquared32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecondSquared32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.MeterPerSecondSquared32Member()[ix].SetVal(MA1->MeterPerSecondSquared32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecondSquared32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecondSquared32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeterPerSecondSquared32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeterPerSecondSquared32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared32Member (AEO,ix);
    }

    // SetNull test
    MT1->MeterPerSecondSquared32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullMeterPerSecondSquared32Member(MT2);
    MA1->MeterPerSecondSquared32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullMeterPerSecondSquared32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->MeterPerSecondSquared32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared32Member (MT2) &&
        MA1->MeterPerSecondSquared32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Newton32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Newton32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Newton32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Newton32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Newton32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Newton32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Newton32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Newton32Member().IsNull();
    In_Req_Ok  = !MT1->Newton32Member().IsChanged();
    MT1->Newton32Member().SetVal(DotsTest::ParameterTypes::Newton32Parameter());
    Null_Ok  = Null_Ok && !MT1->Newton32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Newton32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Newton32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Newton32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Newton32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Newton32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Newton32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Newton32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullNewton32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton32Member (MT2);
    DotsTest::MemberTypesProperty::SetNewton32Member(MT2, MT1->Newton32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetNewton32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullNewton32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton32Member(MI);
    DotsTest::MemberTypesProperty::SetNewton32Member(MI,MT2->Newton32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetNewton32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullNewton32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetNewton32Member(MIA,MT2->Newton32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Newton32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetNewton32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetNewton32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Newton32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Newton32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Newton32Member()[ix].IsChanged();
        MA1->Newton32Member()[ix].SetVal(DotsTest::ParameterArrays::Newton32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Newton32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Newton32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullNewton32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton32Member (MA2, ix);
        MA2->Newton32Member()[ix].SetVal(MA1->Newton32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetNewton32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullNewton32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Newton32Member()[ix].SetVal(MA1->Newton32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetNewton32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullNewton32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetNewton32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetNewton32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetNewton32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton32Member (AEO,ix);
    }

    // SetNull test
    MT1->Newton32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullNewton32Member(MT2);
    MA1->Newton32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullNewton32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Newton32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullNewton32Member (MT2) &&
        MA1->Newton32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullNewton32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Pascal32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Pascal32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Pascal32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Pascal32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Pascal32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Pascal32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Pascal32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Pascal32Member().IsNull();
    In_Req_Ok  = !MT1->Pascal32Member().IsChanged();
    MT1->Pascal32Member().SetVal(DotsTest::ParameterTypes::Pascal32Parameter());
    Null_Ok  = Null_Ok && !MT1->Pascal32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Pascal32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Pascal32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Pascal32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Pascal32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Pascal32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Pascal32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Pascal32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullPascal32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal32Member (MT2);
    DotsTest::MemberTypesProperty::SetPascal32Member(MT2, MT1->Pascal32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetPascal32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullPascal32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal32Member(MI);
    DotsTest::MemberTypesProperty::SetPascal32Member(MI,MT2->Pascal32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetPascal32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullPascal32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetPascal32Member(MIA,MT2->Pascal32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Pascal32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetPascal32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetPascal32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Pascal32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Pascal32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Pascal32Member()[ix].IsChanged();
        MA1->Pascal32Member()[ix].SetVal(DotsTest::ParameterArrays::Pascal32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Pascal32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Pascal32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullPascal32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal32Member (MA2, ix);
        MA2->Pascal32Member()[ix].SetVal(MA1->Pascal32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetPascal32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullPascal32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Pascal32Member()[ix].SetVal(MA1->Pascal32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetPascal32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullPascal32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetPascal32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetPascal32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetPascal32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal32Member (AEO,ix);
    }

    // SetNull test
    MT1->Pascal32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullPascal32Member(MT2);
    MA1->Pascal32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullPascal32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Pascal32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullPascal32Member (MT2) &&
        MA1->Pascal32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullPascal32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Radian32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Radian32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Radian32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Radian32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Radian32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Radian32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Radian32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Radian32Member().IsNull();
    In_Req_Ok  = !MT1->Radian32Member().IsChanged();
    MT1->Radian32Member().SetVal(DotsTest::ParameterTypes::Radian32Parameter());
    Null_Ok  = Null_Ok && !MT1->Radian32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Radian32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Radian32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Radian32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Radian32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Radian32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Radian32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Radian32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadian32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian32Member (MT2);
    DotsTest::MemberTypesProperty::SetRadian32Member(MT2, MT1->Radian32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetRadian32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadian32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian32Member(MI);
    DotsTest::MemberTypesProperty::SetRadian32Member(MI,MT2->Radian32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetRadian32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadian32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetRadian32Member(MIA,MT2->Radian32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Radian32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadian32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadian32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Radian32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Radian32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Radian32Member()[ix].IsChanged();
        MA1->Radian32Member()[ix].SetVal(DotsTest::ParameterArrays::Radian32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Radian32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Radian32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadian32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian32Member (MA2, ix);
        MA2->Radian32Member()[ix].SetVal(MA1->Radian32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetRadian32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadian32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Radian32Member()[ix].SetVal(MA1->Radian32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadian32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadian32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadian32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadian32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadian32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian32Member (AEO,ix);
    }

    // SetNull test
    MT1->Radian32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullRadian32Member(MT2);
    MA1->Radian32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullRadian32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Radian32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullRadian32Member (MT2) &&
        MA1->Radian32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullRadian32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_RadianPerSecond32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"RadianPerSecond32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::RadianPerSecond32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::RadianPerSecond32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::RadianPerSecond32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::RadianPerSecond32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::RadianPerSecond32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->RadianPerSecond32Member().IsNull();
    In_Req_Ok  = !MT1->RadianPerSecond32Member().IsChanged();
    MT1->RadianPerSecond32Member().SetVal(DotsTest::ParameterTypes::RadianPerSecond32Parameter());
    Null_Ok  = Null_Ok && !MT1->RadianPerSecond32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->RadianPerSecond32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::RadianPerSecond32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::RadianPerSecond32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::RadianPerSecond32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("RadianPerSecond32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("RadianPerSecond32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("RadianPerSecond32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member (MT2);
    DotsTest::MemberTypesProperty::SetRadianPerSecond32Member(MT2, MT1->RadianPerSecond32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetRadianPerSecond32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member(MI);
    DotsTest::MemberTypesProperty::SetRadianPerSecond32Member(MI,MT2->RadianPerSecond32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetRadianPerSecond32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetRadianPerSecond32Member(MIA,MT2->RadianPerSecond32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->RadianPerSecond32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadianPerSecond32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadianPerSecond32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::RadianPerSecond32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->RadianPerSecond32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->RadianPerSecond32Member()[ix].IsChanged();
        MA1->RadianPerSecond32Member()[ix].SetVal(DotsTest::ParameterArrays::RadianPerSecond32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->RadianPerSecond32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->RadianPerSecond32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (MA2, ix);
        MA2->RadianPerSecond32Member()[ix].SetVal(MA1->RadianPerSecond32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecond32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.RadianPerSecond32Member()[ix].SetVal(MA1->RadianPerSecond32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecond32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecond32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadianPerSecond32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadianPerSecond32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond32Member (AEO,ix);
    }

    // SetNull test
    MT1->RadianPerSecond32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullRadianPerSecond32Member(MT2);
    MA1->RadianPerSecond32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullRadianPerSecond32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->RadianPerSecond32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullRadianPerSecond32Member (MT2) &&
        MA1->RadianPerSecond32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullRadianPerSecond32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_RadianPerSecondSquared32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"RadianPerSecondSquared32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::RadianPerSecondSquared32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::RadianPerSecondSquared32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::RadianPerSecondSquared32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::RadianPerSecondSquared32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::RadianPerSecondSquared32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->RadianPerSecondSquared32Member().IsNull();
    In_Req_Ok  = !MT1->RadianPerSecondSquared32Member().IsChanged();
    MT1->RadianPerSecondSquared32Member().SetVal(DotsTest::ParameterTypes::RadianPerSecondSquared32Parameter());
    Null_Ok  = Null_Ok && !MT1->RadianPerSecondSquared32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->RadianPerSecondSquared32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::RadianPerSecondSquared32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::RadianPerSecondSquared32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::RadianPerSecondSquared32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("RadianPerSecondSquared32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("RadianPerSecondSquared32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("RadianPerSecondSquared32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member (MT2);
    DotsTest::MemberTypesProperty::SetRadianPerSecondSquared32Member(MT2, MT1->RadianPerSecondSquared32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetRadianPerSecondSquared32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member(MI);
    DotsTest::MemberTypesProperty::SetRadianPerSecondSquared32Member(MI,MT2->RadianPerSecondSquared32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetRadianPerSecondSquared32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetRadianPerSecondSquared32Member(MIA,MT2->RadianPerSecondSquared32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->RadianPerSecondSquared32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadianPerSecondSquared32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadianPerSecondSquared32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::RadianPerSecondSquared32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->RadianPerSecondSquared32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->RadianPerSecondSquared32Member()[ix].IsChanged();
        MA1->RadianPerSecondSquared32Member()[ix].SetVal(DotsTest::ParameterArrays::RadianPerSecondSquared32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->RadianPerSecondSquared32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->RadianPerSecondSquared32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (MA2, ix);
        MA2->RadianPerSecondSquared32Member()[ix].SetVal(MA1->RadianPerSecondSquared32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecondSquared32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.RadianPerSecondSquared32Member()[ix].SetVal(MA1->RadianPerSecondSquared32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecondSquared32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecondSquared32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadianPerSecondSquared32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadianPerSecondSquared32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared32Member (AEO,ix);
    }

    // SetNull test
    MT1->RadianPerSecondSquared32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullRadianPerSecondSquared32Member(MT2);
    MA1->RadianPerSecondSquared32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullRadianPerSecondSquared32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->RadianPerSecondSquared32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared32Member (MT2) &&
        MA1->RadianPerSecondSquared32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Second32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Second32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Second32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Second32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Second32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Second32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Second32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Second32Member().IsNull();
    In_Req_Ok  = !MT1->Second32Member().IsChanged();
    MT1->Second32Member().SetVal(DotsTest::ParameterTypes::Second32Parameter());
    Null_Ok  = Null_Ok && !MT1->Second32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Second32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Second32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Second32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Second32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Second32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Second32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Second32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSecond32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond32Member (MT2);
    DotsTest::MemberTypesProperty::SetSecond32Member(MT2, MT1->Second32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetSecond32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSecond32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond32Member(MI);
    DotsTest::MemberTypesProperty::SetSecond32Member(MI,MT2->Second32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetSecond32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSecond32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetSecond32Member(MIA,MT2->Second32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Second32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSecond32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSecond32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Second32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Second32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Second32Member()[ix].IsChanged();
        MA1->Second32Member()[ix].SetVal(DotsTest::ParameterArrays::Second32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Second32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Second32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSecond32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond32Member (MA2, ix);
        MA2->Second32Member()[ix].SetVal(MA1->Second32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetSecond32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSecond32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Second32Member()[ix].SetVal(MA1->Second32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSecond32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSecond32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSecond32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSecond32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSecond32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond32Member (AEO,ix);
    }

    // SetNull test
    MT1->Second32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullSecond32Member(MT2);
    MA1->Second32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullSecond32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Second32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullSecond32Member (MT2) &&
        MA1->Second32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullSecond32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_SquareMeter32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"SquareMeter32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::SquareMeter32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::SquareMeter32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::SquareMeter32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::SquareMeter32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::SquareMeter32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->SquareMeter32Member().IsNull();
    In_Req_Ok  = !MT1->SquareMeter32Member().IsChanged();
    MT1->SquareMeter32Member().SetVal(DotsTest::ParameterTypes::SquareMeter32Parameter());
    Null_Ok  = Null_Ok && !MT1->SquareMeter32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->SquareMeter32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::SquareMeter32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::SquareMeter32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::SquareMeter32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("SquareMeter32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("SquareMeter32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("SquareMeter32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSquareMeter32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member (MT2);
    DotsTest::MemberTypesProperty::SetSquareMeter32Member(MT2, MT1->SquareMeter32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetSquareMeter32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSquareMeter32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member(MI);
    DotsTest::MemberTypesProperty::SetSquareMeter32Member(MI,MT2->SquareMeter32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetSquareMeter32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSquareMeter32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetSquareMeter32Member(MIA,MT2->SquareMeter32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->SquareMeter32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSquareMeter32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSquareMeter32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::SquareMeter32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->SquareMeter32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->SquareMeter32Member()[ix].IsChanged();
        MA1->SquareMeter32Member()[ix].SetVal(DotsTest::ParameterArrays::SquareMeter32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->SquareMeter32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->SquareMeter32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (MA2, ix);
        MA2->SquareMeter32Member()[ix].SetVal(MA1->SquareMeter32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetSquareMeter32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.SquareMeter32Member()[ix].SetVal(MA1->SquareMeter32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSquareMeter32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSquareMeter32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSquareMeter32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSquareMeter32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter32Member (AEO,ix);
    }

    // SetNull test
    MT1->SquareMeter32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullSquareMeter32Member(MT2);
    MA1->SquareMeter32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullSquareMeter32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->SquareMeter32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullSquareMeter32Member (MT2) &&
        MA1->SquareMeter32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullSquareMeter32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Steradian32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Steradian32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Steradian32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Steradian32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Steradian32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Steradian32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Steradian32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Steradian32Member().IsNull();
    In_Req_Ok  = !MT1->Steradian32Member().IsChanged();
    MT1->Steradian32Member().SetVal(DotsTest::ParameterTypes::Steradian32Parameter());
    Null_Ok  = Null_Ok && !MT1->Steradian32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Steradian32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Steradian32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Steradian32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Steradian32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Steradian32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Steradian32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Steradian32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSteradian32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian32Member (MT2);
    DotsTest::MemberTypesProperty::SetSteradian32Member(MT2, MT1->Steradian32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetSteradian32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSteradian32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian32Member(MI);
    DotsTest::MemberTypesProperty::SetSteradian32Member(MI,MT2->Steradian32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetSteradian32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSteradian32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetSteradian32Member(MIA,MT2->Steradian32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Steradian32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSteradian32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSteradian32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Steradian32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Steradian32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Steradian32Member()[ix].IsChanged();
        MA1->Steradian32Member()[ix].SetVal(DotsTest::ParameterArrays::Steradian32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Steradian32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Steradian32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSteradian32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian32Member (MA2, ix);
        MA2->Steradian32Member()[ix].SetVal(MA1->Steradian32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetSteradian32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSteradian32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Steradian32Member()[ix].SetVal(MA1->Steradian32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSteradian32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSteradian32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSteradian32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSteradian32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSteradian32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian32Member (AEO,ix);
    }

    // SetNull test
    MT1->Steradian32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullSteradian32Member(MT2);
    MA1->Steradian32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullSteradian32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Steradian32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullSteradian32Member (MT2) &&
        MA1->Steradian32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullSteradian32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Volt32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Volt32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Volt32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Volt32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Volt32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Volt32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Volt32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Volt32Member().IsNull();
    In_Req_Ok  = !MT1->Volt32Member().IsChanged();
    MT1->Volt32Member().SetVal(DotsTest::ParameterTypes::Volt32Parameter());
    Null_Ok  = Null_Ok && !MT1->Volt32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Volt32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Volt32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Volt32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Volt32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Volt32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Volt32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Volt32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;

    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullVolt32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt32Member (MT2);
    DotsTest::MemberTypesProperty::SetVolt32Member(MT2, MT1->Volt32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetVolt32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullVolt32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt32Member(MI);
    DotsTest::MemberTypesProperty::SetVolt32Member(MI,MT2->Volt32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetVolt32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullVolt32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetVolt32Member(MIA,MT2->Volt32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Volt32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetVolt32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetVolt32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Volt32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Volt32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Volt32Member()[ix].IsChanged();
        MA1->Volt32Member()[ix].SetVal(DotsTest::ParameterArrays::Volt32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Volt32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Volt32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullVolt32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt32Member (MA2, ix);
        MA2->Volt32Member()[ix].SetVal(MA1->Volt32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetVolt32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullVolt32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Volt32Member()[ix].SetVal(MA1->Volt32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetVolt32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullVolt32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetVolt32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetVolt32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetVolt32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt32Member (AEO,ix);
    }

    // SetNull test
    MT1->Volt32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullVolt32Member(MT2);
    MA1->Volt32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullVolt32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Volt32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullVolt32Member (MT2) &&
        MA1->Volt32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullVolt32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Watt32()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Watt32");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Watt32MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Watt32MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Watt32ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Watt32MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Watt32MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Watt32Member().IsNull();
    In_Req_Ok  = !MT1->Watt32Member().IsChanged();
    MT1->Watt32Member().SetVal(DotsTest::ParameterTypes::Watt32Parameter());
    Null_Ok  = Null_Ok && !MT1->Watt32Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Watt32Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Watt32MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Watt32MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Watt32MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Watt32Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Watt32Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Watt32Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullWatt32Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt32Member (MT2);
    DotsTest::MemberTypesProperty::SetWatt32Member(MT2, MT1->Watt32Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetWatt32Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt32Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt32Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullWatt32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt32Member(MI);
    DotsTest::MemberTypesProperty::SetWatt32Member(MI,MT2->Watt32Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetWatt32Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt32Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt32Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullWatt32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt32Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetWatt32Member(MIA,MT2->Watt32Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Watt32Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt32Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt32Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt32Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetWatt32Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt32Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt32Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt32Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetWatt32Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt32Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt32Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Watt32ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Watt32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Watt32Member()[ix].IsChanged();
        MA1->Watt32Member()[ix].SetVal(DotsTest::ParameterArrays::Watt32Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Watt32Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Watt32Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullWatt32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt32Member (MA2, ix);
        MA2->Watt32Member()[ix].SetVal(MA1->Watt32Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt32Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt32Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetWatt32Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullWatt32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt32Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Watt32Member()[ix].SetVal(MA1->Watt32Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetWatt32Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt32Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt32Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullWatt32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt32Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetWatt32Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt32Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt32Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt32Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetWatt32Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt32Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt32Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt32Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetWatt32Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt32Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt32Member (AEO,ix);
    }

    // SetNull test
    MT1->Watt32Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullWatt32Member(MT2);
    MA1->Watt32Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullWatt32Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Watt32Member().IsNull() && DotsTest::MemberTypesProperty::IsNullWatt32Member (MT2) &&
        MA1->Watt32Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullWatt32Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Ampere64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Ampere64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Ampere64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Ampere64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Ampere64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Ampere64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Ampere64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Ampere64Member().IsNull();
    In_Req_Ok  = !MT1->Ampere64Member().IsChanged();
    MT1->Ampere64Member().SetVal(DotsTest::ParameterTypes::Ampere64Parameter());
    Null_Ok  = Null_Ok && !MT1->Ampere64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Ampere64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Ampere64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Ampere64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Ampere64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Ampere64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Ampere64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Ampere64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullAmpere64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere64Member (MT2);
    DotsTest::MemberTypesProperty::SetAmpere64Member(MT2, MT1->Ampere64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetAmpere64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullAmpere64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere64Member(MI);
    DotsTest::MemberTypesProperty::SetAmpere64Member(MI,MT2->Ampere64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetAmpere64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullAmpere64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetAmpere64Member(MIA,MT2->Ampere64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Ampere64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedAmpere64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetAmpere64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetAmpere64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullAmpere64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedAmpere64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Ampere64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Ampere64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Ampere64Member()[ix].IsChanged();
        MA1->Ampere64Member()[ix].SetVal(DotsTest::ParameterArrays::Ampere64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Ampere64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Ampere64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullAmpere64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere64Member (MA2, ix);
        MA2->Ampere64Member()[ix].SetVal(MA1->Ampere64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetAmpere64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullAmpere64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Ampere64Member()[ix].SetVal(MA1->Ampere64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetAmpere64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullAmpere64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetAmpere64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedAmpere64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetAmpere64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetAmpere64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullAmpere64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedAmpere64Member (AEO,ix);
    }

    // SetNull test
    MT1->Ampere64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullAmpere64Member(MT2);
    MA1->Ampere64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullAmpere64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Ampere64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullAmpere64Member (MT2) &&
        MA1->Ampere64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullAmpere64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_CubicMeter64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"CubicMeter64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::CubicMeter64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::CubicMeter64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::CubicMeter64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::CubicMeter64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::CubicMeter64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->CubicMeter64Member().IsNull();
    In_Req_Ok  = !MT1->CubicMeter64Member().IsChanged();
    MT1->CubicMeter64Member().SetVal(DotsTest::ParameterTypes::CubicMeter64Parameter());
    Null_Ok  = Null_Ok && !MT1->CubicMeter64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->CubicMeter64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::CubicMeter64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::CubicMeter64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::CubicMeter64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("CubicMeter64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("CubicMeter64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("CubicMeter64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullCubicMeter64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member (MT2);
    DotsTest::MemberTypesProperty::SetCubicMeter64Member(MT2, MT1->CubicMeter64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetCubicMeter64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullCubicMeter64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member(MI);
    DotsTest::MemberTypesProperty::SetCubicMeter64Member(MI,MT2->CubicMeter64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetCubicMeter64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullCubicMeter64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetCubicMeter64Member(MIA,MT2->CubicMeter64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->CubicMeter64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetCubicMeter64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetCubicMeter64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullCubicMeter64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedCubicMeter64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::CubicMeter64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->CubicMeter64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->CubicMeter64Member()[ix].IsChanged();
        MA1->CubicMeter64Member()[ix].SetVal(DotsTest::ParameterArrays::CubicMeter64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->CubicMeter64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->CubicMeter64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (MA2, ix);
        MA2->CubicMeter64Member()[ix].SetVal(MA1->CubicMeter64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetCubicMeter64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.CubicMeter64Member()[ix].SetVal(MA1->CubicMeter64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetCubicMeter64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetCubicMeter64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetCubicMeter64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetCubicMeter64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedCubicMeter64Member (AEO,ix);
    }

    // SetNull test
    MT1->CubicMeter64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullCubicMeter64Member(MT2);
    MA1->CubicMeter64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullCubicMeter64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->CubicMeter64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullCubicMeter64Member (MT2) &&
        MA1->CubicMeter64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullCubicMeter64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Hertz64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Hertz64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Hertz64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Hertz64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Hertz64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Hertz64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Hertz64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Hertz64Member().IsNull();
    In_Req_Ok  = !MT1->Hertz64Member().IsChanged();
    MT1->Hertz64Member().SetVal(DotsTest::ParameterTypes::Hertz64Parameter());
    Null_Ok  = Null_Ok && !MT1->Hertz64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Hertz64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Hertz64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Hertz64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Hertz64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Hertz64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Hertz64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Hertz64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullHertz64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz64Member (MT2);
    DotsTest::MemberTypesProperty::SetHertz64Member(MT2, MT1->Hertz64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetHertz64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullHertz64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz64Member(MI);
    DotsTest::MemberTypesProperty::SetHertz64Member(MI,MT2->Hertz64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetHertz64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullHertz64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetHertz64Member(MIA,MT2->Hertz64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Hertz64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedHertz64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetHertz64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetHertz64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullHertz64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedHertz64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Hertz64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Hertz64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Hertz64Member()[ix].IsChanged();
        MA1->Hertz64Member()[ix].SetVal(DotsTest::ParameterArrays::Hertz64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Hertz64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Hertz64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullHertz64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz64Member (MA2, ix);
        MA2->Hertz64Member()[ix].SetVal(MA1->Hertz64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetHertz64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullHertz64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Hertz64Member()[ix].SetVal(MA1->Hertz64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetHertz64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullHertz64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetHertz64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedHertz64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetHertz64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetHertz64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullHertz64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedHertz64Member (AEO,ix);
    }

    // SetNull test
    MT1->Hertz64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullHertz64Member(MT2);
    MA1->Hertz64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullHertz64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Hertz64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullHertz64Member (MT2) &&
        MA1->Hertz64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullHertz64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Joule64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Joule64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Joule64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Joule64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Joule64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Joule64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Joule64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Joule64Member().IsNull();
    In_Req_Ok  = !MT1->Joule64Member().IsChanged();
    MT1->Joule64Member().SetVal(DotsTest::ParameterTypes::Joule64Parameter());
    Null_Ok  = Null_Ok && !MT1->Joule64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Joule64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Joule64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Joule64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Joule64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Joule64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Joule64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Joule64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullJoule64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule64Member (MT2);
    DotsTest::MemberTypesProperty::SetJoule64Member(MT2, MT1->Joule64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetJoule64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullJoule64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule64Member(MI);
    DotsTest::MemberTypesProperty::SetJoule64Member(MI,MT2->Joule64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetJoule64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullJoule64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetJoule64Member(MIA,MT2->Joule64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Joule64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedJoule64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetJoule64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetJoule64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullJoule64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedJoule64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Joule64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Joule64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Joule64Member()[ix].IsChanged();
        MA1->Joule64Member()[ix].SetVal(DotsTest::ParameterArrays::Joule64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Joule64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Joule64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullJoule64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule64Member (MA2, ix);
        MA2->Joule64Member()[ix].SetVal(MA1->Joule64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetJoule64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullJoule64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Joule64Member()[ix].SetVal(MA1->Joule64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetJoule64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullJoule64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetJoule64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedJoule64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetJoule64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetJoule64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullJoule64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedJoule64Member (AEO,ix);
    }

    // SetNull test
    MT1->Joule64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullJoule64Member(MT2);
    MA1->Joule64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullJoule64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Joule64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullJoule64Member (MT2) &&
        MA1->Joule64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullJoule64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Kelvin64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Kelvin64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Kelvin64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Kelvin64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Kelvin64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Kelvin64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Kelvin64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Kelvin64Member().IsNull();
    In_Req_Ok  = !MT1->Kelvin64Member().IsChanged();
    MT1->Kelvin64Member().SetVal(DotsTest::ParameterTypes::Kelvin64Parameter());
    Null_Ok  = Null_Ok && !MT1->Kelvin64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Kelvin64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Kelvin64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Kelvin64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Kelvin64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Kelvin64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Kelvin64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Kelvin64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKelvin64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin64Member (MT2);
    DotsTest::MemberTypesProperty::SetKelvin64Member(MT2, MT1->Kelvin64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetKelvin64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKelvin64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin64Member(MI);
    DotsTest::MemberTypesProperty::SetKelvin64Member(MI,MT2->Kelvin64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetKelvin64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKelvin64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetKelvin64Member(MIA,MT2->Kelvin64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Kelvin64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKelvin64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetKelvin64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetKelvin64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKelvin64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKelvin64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Kelvin64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Kelvin64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Kelvin64Member()[ix].IsChanged();
        MA1->Kelvin64Member()[ix].SetVal(DotsTest::ParameterArrays::Kelvin64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Kelvin64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Kelvin64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKelvin64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin64Member (MA2, ix);
        MA2->Kelvin64Member()[ix].SetVal(MA1->Kelvin64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetKelvin64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKelvin64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Kelvin64Member()[ix].SetVal(MA1->Kelvin64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetKelvin64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKelvin64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetKelvin64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKelvin64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetKelvin64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetKelvin64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKelvin64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKelvin64Member (AEO,ix);
    }

    // SetNull test
    MT1->Kelvin64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullKelvin64Member(MT2);
    MA1->Kelvin64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullKelvin64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Kelvin64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullKelvin64Member (MT2) &&
        MA1->Kelvin64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullKelvin64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Kilogram64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Kilogram64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Kilogram64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Kilogram64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Kilogram64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Kilogram64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Kilogram64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Kilogram64Member().IsNull();
    In_Req_Ok  = !MT1->Kilogram64Member().IsChanged();
    MT1->Kilogram64Member().SetVal(DotsTest::ParameterTypes::Kilogram64Parameter());
    Null_Ok  = Null_Ok && !MT1->Kilogram64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Kilogram64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Kilogram64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Kilogram64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Kilogram64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Kilogram64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Kilogram64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Kilogram64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKilogram64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram64Member (MT2);
    DotsTest::MemberTypesProperty::SetKilogram64Member(MT2, MT1->Kilogram64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetKilogram64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKilogram64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram64Member(MI);
    DotsTest::MemberTypesProperty::SetKilogram64Member(MI,MT2->Kilogram64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetKilogram64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullKilogram64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetKilogram64Member(MIA,MT2->Kilogram64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Kilogram64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedKilogram64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetKilogram64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetKilogram64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullKilogram64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedKilogram64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Kilogram64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Kilogram64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Kilogram64Member()[ix].IsChanged();
        MA1->Kilogram64Member()[ix].SetVal(DotsTest::ParameterArrays::Kilogram64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Kilogram64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Kilogram64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKilogram64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram64Member (MA2, ix);
        MA2->Kilogram64Member()[ix].SetVal(MA1->Kilogram64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetKilogram64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKilogram64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Kilogram64Member()[ix].SetVal(MA1->Kilogram64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetKilogram64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullKilogram64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetKilogram64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedKilogram64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetKilogram64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetKilogram64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullKilogram64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedKilogram64Member (AEO,ix);
    }

    // SetNull test
    MT1->Kilogram64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullKilogram64Member(MT2);
    MA1->Kilogram64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullKilogram64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Kilogram64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullKilogram64Member (MT2) &&
        MA1->Kilogram64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullKilogram64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Meter64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Meter64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Meter64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Meter64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Meter64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Meter64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Meter64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Meter64Member().IsNull();
    In_Req_Ok  = !MT1->Meter64Member().IsChanged();
    MT1->Meter64Member().SetVal(DotsTest::ParameterTypes::Meter64Parameter());
    Null_Ok  = Null_Ok && !MT1->Meter64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Meter64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Meter64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Meter64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Meter64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Meter64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Meter64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Meter64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeter64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter64Member (MT2);
    DotsTest::MemberTypesProperty::SetMeter64Member(MT2, MT1->Meter64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetMeter64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeter64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter64Member(MI);
    DotsTest::MemberTypesProperty::SetMeter64Member(MI,MT2->Meter64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetMeter64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeter64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetMeter64Member(MIA,MT2->Meter64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Meter64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeter64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeter64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeter64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeter64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeter64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Meter64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Meter64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Meter64Member()[ix].IsChanged();
        MA1->Meter64Member()[ix].SetVal(DotsTest::ParameterArrays::Meter64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Meter64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Meter64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeter64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter64Member (MA2, ix);
        MA2->Meter64Member()[ix].SetVal(MA1->Meter64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetMeter64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeter64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Meter64Member()[ix].SetVal(MA1->Meter64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeter64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeter64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeter64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeter64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeter64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeter64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeter64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeter64Member (AEO,ix);
    }

    // SetNull test
    MT1->Meter64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullMeter64Member(MT2);
    MA1->Meter64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullMeter64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Meter64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullMeter64Member (MT2) &&
        MA1->Meter64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullMeter64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_MeterPerSecond64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"MeterPerSecond64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::MeterPerSecond64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::MeterPerSecond64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::MeterPerSecond64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::MeterPerSecond64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::MeterPerSecond64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->MeterPerSecond64Member().IsNull();
    In_Req_Ok  = !MT1->MeterPerSecond64Member().IsChanged();
    MT1->MeterPerSecond64Member().SetVal(DotsTest::ParameterTypes::MeterPerSecond64Parameter());
    Null_Ok  = Null_Ok && !MT1->MeterPerSecond64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->MeterPerSecond64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::MeterPerSecond64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::MeterPerSecond64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::MeterPerSecond64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("MeterPerSecond64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("MeterPerSecond64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("MeterPerSecond64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member (MT2);
    DotsTest::MemberTypesProperty::SetMeterPerSecond64Member(MT2, MT1->MeterPerSecond64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetMeterPerSecond64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member(MI);
    DotsTest::MemberTypesProperty::SetMeterPerSecond64Member(MI,MT2->MeterPerSecond64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetMeterPerSecond64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetMeterPerSecond64Member(MIA,MT2->MeterPerSecond64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->MeterPerSecond64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeterPerSecond64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeterPerSecond64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecond64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::MeterPerSecond64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->MeterPerSecond64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->MeterPerSecond64Member()[ix].IsChanged();
        MA1->MeterPerSecond64Member()[ix].SetVal(DotsTest::ParameterArrays::MeterPerSecond64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->MeterPerSecond64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->MeterPerSecond64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (MA2, ix);
        MA2->MeterPerSecond64Member()[ix].SetVal(MA1->MeterPerSecond64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecond64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.MeterPerSecond64Member()[ix].SetVal(MA1->MeterPerSecond64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecond64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecond64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeterPerSecond64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeterPerSecond64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecond64Member (AEO,ix);
    }

    // SetNull test
    MT1->MeterPerSecond64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullMeterPerSecond64Member(MT2);
    MA1->MeterPerSecond64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullMeterPerSecond64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->MeterPerSecond64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullMeterPerSecond64Member (MT2) &&
        MA1->MeterPerSecond64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullMeterPerSecond64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_MeterPerSecondSquared64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"MeterPerSecondSquared64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::MeterPerSecondSquared64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::MeterPerSecondSquared64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::MeterPerSecondSquared64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::MeterPerSecondSquared64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::MeterPerSecondSquared64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->MeterPerSecondSquared64Member().IsNull();
    In_Req_Ok  = !MT1->MeterPerSecondSquared64Member().IsChanged();
    MT1->MeterPerSecondSquared64Member().SetVal(DotsTest::ParameterTypes::MeterPerSecondSquared64Parameter());
    Null_Ok  = Null_Ok && !MT1->MeterPerSecondSquared64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->MeterPerSecondSquared64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::MeterPerSecondSquared64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::MeterPerSecondSquared64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::MeterPerSecondSquared64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("MeterPerSecondSquared64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("MeterPerSecondSquared64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("MeterPerSecondSquared64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member (MT2);
    DotsTest::MemberTypesProperty::SetMeterPerSecondSquared64Member(MT2, MT1->MeterPerSecondSquared64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetMeterPerSecondSquared64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member(MI);
    DotsTest::MemberTypesProperty::SetMeterPerSecondSquared64Member(MI,MT2->MeterPerSecondSquared64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetMeterPerSecondSquared64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetMeterPerSecondSquared64Member(MIA,MT2->MeterPerSecondSquared64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->MeterPerSecondSquared64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeterPerSecondSquared64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetMeterPerSecondSquared64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedMeterPerSecondSquared64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::MeterPerSecondSquared64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->MeterPerSecondSquared64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->MeterPerSecondSquared64Member()[ix].IsChanged();
        MA1->MeterPerSecondSquared64Member()[ix].SetVal(DotsTest::ParameterArrays::MeterPerSecondSquared64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->MeterPerSecondSquared64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->MeterPerSecondSquared64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (MA2, ix);
        MA2->MeterPerSecondSquared64Member()[ix].SetVal(MA1->MeterPerSecondSquared64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecondSquared64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.MeterPerSecondSquared64Member()[ix].SetVal(MA1->MeterPerSecondSquared64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecondSquared64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetMeterPerSecondSquared64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeterPerSecondSquared64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetMeterPerSecondSquared64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedMeterPerSecondSquared64Member (AEO,ix);
    }

    // SetNull test
    MT1->MeterPerSecondSquared64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullMeterPerSecondSquared64Member(MT2);
    MA1->MeterPerSecondSquared64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullMeterPerSecondSquared64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->MeterPerSecondSquared64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullMeterPerSecondSquared64Member (MT2) &&
        MA1->MeterPerSecondSquared64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullMeterPerSecondSquared64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Newton64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Newton64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Newton64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Newton64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Newton64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Newton64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Newton64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Newton64Member().IsNull();
    In_Req_Ok  = !MT1->Newton64Member().IsChanged();
    MT1->Newton64Member().SetVal(DotsTest::ParameterTypes::Newton64Parameter());
    Null_Ok  = Null_Ok && !MT1->Newton64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Newton64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Newton64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Newton64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Newton64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Newton64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Newton64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Newton64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullNewton64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton64Member (MT2);
    DotsTest::MemberTypesProperty::SetNewton64Member(MT2, MT1->Newton64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetNewton64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullNewton64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton64Member(MI);
    DotsTest::MemberTypesProperty::SetNewton64Member(MI,MT2->Newton64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetNewton64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullNewton64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetNewton64Member(MIA,MT2->Newton64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Newton64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedNewton64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetNewton64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetNewton64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullNewton64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedNewton64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Newton64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Newton64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Newton64Member()[ix].IsChanged();
        MA1->Newton64Member()[ix].SetVal(DotsTest::ParameterArrays::Newton64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Newton64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Newton64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullNewton64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton64Member (MA2, ix);
        MA2->Newton64Member()[ix].SetVal(MA1->Newton64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetNewton64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullNewton64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Newton64Member()[ix].SetVal(MA1->Newton64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetNewton64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullNewton64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetNewton64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedNewton64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetNewton64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetNewton64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullNewton64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedNewton64Member (AEO,ix);
    }

    // SetNull test
    MT1->Newton64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullNewton64Member(MT2);
    MA1->Newton64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullNewton64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Newton64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullNewton64Member (MT2) &&
        MA1->Newton64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullNewton64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Pascal64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Pascal64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Pascal64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Pascal64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Pascal64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Pascal64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Pascal64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Pascal64Member().IsNull();
    In_Req_Ok  = !MT1->Pascal64Member().IsChanged();
    MT1->Pascal64Member().SetVal(DotsTest::ParameterTypes::Pascal64Parameter());
    Null_Ok  = Null_Ok && !MT1->Pascal64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Pascal64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Pascal64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Pascal64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Pascal64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Pascal64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Pascal64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Pascal64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullPascal64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal64Member (MT2);
    DotsTest::MemberTypesProperty::SetPascal64Member(MT2, MT1->Pascal64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetPascal64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullPascal64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal64Member(MI);
    DotsTest::MemberTypesProperty::SetPascal64Member(MI,MT2->Pascal64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetPascal64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullPascal64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetPascal64Member(MIA,MT2->Pascal64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Pascal64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedPascal64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetPascal64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetPascal64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullPascal64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedPascal64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Pascal64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Pascal64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Pascal64Member()[ix].IsChanged();
        MA1->Pascal64Member()[ix].SetVal(DotsTest::ParameterArrays::Pascal64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Pascal64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Pascal64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullPascal64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal64Member (MA2, ix);
        MA2->Pascal64Member()[ix].SetVal(MA1->Pascal64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetPascal64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullPascal64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Pascal64Member()[ix].SetVal(MA1->Pascal64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetPascal64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullPascal64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetPascal64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedPascal64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetPascal64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetPascal64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullPascal64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedPascal64Member (AEO,ix);
    }

    // SetNull test
    MT1->Pascal64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullPascal64Member(MT2);
    MA1->Pascal64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullPascal64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Pascal64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullPascal64Member (MT2) &&
        MA1->Pascal64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullPascal64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Radian64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Radian64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Radian64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Radian64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Radian64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Radian64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Radian64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Radian64Member().IsNull();
    In_Req_Ok  = !MT1->Radian64Member().IsChanged();
    MT1->Radian64Member().SetVal(DotsTest::ParameterTypes::Radian64Parameter());
    Null_Ok  = Null_Ok && !MT1->Radian64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Radian64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Radian64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Radian64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Radian64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Radian64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Radian64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Radian64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadian64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian64Member (MT2);
    DotsTest::MemberTypesProperty::SetRadian64Member(MT2, MT1->Radian64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetRadian64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadian64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian64Member(MI);
    DotsTest::MemberTypesProperty::SetRadian64Member(MI,MT2->Radian64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetRadian64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadian64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetRadian64Member(MIA,MT2->Radian64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Radian64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadian64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadian64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadian64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadian64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadian64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Radian64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Radian64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Radian64Member()[ix].IsChanged();
        MA1->Radian64Member()[ix].SetVal(DotsTest::ParameterArrays::Radian64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Radian64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Radian64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadian64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian64Member (MA2, ix);
        MA2->Radian64Member()[ix].SetVal(MA1->Radian64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetRadian64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadian64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Radian64Member()[ix].SetVal(MA1->Radian64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadian64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadian64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadian64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadian64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadian64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadian64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadian64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadian64Member (AEO,ix);
    }

    // SetNull test
    MT1->Radian64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullRadian64Member(MT2);
    MA1->Radian64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullRadian64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Radian64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullRadian64Member (MT2) &&
        MA1->Radian64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullRadian64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_RadianPerSecond64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"RadianPerSecond64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::RadianPerSecond64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::RadianPerSecond64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::RadianPerSecond64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::RadianPerSecond64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::RadianPerSecond64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->RadianPerSecond64Member().IsNull();
    In_Req_Ok  = !MT1->RadianPerSecond64Member().IsChanged();
    MT1->RadianPerSecond64Member().SetVal(DotsTest::ParameterTypes::RadianPerSecond64Parameter());
    Null_Ok  = Null_Ok && !MT1->RadianPerSecond64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->RadianPerSecond64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::RadianPerSecond64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::RadianPerSecond64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::RadianPerSecond64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("RadianPerSecond64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("RadianPerSecond64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("RadianPerSecond64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member (MT2);
    DotsTest::MemberTypesProperty::SetRadianPerSecond64Member(MT2, MT1->RadianPerSecond64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetRadianPerSecond64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member(MI);
    DotsTest::MemberTypesProperty::SetRadianPerSecond64Member(MI,MT2->RadianPerSecond64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetRadianPerSecond64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetRadianPerSecond64Member(MIA,MT2->RadianPerSecond64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->RadianPerSecond64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadianPerSecond64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadianPerSecond64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecond64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::RadianPerSecond64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->RadianPerSecond64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->RadianPerSecond64Member()[ix].IsChanged();
        MA1->RadianPerSecond64Member()[ix].SetVal(DotsTest::ParameterArrays::RadianPerSecond64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->RadianPerSecond64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->RadianPerSecond64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (MA2, ix);
        MA2->RadianPerSecond64Member()[ix].SetVal(MA1->RadianPerSecond64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecond64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.RadianPerSecond64Member()[ix].SetVal(MA1->RadianPerSecond64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecond64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecond64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadianPerSecond64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadianPerSecond64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecond64Member (AEO,ix);
    }

    // SetNull test
    MT1->RadianPerSecond64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullRadianPerSecond64Member(MT2);
    MA1->RadianPerSecond64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullRadianPerSecond64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->RadianPerSecond64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullRadianPerSecond64Member (MT2) &&
        MA1->RadianPerSecond64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullRadianPerSecond64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_RadianPerSecondSquared64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"RadianPerSecondSquared64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::RadianPerSecondSquared64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::RadianPerSecondSquared64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::RadianPerSecondSquared64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::RadianPerSecondSquared64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::RadianPerSecondSquared64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->RadianPerSecondSquared64Member().IsNull();
    In_Req_Ok  = !MT1->RadianPerSecondSquared64Member().IsChanged();
    MT1->RadianPerSecondSquared64Member().SetVal(DotsTest::ParameterTypes::RadianPerSecondSquared64Parameter());
    Null_Ok  = Null_Ok && !MT1->RadianPerSecondSquared64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->RadianPerSecondSquared64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::RadianPerSecondSquared64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::RadianPerSecondSquared64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::RadianPerSecondSquared64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("RadianPerSecondSquared64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("RadianPerSecondSquared64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("RadianPerSecondSquared64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member (MT2);
    DotsTest::MemberTypesProperty::SetRadianPerSecondSquared64Member(MT2, MT1->RadianPerSecondSquared64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetRadianPerSecondSquared64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member(MI);
    DotsTest::MemberTypesProperty::SetRadianPerSecondSquared64Member(MI,MT2->RadianPerSecondSquared64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetRadianPerSecondSquared64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetRadianPerSecondSquared64Member(MIA,MT2->RadianPerSecondSquared64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->RadianPerSecondSquared64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadianPerSecondSquared64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetRadianPerSecondSquared64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedRadianPerSecondSquared64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::RadianPerSecondSquared64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->RadianPerSecondSquared64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->RadianPerSecondSquared64Member()[ix].IsChanged();
        MA1->RadianPerSecondSquared64Member()[ix].SetVal(DotsTest::ParameterArrays::RadianPerSecondSquared64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->RadianPerSecondSquared64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->RadianPerSecondSquared64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (MA2, ix);
        MA2->RadianPerSecondSquared64Member()[ix].SetVal(MA1->RadianPerSecondSquared64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecondSquared64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.RadianPerSecondSquared64Member()[ix].SetVal(MA1->RadianPerSecondSquared64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecondSquared64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetRadianPerSecondSquared64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadianPerSecondSquared64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetRadianPerSecondSquared64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedRadianPerSecondSquared64Member (AEO,ix);
    }

    // SetNull test
    MT1->RadianPerSecondSquared64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullRadianPerSecondSquared64Member(MT2);
    MA1->RadianPerSecondSquared64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullRadianPerSecondSquared64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->RadianPerSecondSquared64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullRadianPerSecondSquared64Member (MT2) &&
        MA1->RadianPerSecondSquared64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullRadianPerSecondSquared64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Second64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Second64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Second64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Second64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Second64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Second64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Second64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Second64Member().IsNull();
    In_Req_Ok  = !MT1->Second64Member().IsChanged();
    MT1->Second64Member().SetVal(DotsTest::ParameterTypes::Second64Parameter());
    Null_Ok  = Null_Ok && !MT1->Second64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Second64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Second64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Second64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Second64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Second64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Second64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Second64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSecond64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond64Member (MT2);
    DotsTest::MemberTypesProperty::SetSecond64Member(MT2, MT1->Second64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetSecond64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSecond64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond64Member(MI);
    DotsTest::MemberTypesProperty::SetSecond64Member(MI,MT2->Second64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetSecond64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSecond64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetSecond64Member(MIA,MT2->Second64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Second64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSecond64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSecond64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSecond64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSecond64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSecond64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Second64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Second64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Second64Member()[ix].IsChanged();
        MA1->Second64Member()[ix].SetVal(DotsTest::ParameterArrays::Second64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Second64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Second64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSecond64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond64Member (MA2, ix);
        MA2->Second64Member()[ix].SetVal(MA1->Second64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetSecond64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSecond64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Second64Member()[ix].SetVal(MA1->Second64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSecond64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSecond64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSecond64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSecond64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSecond64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSecond64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSecond64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSecond64Member (AEO,ix);
    }

    // SetNull test
    MT1->Second64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullSecond64Member(MT2);
    MA1->Second64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullSecond64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Second64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullSecond64Member (MT2) &&
        MA1->Second64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullSecond64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_SquareMeter64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"SquareMeter64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::SquareMeter64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::SquareMeter64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::SquareMeter64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::SquareMeter64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::SquareMeter64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->SquareMeter64Member().IsNull();
    In_Req_Ok  = !MT1->SquareMeter64Member().IsChanged();
    MT1->SquareMeter64Member().SetVal(DotsTest::ParameterTypes::SquareMeter64Parameter());
    Null_Ok  = Null_Ok && !MT1->SquareMeter64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->SquareMeter64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::SquareMeter64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::SquareMeter64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::SquareMeter64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("SquareMeter64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("SquareMeter64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("SquareMeter64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSquareMeter64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member (MT2);
    DotsTest::MemberTypesProperty::SetSquareMeter64Member(MT2, MT1->SquareMeter64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetSquareMeter64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSquareMeter64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member(MI);
    DotsTest::MemberTypesProperty::SetSquareMeter64Member(MI,MT2->SquareMeter64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetSquareMeter64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSquareMeter64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetSquareMeter64Member(MIA,MT2->SquareMeter64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->SquareMeter64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSquareMeter64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSquareMeter64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSquareMeter64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSquareMeter64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::SquareMeter64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->SquareMeter64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->SquareMeter64Member()[ix].IsChanged();
        MA1->SquareMeter64Member()[ix].SetVal(DotsTest::ParameterArrays::SquareMeter64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->SquareMeter64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->SquareMeter64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (MA2, ix);
        MA2->SquareMeter64Member()[ix].SetVal(MA1->SquareMeter64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetSquareMeter64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.SquareMeter64Member()[ix].SetVal(MA1->SquareMeter64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSquareMeter64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSquareMeter64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSquareMeter64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSquareMeter64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSquareMeter64Member (AEO,ix);
    }

    // SetNull test
    MT1->SquareMeter64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullSquareMeter64Member(MT2);
    MA1->SquareMeter64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullSquareMeter64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->SquareMeter64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullSquareMeter64Member (MT2) &&
        MA1->SquareMeter64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullSquareMeter64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Steradian64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Steradian64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Steradian64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Steradian64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Steradian64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Steradian64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Steradian64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Steradian64Member().IsNull();
    In_Req_Ok  = !MT1->Steradian64Member().IsChanged();
    MT1->Steradian64Member().SetVal(DotsTest::ParameterTypes::Steradian64Parameter());
    Null_Ok  = Null_Ok && !MT1->Steradian64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Steradian64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Steradian64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Steradian64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Steradian64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Steradian64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Steradian64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Steradian64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSteradian64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian64Member (MT2);
    DotsTest::MemberTypesProperty::SetSteradian64Member(MT2, MT1->Steradian64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetSteradian64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSteradian64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian64Member(MI);
    DotsTest::MemberTypesProperty::SetSteradian64Member(MI,MT2->Steradian64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetSteradian64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullSteradian64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetSteradian64Member(MIA,MT2->Steradian64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Steradian64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedSteradian64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSteradian64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetSteradian64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullSteradian64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedSteradian64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Steradian64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Steradian64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Steradian64Member()[ix].IsChanged();
        MA1->Steradian64Member()[ix].SetVal(DotsTest::ParameterArrays::Steradian64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Steradian64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Steradian64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSteradian64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian64Member (MA2, ix);
        MA2->Steradian64Member()[ix].SetVal(MA1->Steradian64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetSteradian64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSteradian64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Steradian64Member()[ix].SetVal(MA1->Steradian64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSteradian64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullSteradian64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetSteradian64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedSteradian64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSteradian64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetSteradian64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullSteradian64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedSteradian64Member (AEO,ix);
    }

    // SetNull test
    MT1->Steradian64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullSteradian64Member(MT2);
    MA1->Steradian64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullSteradian64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Steradian64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullSteradian64Member (MT2) &&
        MA1->Steradian64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullSteradian64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Volt64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Volt64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Volt64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Volt64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Volt64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Volt64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Volt64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Volt64Member().IsNull();
    In_Req_Ok  = !MT1->Volt64Member().IsChanged();
    MT1->Volt64Member().SetVal(DotsTest::ParameterTypes::Volt64Parameter());
    Null_Ok  = Null_Ok && !MT1->Volt64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Volt64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Volt64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Volt64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Volt64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Volt64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Volt64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Volt64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullVolt64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt64Member (MT2);
    DotsTest::MemberTypesProperty::SetVolt64Member(MT2, MT1->Volt64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetVolt64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullVolt64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt64Member(MI);
    DotsTest::MemberTypesProperty::SetVolt64Member(MI,MT2->Volt64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetVolt64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullVolt64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetVolt64Member(MIA,MT2->Volt64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Volt64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedVolt64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetVolt64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetVolt64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullVolt64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedVolt64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Volt64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Volt64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Volt64Member()[ix].IsChanged();
        MA1->Volt64Member()[ix].SetVal(DotsTest::ParameterArrays::Volt64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Volt64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Volt64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullVolt64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt64Member (MA2, ix);
        MA2->Volt64Member()[ix].SetVal(MA1->Volt64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetVolt64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullVolt64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Volt64Member()[ix].SetVal(MA1->Volt64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetVolt64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullVolt64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetVolt64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedVolt64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetVolt64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetVolt64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullVolt64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedVolt64Member (AEO,ix);
    }

    // SetNull test
    MT1->Volt64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullVolt64Member(MT2);
    MA1->Volt64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullVolt64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Volt64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullVolt64Member (MT2) &&
        MA1->Volt64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullVolt64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}


void Test_Watt64()
{
    // Locals
    bool Null_Ok, In_Req_Ok;


    //  1. Member Parameter -> Member Class MT1 -> Member Property MT2 -> Output
    //  2. Member Property MT2 -> Member Item MI -> Output
    //  3. Member Property MT2 -> Member Item Array MIA -> Member Type Property MIA -> Output
    //  4. Member Property EO & AEO -> Output
    //  5. Member Parameter -> Member Array MA1 -> Member Array MA2 -> Member Array Property MA2 -> Output
    //  6. Member Array MA1 -> Member Item MI -> Member Array Property MI -> Output
    //  7. Member Array MA1 -> Member Item Array MIA -> Member Array Property MIA -> Output
    //  8. Member Array Property EO & AEO -> Output
    Header(L"Watt64");
    std::wcout<<"MemberId: "<<DotsTest::MemberTypes::Watt64MemberMemberIndex()<<std::endl;
    std::wcout<<"MemberId (arr): "<<DotsTest::MemberArrays::Watt64MemberMemberIndex()<<std::endl;

    std::wcout<<"Array Size Ok: "<<
        (DotsTest::ParameterArrays::Watt64ParameterArraySize() == 2 &&
         DotsTest::MemberArrays::Watt64MemberArraySize() == 2 &&
         DotsTest::MemberArraysProperty::Watt64MemberArraySize(MA1) == 2)<<std::endl;
    Null_Ok  = MT1->Watt64Member().IsNull();
    In_Req_Ok  = !MT1->Watt64Member().IsChanged();
    MT1->Watt64Member().SetVal(DotsTest::ParameterTypes::Watt64Parameter());
    Null_Ok  = Null_Ok && !MT1->Watt64Member().IsNull();
    In_Req_Ok  = In_Req_Ok && MT1->Watt64Member().IsChanged();

    //locals
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;
    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberArrays::ClassTypeId, DotsTest::MemberArrays::Watt64MemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );

    std::wcout<<"----Members---- " <<std::endl;
    std::wcout<<"GetInfo: " << lMT << "," << lMN << "," << lMTId<< "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Members::GetName(DotsTest::MemberTypes::ClassTypeId,
                                                                                   DotsTest::MemberTypes::Watt64MemberMemberIndex())<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Members::GetTypeName(DotsTest::MemberTypes::ClassTypeId,
                                                                                           DotsTest::MemberTypes::Watt64MemberMemberIndex()) <<std::endl;

    std::wcout<<"----Parameters---- " <<std::endl;
    std::wcout<<"GetName: " <<Safir::Dob::Typesystem::Parameters::GetName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                      Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                   utf8_to_wstr("Watt64Parameter"))) << std::endl;
    std::wcout<<"GetType: " <<Safir::Dob::Typesystem::Parameters::GetType(DotsTest::ParameterTypes::ClassTypeId,
                                                                         Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                      utf8_to_wstr("Watt64Parameter")))<<std::endl;
    std::wcout<<"GetTypeName: " <<Safir::Dob::Typesystem::Parameters::GetTypeName(DotsTest::ParameterTypes::ClassTypeId,
                                                                                              Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                                                                           utf8_to_wstr("Watt64Parameter")))<<std::endl;
    std::wcout<<"------------------ " <<std::endl;


    // MemberTypes
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullWatt64Member (MT2);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt64Member (MT2);
    DotsTest::MemberTypesProperty::SetWatt64Member(MT2, MT1->Watt64Member().GetVal());
    std::wcout<<"Val: "<<DotsTest::MemberTypesProperty::GetWatt64Member (MT2)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt64Member (MT2);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt64Member (MT2);

    // MemberItems
    DotsTest::TypesItem item;
    MI->TypesItem().SetPtr(item.Clone());
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullWatt64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt64Member(MI);
    DotsTest::MemberTypesProperty::SetWatt64Member(MI,MT2->Watt64Member().GetVal());
    std::wcout<<"Item Val: "<<DotsTest::MemberTypesProperty::GetWatt64Member(MI)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt64Member(MI);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt64Member(MI);

    // MemberItemsArray
    Null_Ok  = Null_Ok && DotsTest::MemberTypesProperty::IsNullWatt64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt64Member(MIA);
    MIA->TypesItemArray()[1].SetPtr(item.Clone());
    DotsTest::MemberTypesProperty::SetWatt64Member(MIA,MT2->Watt64Member().GetVal());
    std::wcout<<"Item Array Val: "<<MIA->TypesItemArray()[1]->Watt64Member().GetVal()<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt64Member(MIA);
    In_Req_Ok  = In_Req_Ok && DotsTest::MemberTypesProperty::IsChangedWatt64Member(MIA);

    // EmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt64Member(EO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetWatt64Member(EO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt64Member(EO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt64Member(EO);

    // AnotherEmptyObject
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt64Member(AEO);
    std::wcout<<"Property Parameter Val: " << DotsTest::MemberTypesProperty::GetWatt64Member(AEO)<<std::endl;
    Null_Ok  = Null_Ok && !DotsTest::MemberTypesProperty::IsNullWatt64Member(AEO);
    In_Req_Ok  = In_Req_Ok && !DotsTest::MemberTypesProperty::IsChangedWatt64Member(AEO);

    //Array test
    for (int ix = 0; ix < DotsTest::ParameterArrays::Watt64ParameterArraySize(); ix++)
    {
        //MemberArray
        Null_Ok  = Null_Ok && MA1->Watt64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && !MA1->Watt64Member()[ix].IsChanged();
        MA1->Watt64Member()[ix].SetVal(DotsTest::ParameterArrays::Watt64Parameter (ix));
        Null_Ok  = Null_Ok && !MA1->Watt64Member()[ix].IsNull();
        In_Req_Ok  = In_Req_Ok && MA1->Watt64Member()[ix].IsChanged();

        // MemberArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullWatt64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt64Member (MA2, ix);
        MA2->Watt64Member()[ix].SetVal(MA1->Watt64Member()[ix].GetVal());
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt64Member (MA2, ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt64Member (MA2, ix);
        std::wcout<<"Val "<<ix<<": "<<DotsTest::MemberArraysProperty::GetWatt64Member (MA2, ix)<<std::endl;

        // MemberItems
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullWatt64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt64Member (MI,ix);

        DotsTest::ArraysItem item;
        item.Watt64Member()[ix].SetVal(MA1->Watt64Member()[ix].GetVal());
        MI->ArraysItem().SetPtr(item.Clone());
        std::wcout<<"Array Item Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetWatt64Member (MI,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt64Member (MI,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt64Member (MI,ix);

        // MemberItemsArray
        Null_Ok  = Null_Ok && DotsTest::MemberArraysProperty::IsNullWatt64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt64Member (MIA,ix);

        MIA->ArraysItemArray()[1].SetPtr(item.Clone());
        std::wcout<<"Array Item Array Val "<<ix <<": "<<DotsTest::MemberArraysProperty::GetWatt64Member (MIA,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt64Member (MIA,ix);
        In_Req_Ok  = In_Req_Ok && DotsTest::MemberArraysProperty::IsChangedWatt64Member (MIA,ix);

        // EmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt64Member (EO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetWatt64Member(EO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt64Member (EO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt64Member (EO,ix);

        // AnotherEmptyObject
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt64Member (AEO,ix);
        std::wcout<<"Parameter Array Val "<<ix <<": " << DotsTest::MemberArraysProperty::GetWatt64Member(AEO,ix)<<std::endl;
        Null_Ok  = Null_Ok && !DotsTest::MemberArraysProperty::IsNullWatt64Member (AEO,ix);
        In_Req_Ok  = In_Req_Ok && !DotsTest::MemberArraysProperty::IsChangedWatt64Member (AEO,ix);
    }

    // SetNull test
    MT1->Watt64Member().SetNull();
    DotsTest::MemberTypesProperty::SetNullWatt64Member(MT2);
    MA1->Watt64Member()[1].SetNull();
    DotsTest::MemberArraysProperty::SetNullWatt64Member(MA2, 1);
    Null_Ok  = Null_Ok &&
        MT1->Watt64Member().IsNull() && DotsTest::MemberTypesProperty::IsNullWatt64Member (MT2) &&
        MA1->Watt64Member()[1].IsNull() && DotsTest::MemberArraysProperty::IsNullWatt64Member (MA2, 1);

    std::wcout<<"Is_Null OK: "<<Null_Ok<<std::endl;
    std::wcout<<"Is_Changed OK: "<<In_Req_Ok<<std::endl;

}
void Test_TestException()
{
    Header(L"TestException");

    try
    {
        throw DotsTest::TestException(L"Testing a TestException", __WFILE__, __LINE__);
    }
    catch (const DotsTest::TestException & e)
    {
        std::wcout << "Caught exception: "<< Safir::Dob::Typesystem::Operations::GetName(e.GetTypeId()) << std::endl;
    }
}

void Test_LibraryExceptions()
{
    Header(L"LibraryExceptions");
    Safir::Dob::Typesystem::LibraryExceptions::Instance().Set(DotsTest::TestException(L"For LibraryExceptions",__WFILE__,__LINE__));
    try
    {
        Safir::Dob::Typesystem::LibraryExceptions::Instance().Throw();
    }
    catch (const DotsTest::TestException & e)
    {
        std::wcout << "Caught exception: "<< Safir::Dob::Typesystem::Operations::GetName(e.GetTypeId()) << std::endl;
    }

    Safir::Dob::Typesystem::LibraryExceptions::Instance().Set(std::exception());
    try
    {
        Safir::Dob::Typesystem::LibraryExceptions::Instance().Throw();
    }
    catch (const std::exception &)
    {
        std::wcout << "Caught native exception" << std::endl;
    }

}

void Test_IsProperty()
{
        Header(L"IsProperty");
        Safir::Dob::Typesystem::TypeIdVector vect = Safir::Dob::Typesystem::Operations::GetAllTypeIds();
        Safir::Dob::Typesystem::TypeIdVector::iterator iter = vect.begin();

        while (iter != vect.end())
        {
            if (Safir::Dob::Typesystem::Operations::IsProperty(*iter))
            {
                //only care about the ones that are ours
                if (boost::starts_with(Safir::Dob::Typesystem::Operations::GetName(*iter),"DotsTest"))
                {
                    std::wcout << Safir::Dob::Typesystem::Operations::GetName(*iter) << std::endl;
                }
            }
            ++iter;
        }
}
void Test_IsEnumeration()
{
        Header(L"IsEnumeration");
        Safir::Dob::Typesystem::TypeIdVector vect = Safir::Dob::Typesystem::Operations::GetAllTypeIds();
        Safir::Dob::Typesystem::TypeIdVector::iterator iter = vect.begin();

        while (iter != vect.end())
        {
            if (Safir::Dob::Typesystem::Operations::IsEnumeration(*iter))
            {
                //only care about the ones that are ours
                if (boost::starts_with(Safir::Dob::Typesystem::Operations::GetName(*iter),"DotsTest"))
                {
                    std::wcout << Safir::Dob::Typesystem::Operations::GetName(*iter) << std::endl;
                }
            }
            ++iter;
        }
}
void Test_IsException()
{
        Header(L"IsException");
        Safir::Dob::Typesystem::TypeIdVector vect = Safir::Dob::Typesystem::Operations::GetAllTypeIds();
        Safir::Dob::Typesystem::TypeIdVector::iterator iter = vect.begin();

        while (iter != vect.end())
        {
            if (Safir::Dob::Typesystem::Operations::IsException(*iter))
            {
                //only care about the ones that are ours.
                if (boost::starts_with(Safir::Dob::Typesystem::Operations::GetName(*iter),"DotsTest"))
                {
                    std::wcout << Safir::Dob::Typesystem::Operations::GetName(*iter) << std::endl;
                }
            }
            ++iter;
        }
}

void FileCheck(const std::wstring& path, const std::wstring& expectedName)
{
    if (!boost::ends_with
        (path,
         expectedName))
    {
        std::wcout << "Failed to find " << expectedName << std::endl;
    }

    if (!boost::filesystem::exists(path))
    {
        std::wcout << "Dou file does not exist: " << path << std::endl;
    }
}
void Test_GetDouFilePath()
{
    using namespace Safir::Dob::Typesystem::Internal;
    FileCheck(GetDouFilePath(DotsTest::MemberTypes::ClassTypeId),
              L"DotsTest.MemberTypes.dou");

    FileCheck(GetDouFilePath(DotsTest::TestException::ExceptionTypeId),
              L"DotsTest.TestException.dou");

    FileCheck(GetDouFilePath(DotsTest::MemberTypesProperty::ClassTypeId),
              L"DotsTest.MemberTypesProperty.dou");
}

/* This test attempts to deserialize a piece of xml that represents a class
 * that is part of a dou library that this executable has not been linked with.
 * The idea being that this test will succeed if dots_cpp has loaded all the
 * required libraries as specified by typesystem.ini
 */
void Test_DeserializeUnlinkedObject()
{
    Header(L"DeserializeUnlinkedObject");
    const std::wstring xml(L"<?xml version=\"1.0\" encoding=\"utf-8\"?><DotsTest.ExtraObject><Int32Member>-32</Int32Member></DotsTest.ExtraObject>");

    const Safir::Dob::Typesystem::ObjectPtr obj = Safir::Dob::Typesystem::Serialization::ToObject(xml);

    std::wcout << "Class name: " << Safir::Dob::Typesystem::Operations::GetName(obj->GetTypeId()) << std::endl;

}


int tests = 0;
int failures = 0;


void Check(bool expr, const std::string & description = "")
{
    ++tests;
    if (!expr)
    {
        ++failures;
        std::wcout << "Testcase " << tests;
        if (!description.empty())
        {
            std::wcout << " '" << description.c_str() << "'";
        }
        std::wcout << " failed!" << std::endl;
    }
}

void ContainerTest()
{
    using namespace Safir::Dob::Typesystem;
    using namespace DotsTest;

    //    HandlerId h;

    //container testing
    {
        Int32Container intCont;
        Check(intCont.IsNull());
        Check(!intCont.IsChanged());
        intCont.SetVal(10);
        Check(intCont.GetVal() == 10);
        Check(!intCont.IsNull());
        Check(intCont.IsChanged());


        BooleanContainer boolCont;
        Check(boolCont.IsNull());
        Check(!boolCont.IsChanged());
        boolCont.SetVal(true);
        Check(boolCont.GetVal() == true);
        Check(!boolCont.IsNull());
        Check(boolCont.IsChanged());

        TestEnum::EnumerationContainer enumCont;
        Check(enumCont.IsNull());
        Check(!enumCont.IsChanged());
        enumCont.SetVal(TestEnum::MyFirst);
        Check(enumCont.GetVal() == TestEnum::MyFirst);
        Check(!enumCont.IsNull());
        Check(enumCont.IsChanged());

        Float32Container floatCont;
        Check(floatCont.IsNull());
        Check(!floatCont.IsChanged());
        floatCont.SetVal(10);
        Check(floatCont.GetVal() == 10);
        Check(!floatCont.IsNull());
        Check(floatCont.IsChanged());

        TypeIdContainer typeIdCont;
        Check(typeIdCont.IsNull());
        Check(!typeIdCont.IsChanged());
        typeIdCont.SetVal(10);
        Check(typeIdCont.GetVal() == 10);
        Check(!typeIdCont.IsNull());
        Check(typeIdCont.IsChanged());

        InstanceIdContainer instanceIdCont;
        Check(instanceIdCont.IsNull());
        Check(!instanceIdCont.IsChanged());
        instanceIdCont.SetVal(InstanceId(10));
        Check(instanceIdCont.GetVal() == InstanceId(10));
        Check(!instanceIdCont.IsNull());
        Check(instanceIdCont.IsChanged());

        StringContainer strCont;
        Check(strCont.IsNull());
        Check(!strCont.IsChanged());
        strCont.SetVal(L"Kalle");
        Check(strCont.GetVal() == L"Kalle");
        Check(!strCont.IsNull());
        Check(strCont.IsChanged());

    }

#define CheckNullExc(expr) \
    try{bool b = expr;b;Check(false);}catch(const Safir::Dob::Typesystem::NullException &){Check(true);}

    {
        MemberTypes t;
        Check(t.Int32Member().IsNull());
        Check(!t.Int32Member().IsChanged());
        CheckNullExc(t.Int32Member() == 10);
        CheckNullExc(10 == t.Int32Member());
        CheckNullExc(t.Int32Member() != 10);
        CheckNullExc(10 != t.Int32Member());
        t.Int32Member().SetVal(10);
        Check(!t.Int32Member().IsNull());
        Check(t.Int32Member().IsChanged());
        Check(t.Int32Member().GetVal() == 10);
        Check(t.Int32Member() == 10);
        const Int32 i = t.Int32Member();
        Check(i == 10);



        CheckNullExc(t.BooleanMember() == true);
        CheckNullExc(true == t.BooleanMember());
        CheckNullExc(t.BooleanMember());
        CheckNullExc(t.BooleanMember() != true);
        CheckNullExc(true != t.BooleanMember());
        CheckNullExc(t.BooleanMember());
        t.BooleanMember() = true;
        Check(!t.BooleanMember().IsNull());
        Check(t.BooleanMember().IsChanged());
        Check(t.BooleanMember() == true);
        Check(t.BooleanMember());
        t.BooleanMember().SetNull();
        t.BooleanMember().SetChanged(false);
        Check(t.BooleanMember().IsNull());
        Check(!t.BooleanMember().IsChanged());
        t.BooleanMember().SetVal(false);
        Check(!t.BooleanMember().IsNull());
        Check(t.BooleanMember().IsChanged());
        Check(t.BooleanMember() == false);
        Check(!t.BooleanMember());

        Check(t.Float32Member().IsNull());
        Check(!t.Float32Member().IsChanged());
        CheckNullExc(t.Float32Member() == 10);
        CheckNullExc(10 == t.Float32Member());
        CheckNullExc(t.Float32Member() != 10);
        CheckNullExc(10 != t.Float32Member());
        t.Float32Member().SetVal(10);
        Check(!t.Float32Member().IsNull());
        Check(t.Float32Member().IsChanged());
        Check(t.Float32Member().GetVal() == 10);
        Check(t.Float32Member() == 10.0);

        Check(t.TypeIdMember().IsNull());
        Check(!t.TypeIdMember().IsChanged());
        t.TypeIdMember().SetVal(10);
        Check(!t.TypeIdMember().IsNull());
        Check(t.TypeIdMember().IsChanged());
        Check(t.TypeIdMember().GetVal() == 10);
        Check(t.TypeIdMember() == 10);

        Check(t.InstanceIdMember().IsNull());
        Check(!t.InstanceIdMember().IsChanged());
        CheckNullExc(t.InstanceIdMember() == InstanceId(10));
        CheckNullExc(InstanceId(10) == t.InstanceIdMember());
        CheckNullExc(t.InstanceIdMember() != InstanceId(10));
        CheckNullExc(InstanceId(10) != t.InstanceIdMember());
        t.InstanceIdMember().SetVal(InstanceId(10));
        Check(!t.InstanceIdMember().IsNull());
        Check(t.InstanceIdMember().IsChanged());
        Check(t.InstanceIdMember().GetVal() == InstanceId(10));
        Check(InstanceId(10) == t.InstanceIdMember());
        Check(t.InstanceIdMember() == InstanceId(10));
        Check(InstanceId(11) != t.InstanceIdMember());
        Check(t.InstanceIdMember() != InstanceId(11));
        t.InstanceIdMember().SetVal(InstanceId(L"Kalle"));
        Check(InstanceId(L"Kalle") == t.InstanceIdMember());
        Check(t.InstanceIdMember() == InstanceId(L"Kalle"));
        Check(InstanceId(L"Pelle") != t.InstanceIdMember());
        Check(t.InstanceIdMember() != InstanceId(L"Pelle"));

        Check(t.StringMember().IsNull());
        Check(!t.StringMember().IsChanged());
        CheckNullExc(t.StringMember() == L"Kalle");
        CheckNullExc(L"Kalle" == t.StringMember());
        CheckNullExc(t.StringMember() != L"Kalle");
        CheckNullExc(L"Kalle" != t.StringMember());
        const std::wstring kalleS(L"Kalle");
        CheckNullExc(t.StringMember() == kalleS);
        CheckNullExc(kalleS == t.StringMember());
        CheckNullExc(t.StringMember() != kalleS);
        CheckNullExc(kalleS != t.StringMember());
        t.StringMember().SetVal(L"Kalle");
        Check(!t.StringMember().IsNull());
        Check(t.StringMember().IsChanged());
        Check(t.StringMember().GetVal() == L"Kalle");
        Check(t.StringMember() == L"Kalle");
        Check(L"Kalle" == t.StringMember());
        Check(t.StringMember() != L"Pelle");
        Check(L"Pelle" != t.StringMember());

        Check(t.BinaryMember().IsNull());
        Check(!t.BinaryMember().IsChanged());
        const char kalleC [] = "Kalle";
        const Safir::Dob::Typesystem::Binary kalle(kalleC,kalleC + sizeof(kalleC));
        CheckNullExc(t.BinaryMember() == kalle);
        CheckNullExc(kalle == t.BinaryMember());
        CheckNullExc(t.BinaryMember() != kalle);
        CheckNullExc(kalle != t.BinaryMember());
        t.BinaryMember().SetVal(kalle);
        Check(!t.BinaryMember().IsNull());
        Check(t.BinaryMember().IsChanged());
        Check(t.BinaryMember().GetVal() == kalle);
        Check(t.BinaryMember() == kalle);
        Check(kalle == t.BinaryMember());
        const char pelleC [] = "PellePlutt";
        const Safir::Dob::Typesystem::Binary pelle(pelleC,pelleC + sizeof(pelleC));
        Check(t.BinaryMember() != pelle);
        Check(pelle != t.BinaryMember());
    }

    {

        MemberTypes t;
        t.Int32Member() = 20;
        Check(!t.Int32Member().IsNull());
        Check(t.Int32Member().IsChanged());
        Check(t.Int32Member() == 20);
        t.Int32Member().SetNull();
        t.Int32Member().SetChanged(false);
        Check(t.Int32Member().IsNull());
        Check(!t.Int32Member().IsChanged());
        t.Int32Member().SetVal(30);
        Check(!t.Int32Member().IsNull());
        Check(t.Int32Member().IsChanged());
        Check(t.Int32Member() == 30);


        t.BooleanMember() = true;
        Check(!t.BooleanMember().IsNull());
        Check(t.BooleanMember().IsChanged());
        Check(t.BooleanMember() == true);
        Check(t.BooleanMember());
        t.BooleanMember().SetNull();
        t.BooleanMember().SetChanged(false);
        Check(t.BooleanMember().IsNull());
        Check(!t.BooleanMember().IsChanged());
        t.BooleanMember().SetVal(false);
        Check(!t.BooleanMember().IsNull());
        Check(t.BooleanMember().IsChanged());
        Check(t.BooleanMember() == false);
        Check(!t.BooleanMember());


        t.EnumerationMember() = TestEnum::MyFirst;
        Check(!t.EnumerationMember().IsNull());
        Check(t.EnumerationMember().IsChanged());
        Check(t.EnumerationMember() == TestEnum::MyFirst);
        t.EnumerationMember().SetNull();
        t.EnumerationMember().SetChanged(false);
        Check(t.EnumerationMember().IsNull());
        Check(!t.EnumerationMember().IsChanged());
        t.EnumerationMember().SetVal(TestEnum::MySecond);
        Check(!t.EnumerationMember().IsNull());
        Check(t.EnumerationMember().IsChanged());
        Check(t.EnumerationMember() == TestEnum::MySecond);


        t.Float32Member() = 20;
        Check(!t.Float32Member().IsNull());
        Check(t.Float32Member().IsChanged());
        Check(t.Float32Member() == 20);
        t.Float32Member().SetNull();
        t.Float32Member().SetChanged(false);
        Check(t.Float32Member().IsNull());
        Check(!t.Float32Member().IsChanged());
        t.Float32Member().SetVal(30);
        Check(!t.Float32Member().IsNull());
        Check(t.Float32Member().IsChanged());
        Check(t.Float32Member() == 30);


        t.TypeIdMember() = 20;
        Check(!t.TypeIdMember().IsNull());
        Check(t.TypeIdMember().IsChanged());
        Check(t.TypeIdMember() == 20);
        t.TypeIdMember().SetNull();
        t.TypeIdMember().SetChanged(false);
        Check(t.TypeIdMember().IsNull());
        Check(!t.TypeIdMember().IsChanged());
        t.TypeIdMember().SetVal(30);
        Check(!t.TypeIdMember().IsNull());
        Check(t.TypeIdMember().IsChanged());
        Check(t.TypeIdMember() == 30);


        t.InstanceIdMember() = InstanceId(20);
        Check(!t.InstanceIdMember().IsNull());
        Check(t.InstanceIdMember().IsChanged());
        Check(InstanceId(20) == t.InstanceIdMember());
        t.InstanceIdMember().SetNull();
        t.InstanceIdMember().SetChanged(false);
        Check(t.InstanceIdMember().IsNull());
        Check(!t.InstanceIdMember().IsChanged());
        t.InstanceIdMember().SetVal(InstanceId(30));
        Check(!t.InstanceIdMember().IsNull());
        Check(t.InstanceIdMember().IsChanged());
        Check(InstanceId(30) == t.InstanceIdMember());
        t.InstanceIdMember() = InstanceId(L"Kalle");
        Check(!t.InstanceIdMember().IsNull());
        Check(t.InstanceIdMember().IsChanged());
        Check(InstanceId(L"Kalle") == t.InstanceIdMember());
    }

    {
        MemberTypes t1;
        MemberTypes t2;
        //proxied container assign.
        t1.Int32Member() = 30;
        t1.Int32Member().SetChanged(false);
        t2.Int32Member() = t1.Int32Member();
        Check(t2.Int32Member() == 30);
        Check(t2.Int32Member().IsChanged());
        Check(!t1.Int32Member().IsChanged());

        //Operators
        ++t2.Int32Member();
        Check(t2.Int32Member() == 31);
        t2.Int32Member()++;
        Check(t2.Int32Member() == 32);
        t2.Int32Member() += 10;
        Check(t2.Int32Member() == 42);
        t2.Int32Member() *= 2;
        Check(t2.Int32Member() == 84);
        t2.Int32Member() /= 4;
        Check(t2.Int32Member() == 21);


        //proxied container assign.
        t1.BooleanMember() = false;
        t1.BooleanMember().SetChanged(false);
        t2.BooleanMember() = t1.BooleanMember();
        Check(!t2.BooleanMember());
        Check(t2.BooleanMember().IsChanged());
        Check(!t1.BooleanMember().IsChanged());

        //Operators N/A

        //proxied container assign.
        t1.EnumerationMember() = TestEnum::MyFirst;
        t1.EnumerationMember().SetChanged(false);
        t2.EnumerationMember() = t1.EnumerationMember();
        Check(t2.EnumerationMember() == TestEnum::MyFirst);
        Check(t2.EnumerationMember().IsChanged());
        Check(!t1.EnumerationMember().IsChanged());

        //Operators
        // Enum operator fail to compile, as should they
        //         ++t2.EnumerationMember();
        //         t2.EnumerationMember()++;
        //         t2.EnumerationMember() += TestEnum::MyFirst;
        //         t2.EnumerationMember() *= TestEnum::MySecond;
        //         t2.EnumerationMember() /= TestEnum::MyFirst;

        //proxied container assign.
        t1.Float32Member() = 30;
        t1.Float32Member().SetChanged(false);
        t2.Float32Member() = t1.Float32Member();
        Check(t2.Float32Member() == 30);
        Check(t2.Float32Member().IsChanged());
        Check(!t1.Float32Member().IsChanged());

        //Operators
        ++t2.Float32Member();
        Check(t2.Float32Member() == 31);
        t2.Float32Member()++;
        Check(t2.Float32Member() == 32);
        t2.Float32Member() += 10;
        Check(t2.Float32Member() == 42);

        t2.Float32Member() *= 2;
        Check(t2.Float32Member() == 84);
        t2.Float32Member() /= 4;
        Check(t2.Float32Member() == 21);

        //proxied container assign.
        t1.TypeIdMember() = 30;
        t1.TypeIdMember().SetChanged(false);
        t2.TypeIdMember() = t1.TypeIdMember();
        Check(t2.TypeIdMember() == 30);
        Check(t2.TypeIdMember().IsChanged());
        Check(!t1.TypeIdMember().IsChanged());

        //Operators
        ++t2.TypeIdMember();
        Check(t2.TypeIdMember() == 31);
        t2.TypeIdMember()++;
        Check(t2.TypeIdMember() == 32);
        t2.TypeIdMember() += 10;
        Check(t2.TypeIdMember() == 42);

        t2.TypeIdMember() *= 2;
        Check(t2.TypeIdMember() == 84);
        t2.TypeIdMember() /= 4;
        Check(t2.TypeIdMember() == 21);

        //proxied container assign.
        t1.InstanceIdMember() = InstanceId(30);
        t1.InstanceIdMember().SetChanged(false);
        t2.InstanceIdMember() = t1.InstanceIdMember();
        Check(InstanceId(30) == t2.InstanceIdMember());
        Check(t2.InstanceIdMember().IsChanged());
        Check(!t1.InstanceIdMember().IsChanged());

        //Operators for Instance Id fail (as should they!)
        /*
        ++t2.InstanceIdMember();
        t2.InstanceIdMember()++;
        t2.InstanceIdMember() += InstanceId(10);
        t2.InstanceIdMember() *= InstanceId(2);
        t2.InstanceIdMember() /= InstanceId(4);
        */
    }

    //objects
    {
        MemberTypes t;
        t.ObjectMember() = Safir::Dob::Typesystem::Object::Create();
        Check(t.ObjectMember().GetPtr()->GetTypeId() == t.ObjectMember()->GetTypeId());
    }

    //Copy
    {
        MemberTypes t1,t2;
        t1.Int32Member() = 10;
        t2.Int32Member().Copy(t1.Int32Member());
        Check(t2.Int32Member() == 10);
        Check(t2.IsChanged());
    }

    //sequences
    {
        DotsTest::MemberSequencesPtr ms=DotsTest::MemberSequences::Create();

        Check(ms->Int32Member().IsNull());
        Check(ms->Int32Member().empty());
        Check(!ms->Int32Member().IsChanged());
        ms->Int32Member().push_back(20);
        ms->Int32Member().push_back(30);
        ms->Int32Member().InsertAt(0, 10);
        Check(!ms->Int32Member().IsNull());
        Check(ms->Int32Member().IsChanged());
        Check(!ms->Int32Member().empty());
        Check(ms->Int32Member().size() == 3);
        ms->Int32Member().SetChanged(false);
        ms->Int32Member().SetNull();
        Check(ms->Int32Member().IsNull());
        Check(ms->Int32Member().empty());
        Check(ms->Int32Member().IsChanged());

        Check(!ms->TestClassMember().IsChanged());
        ms->TestClassMember().push_back(DotsTest::TestItem::Create());
        Check(ms->TestClassMember().IsChanged());
        ms->TestClassMember().SetChanged(false);
        ms->TestClassMember()[0]->MyInt() = 10;;
        Check(ms->TestClassMember()[0]->IsChanged());
        Check(ms->TestClassMember().IsChanged(), "sequence recursive ischanged");
        ms->TestClassMember()[0]->SetChanged(false);
        Check(!ms->TestClassMember().IsChanged(), "sequence recursive ischanged 2");
        ms->TestClassMember().SetChanged(true);
        Check(ms->TestClassMember().IsChanged());
        Check(ms->TestClassMember()[0]->IsChanged(), "sequence recursive ischanged 3");
    }

    //dictionaries
    {
        DotsTest::MemberDictionariesPtr md=DotsTest::MemberDictionaries::Create();

        Check(md->Int32StringMember().IsNull());
        Check(md->Int32StringMember().empty());
        Check(!md->Int32StringMember().IsChanged());
        md->Int32StringMember()[10].SetVal(DotsTest::ParameterDictionaries::Int32StringParameter(10));
        md->Int32StringMember()[20].SetVal(L"asdf");
        Check(!md->Int32StringMember().IsNull());
        Check(md->Int32StringMember().IsChanged());
        Check(!md->Int32StringMember().empty());
        Check(md->Int32StringMember().size() == 2);
        md->Int32StringMember().SetChanged(false);
        md->Int32StringMember().SetNull();
        Check(md->Int32StringMember().IsNull());
        Check(md->Int32StringMember().empty());
        Check(md->Int32StringMember().IsChanged());

        //check recursiveness
        Check(!md->Int32ItemMember().IsChanged());
        md->Int32ItemMember().Insert(0,DotsTest::MemberDictionaries::Create());
        Check(md->Int32ItemMember().IsChanged());
        md->Int32ItemMember().SetChanged(false);
        md->Int32ItemMember()[0]->Int32Int32Member().Insert(10,10);
        Check(md->Int32ItemMember()[0]->IsChanged());
        Check(md->Int32ItemMember().IsChanged());
        md->Int32ItemMember()[0]->SetChanged(false);
        Check(!md->Int32ItemMember().IsChanged());
        md->Int32ItemMember().SetChanged(true);
        Check(md->Int32ItemMember().IsChanged());
        Check(md->Int32ItemMember()[0]->IsChanged());
    }

    //Copying dictionaries with objects
    {
        DotsTest::MemberDictionariesPtr md=DotsTest::MemberDictionaries::Create();
        md->Int32ItemMember().Insert(0,DotsTest::MemberDictionaries::Create());
        md->Int32ItemMember()[0]->Int32ItemMember().Insert(10,DotsTest::MemberDictionaries::Create());
        md->Int32ItemMember()[0]->Int32ItemMember()[10]->Int32StringMember().Insert(20,L"Foobar");
        auto item = DotsTest::TestItem::Create();
        item->MyString().SetVal(L"plopp");
        md->StringObjectMember().Insert(L"plupp",item);

        DotsTest::MemberDictionariesPtr md2=DotsTest::MemberDictionaries::Create();
        Safir::Dob::Typesystem::Utilities::MergeChanges(md2,md);
        Check(md2->Int32ItemMember()[0]->Int32ItemMember()[10]->Int32StringMember()[20].GetVal() == L"Foobar");

        Safir::Dob::Typesystem::DictionaryContainer<Safir::Dob::Typesystem::Int32, DotsTest::MemberDictionariesContainer> cont;
        cont.Copy(md->Int32ItemMember());
    }

    //sequence container Object specialization
    {
        using namespace Safir::Dob::Typesystem;
        using namespace DotsTest;
        ObjectSequenceContainer oc;
        Check(!oc.IsChanged());
        Check(!oc.IsChangedHere());
        oc.SetChangedHere(true);
        Check(oc.IsChanged());
        Check(oc.IsChangedHere());
        oc.SetChanged(false);
        oc.push_back(Object::Create());
        Check(oc.IsChanged());
        Check(oc.IsChangedHere());
        TestItemPtr ti = TestItem::Create();
        oc.push_back(ti);
        oc.SetChanged(false);
        ti->MyInt() = 10;
        Check(oc.IsChanged(), "recursive ischanged");
        Check(!oc.IsChangedHere(), "recursive ischanged 2");
        Check(oc[1]->IsChanged(), "recursive ischanged 3");
    }

    if (failures != 0)
    {
        std::wcout << "There were " << failures << " failures when running ContainerTest! (out of " << tests << " tests)" << std::endl;
    }

}

void PrintSequences(DotsTest::MemberSequencesPtr ms)
{
    std::wcout<<L"--- Int32Member ---"<<std::endl;
    std::wcout<<L"size: "<<ms->Int32Member().size()<<std::endl;
    std::wcout<<L"isChanged: "<<ms->Int32Member().IsChanged()<<std::endl;
    std::wcout<<L"val[0]: "<<ms->Int32Member()[0]<<std::endl;
    std::wcout<<L"val[1]: "<<ms->Int32Member()[1]<<std::endl;

    std::wcout<<L"--- Int64Member ---"<<std::endl;
    std::wcout<<L"size: "<<ms->Int64Member().size()<<std::endl;
    std::wcout<<L"isChanged: "<<ms->Int64Member().IsChanged()<<std::endl;
    std::wcout<<L"val[0]: "<<ms->Int64Member()[0]<<std::endl;
    std::wcout<<L"val[1]: "<<ms->Int64Member()[1]<<std::endl;

    std::wcout<<L"--- Float32Member ---"<<std::endl;
    std::wcout<<L"size: "<<ms->Float32Member().size()<<std::endl;
    std::wcout<<L"isChanged: "<<ms->Float32Member().IsChanged()<<std::endl;
    std::wcout<<L"val[0]: "<<ms->Float32Member()[0]<<std::endl;
    std::wcout<<L"val[1]: "<<ms->Float32Member()[1]<<std::endl;

    std::wcout<<L"--- Float64Member ---"<<std::endl;
    std::wcout<<L"size: "<<ms->Float64Member().size()<<std::endl;
    std::wcout<<L"isChanged: "<<ms->Float64Member().IsChanged()<<std::endl;
    std::wcout<<L"val[0]: "<<ms->Float64Member()[0]<<std::endl;
    std::wcout<<L"val[1]: "<<ms->Float64Member()[1]<<std::endl;

    std::wcout<<L"--- BooleanMember ---"<<std::endl;
    std::wcout<<L"size: "<<ms->BooleanMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<ms->BooleanMember().IsChanged()<<std::endl;
    std::wcout<<L"val[0]: "<<ms->BooleanMember()[0]<<std::endl;
    std::wcout<<L"val[1]: "<<ms->BooleanMember()[1]<<std::endl;

    std::wcout<<L"--- EnumerationMember ---"<<std::endl;
    std::wcout<<L"size: "<<ms->EnumerationMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<ms->EnumerationMember().IsChanged()<<std::endl;
    std::wcout<<L"val[0]: "<<DotsTest::TestEnum::ToString(ms->EnumerationMember()[0])<<std::endl;
    std::wcout<<L"val[1]: "<<DotsTest::TestEnum::ToString(ms->EnumerationMember()[1])<<std::endl;

    std::wcout<<L"--- StringMember ---"<<std::endl;
    std::wcout<<L"size: "<<ms->StringMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<ms->StringMember().IsChanged()<<std::endl;
    std::wcout<<L"val[0]: "<<ms->StringMember()[0]<<std::endl;
    std::wcout<<L"val[1]: "<<ms->StringMember()[1]<<std::endl;

    std::wcout<<L"--- TypeIdMember ---"<<std::endl;
    std::wcout<<L"size: "<<ms->TypeIdMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<ms->TypeIdMember().IsChanged()<<std::endl;
    std::wcout<<L"val[0]: "<<ts::Operations::GetName(ms->TypeIdMember()[0])<<std::endl;
    std::wcout<<L"val[1]: "<<ts::Operations::GetName(ms->TypeIdMember()[1])<<std::endl;

    std::wcout<<L"--- HandlerIdMember ---"<<std::endl;
    std::wcout<<L"size: "<<ms->HandlerIdMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<ms->HandlerIdMember().IsChanged()<<std::endl;
}

void TestSequences()
{
    Header(L"Sequences");

    DotsTest::MemberSequencesPtr ms=DotsTest::MemberSequences::Create();

    ms->Int32Member().push_back(20);
    ms->Int32Member().push_back(30);
    ms->Int32Member().InsertAt(0, 10);
    ms->Int32Member().EraseAt(2);

    ms->Int64Member().push_back(200);
    ms->Int64Member().push_back(300);
    ms->Int64Member().InsertAt(0, 100);
    ms->Int64Member().EraseAt(2);

    ms->Float32Member().push_back(2.2f);
    ms->Float32Member().push_back(3.3f);
    ms->Float32Member().InsertAt(0, 1.1f);
    ms->Float32Member().EraseAt(2);

    ms->Float64Member().push_back(22.2);
    ms->Float64Member().push_back(33.3);
    ms->Float64Member().InsertAt(0, 11.1);
    ms->Float64Member().EraseAt(2);

    ms->BooleanMember().push_back(false);
    ms->BooleanMember().push_back(false);
    ms->BooleanMember().InsertAt(0, true);
    ms->BooleanMember().EraseAt(2);

    ms->EnumerationMember().push_back(DotsTest::TestEnum::MySecond);
    ms->EnumerationMember().push_back(DotsTest::TestEnum::MyThird);
    ms->EnumerationMember().InsertAt(0, DotsTest::TestEnum::MyFirst);
    ms->EnumerationMember().EraseAt(2);

    ms->StringMember().push_back(L"Bb");
    ms->StringMember().push_back(L"Cc");
    ms->StringMember().InsertAt(0, L"Aa");
    ms->StringMember().EraseAt(2);

    ms->TypeIdMember().push_back(DotsTest::MemberSequences::ClassTypeId);
    ms->TypeIdMember().push_back(DotsTest::TestEnum::EnumerationTypeId);
    ms->TypeIdMember().InsertAt(0, DotsTest::MemberDictionaries::ClassTypeId);
    ms->TypeIdMember().EraseAt(2);

    PrintSequences(ms);

    std::wcout<<L"------ To Xml -----"<<std::endl;
    std::wstring xml=ts::Serialization::ToXml(ms);
    std::wcout<<xml<<std::endl;

    std::wcout<<L"------ From Xml -----"<<std::endl;
    DotsTest::MemberSequencesPtr fromXml=std::dynamic_pointer_cast<DotsTest::MemberSequences>(ts::Serialization::ToObject(xml));
    PrintSequences(fromXml);


    std::wcout<<L"------ To Json -----"<<std::endl;
    std::wstring json=ts::Serialization::ToJson(ms);
    std::wcout<<json<<std::endl;

    std::wcout<<L"------ From Json -----"<<std::endl;
    DotsTest::MemberSequencesPtr fromJson=std::dynamic_pointer_cast<DotsTest::MemberSequences>(ts::Serialization::ToObjectFromJson(json));
    PrintSequences(fromJson);
}


void PrintDictionaries(DotsTest::MemberDictionariesPtr md)
{
    std::wcout<<L"--- Int32StringMember ---"<<std::endl;
    std::wcout<<L"size: "<<md->Int32StringMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<md->Int32StringMember().IsChanged()<<std::endl;
    {
        typedef std::map<ts::Int32, ts::StringContainer> Sorted;
        Sorted sorted(md->Int32StringMember().begin(), md->Int32StringMember().end());
        for (Sorted::const_iterator it=sorted.begin(); it!=sorted.end(); ++it)
        {
            if (it->second.IsNull())
                std::wcout<<it->first<<L" = NULL, changed: "<<it->second.IsChanged()<<std::endl;
            else
                std::wcout<<it->first<<L" = "<<it->second.GetVal()<<L", changed: "<<it->second.IsChanged()<<std::endl;
        }
    }

    std::wcout<<L"--- Int64BinaryMember ---"<<std::endl;
    std::wcout<<L"size: "<<md->Int64BinaryMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<md->Int64BinaryMember().IsChanged()<<std::endl;
    {
        typedef std::map<ts::Int64, ts::BinaryContainer> Sorted;
        Sorted sorted(md->Int64BinaryMember().begin(), md->Int64BinaryMember().end());
        for (Sorted::const_iterator it=sorted.begin(); it!=sorted.end(); ++it)
        {
            if (it->second.IsNull())
                std::wcout<<it->first<<L" = NULL, changed: "<<it->second.IsChanged()<<std::endl;
            else
                std::wcout<<it->first<<L" = "<<ts::Utilities::ToWstring(std::string(it->second.GetVal().begin(), it->second.GetVal().end()))<<L", changed: "<<it->second.IsChanged()<<std::endl;
        }
    }


    std::wcout<<L"--- TypeIdEnumMember ---"<<std::endl;
    std::wcout<<L"size: "<<md->TypeIdEnumMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<md->TypeIdEnumMember().IsChanged()<<std::endl;
    {
        typedef std::map<ts::TypeId, DotsTest::TestEnum::EnumerationContainer> Sorted;
        Sorted sorted(md->TypeIdEnumMember().begin(), md->TypeIdEnumMember().end());
        for (Sorted::const_iterator it=sorted.begin(); it!=sorted.end(); ++it)
        {
            if (it->second.IsNull())
                std::wcout<<ts::Operations::GetName(it->first)<<L" = NULL, changed: "<<it->second.IsChanged()<<std::endl;
            else
                std::wcout<<ts::Operations::GetName(it->first)<<L" = "<<DotsTest::TestEnum::ToString(it->second.GetVal())<<L", changed: "<<it->second.IsChanged()<<std::endl;
        }
    }

    std::wcout<<L"--- EnumInstanceIdMember ---"<<std::endl;
    std::wcout<<L"size: "<<md->EnumInstanceIdMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<md->EnumInstanceIdMember().IsChanged()<<std::endl;
    {
        typedef std::map<DotsTest::TestEnum::Enumeration, ts::InstanceIdContainer> Sorted;
        Sorted sorted(md->EnumInstanceIdMember().begin(), md->EnumInstanceIdMember().end());
        for (Sorted::const_iterator it=sorted.begin(); it!=sorted.end(); ++it)
        {
            if (it->second.IsNull())
                std::wcout<<DotsTest::TestEnum::ToString(it->first)<<L" = NULL, changed: "<<it->second.IsChanged()<<std::endl;
            else
                std::wcout<<DotsTest::TestEnum::ToString(it->first)<<L" = "<<it->second.GetVal().ToString()<<L", changed: "<<it->second.IsChanged()<<std::endl;
        }
    }

    std::wcout<<L"--- InstanceIdEntityIdMember ---"<<std::endl;
    std::wcout<<L"size: "<<md->InstanceIdEntityIdMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<md->InstanceIdEntityIdMember().IsChanged()<<std::endl;
    {
        typedef std::map<ts::InstanceId, ts::EntityIdContainer> Sorted;
        Sorted sorted(md->InstanceIdEntityIdMember().begin(), md->InstanceIdEntityIdMember().end());
        for (Sorted::const_iterator it=sorted.begin(); it!=sorted.end(); ++it)
        {
            if (it->second.IsNull())
                std::wcout<<it->first.ToString()<<L" = NULL, changed: "<<it->second.IsChanged()<<std::endl;
            else
                std::wcout<<it->first.ToString()<<L" = "<<it->second.GetVal().ToString()<<L", changed: "<<it->second.IsChanged()<<std::endl;
        }
    }

    std::wcout<<L"--- EntityIdHandlerIdMember ---"<<std::endl;
    std::wcout<<L"size: "<<md->EntityIdHandlerIdMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<md->EntityIdHandlerIdMember().IsChanged()<<std::endl;
    {
        typedef std::map<ts::EntityId, ts::HandlerIdContainer> Sorted;
        Sorted sorted(md->EntityIdHandlerIdMember().begin(), md->EntityIdHandlerIdMember().end());
        for (Sorted::const_iterator it=sorted.begin(); it!=sorted.end(); ++it)
        {
            if (it->second.IsNull())
                std::wcout<<it->first.ToString()<<L" = NULL, changed: "<<it->second.IsChanged()<<std::endl;
            else
                std::wcout<<it->first.ToString()<<L" = "<<it->second.GetVal().ToString()<<L", changed: "<<it->second.IsChanged()<<std::endl;
        }
    }

    std::wcout<<L"--- StringItemMember ---"<<std::endl;
    std::wcout<<L"size: "<<md->StringItemMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<md->StringItemMember().IsChanged()<<std::endl;
    {
        typedef std::map<std::wstring, DotsTest::MemberDictionariesContainer> Sorted;
        Sorted sorted(md->StringItemMember().begin(), md->StringItemMember().end());
        for (Sorted::const_iterator it=sorted.begin(); it!=sorted.end(); ++it)
        {
            if (it->second.IsNull())
                std::wcout<<it->first<<L" = NULL, changed: "<<it->second.IsChanged()<<std::endl;
            else
                std::wcout<<it->first<<L" = "<<ts::Serialization::ToJson(it->second.GetPtr())<<L", changed: "<<it->second.IsChanged()<<std::endl;
        }
    }

    std::wcout<<L"--- StringObjectMember ---"<<std::endl;
    std::wcout<<L"size: "<<md->StringObjectMember().size()<<std::endl;
    std::wcout<<L"isChanged: "<<md->StringObjectMember().IsChanged()<<std::endl;
    {
        typedef std::map<std::wstring, ts::ObjectContainer> Sorted;
        Sorted sorted(md->StringObjectMember().begin(), md->StringObjectMember().end());
        for (Sorted::const_iterator it=sorted.begin(); it!=sorted.end(); ++it)
        {
            if (it->second.IsNull())
                std::wcout<<it->first<<L" = NULL, changed: "<<it->second.IsChanged()<<std::endl;
            else
                std::wcout<<it->first<<L" = "<<ts::Serialization::ToJson(it->second.GetPtr())<<L", changed: "<<it->second.IsChanged()<<std::endl;
        }
    }
}


void PrintDictionaryMemberInfo()
{
    Safir::Dob::Typesystem::MemberType  lMT;
    Safir::Dob::Typesystem::MemberType  lKT;
    const char*                         lMN;
    Safir::Dob::Typesystem::TypeId      lMTId;
    Safir::Dob::Typesystem::TypeId      lKTId;
    Safir::Dob::Typesystem::Int32       lSL;
    Safir::Dob::Typesystem::CollectionType lCT;
    Safir::Dob::Typesystem::Int32       lAL;


    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberDictionaries::ClassTypeId,
                                             DotsTest::MemberDictionaries::Int32StringMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );
    std::wcout <<"GetInfo: " <<lMT << "," << lKT << ","
               << lMN << "," << lMTId << "," << lKTId
               << "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;

    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberDictionaries::ClassTypeId,
                                             DotsTest::MemberDictionaries::Int64BinaryMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );
    std::wcout <<"GetInfo: " <<lMT << "," << lKT << ","
               << lMN << "," << lMTId << "," << lKTId
               << "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;

    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberDictionaries::ClassTypeId,
                                             DotsTest::MemberDictionaries::TypeIdEnumMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );
    std::wcout <<"GetInfo: " <<lMT << "," << lKT << ","
               << lMN << "," << lMTId << "," << lKTId
               << "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;

    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberDictionaries::ClassTypeId,
                                             DotsTest::MemberDictionaries::EnumInstanceIdMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );
    std::wcout <<"GetInfo: " <<lMT << "," << lKT << ","
               << lMN << "," << lMTId << "," << lKTId
               << "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;

    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberDictionaries::ClassTypeId,
                                             DotsTest::MemberDictionaries::InstanceIdEntityIdMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );
    std::wcout <<"GetInfo: " <<lMT << "," << lKT << ","
               << lMN << "," << lMTId << "," << lKTId
               << "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;

    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberDictionaries::ClassTypeId,
                                             DotsTest::MemberDictionaries::EntityIdHandlerIdMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );
    std::wcout <<"GetInfo: " <<lMT << "," << lKT << ","
               << lMN << "," << lMTId << "," << lKTId
               << "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;

    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberDictionaries::ClassTypeId,
                                             DotsTest::MemberDictionaries::StringItemMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );
    std::wcout <<"GetInfo: " <<lMT << "," << lKT << ","
               << lMN << "," << lMTId << "," << lKTId
               << "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;

    Safir::Dob::Typesystem::Members::GetInfo(DotsTest::MemberDictionaries::ClassTypeId,
                                             DotsTest::MemberDictionaries::StringObjectMemberMemberIndex(),
                                             lMT, lKT, lMN, lMTId, lKTId, lSL, lCT, lAL );
    std::wcout <<"GetInfo: " <<lMT << "," << lKT << ","
               << lMN << "," << lMTId << "," << lKTId
               << "," << lSL << "," << CollectionTypeStr(lCT) << "," << lAL <<std::endl;
}

void TestDictionaries()
{
    Header(L"Dictionaries");

    PrintDictionaryMemberInfo();

    DotsTest::MemberDictionariesPtr md=DotsTest::MemberDictionaries::Create();

    md->Int32StringMember()[10].SetVal(DotsTest::ParameterDictionaries::Int32StringParameter(10));
    md->Int32StringMember()[20].SetVal(DotsTest::ParameterDictionaries::Int32StringParameter(20));

    md->Int64BinaryMember()[100].SetVal(DotsTest::ParameterDictionaries::Int32BinaryParameter(10));
    md->Int64BinaryMember()[200].SetVal(DotsTest::ParameterDictionaries::Int32BinaryParameter(20));

    md->TypeIdEnumMember().Insert(DotsTest::MemberDictionaries::ClassTypeId,DotsTest::ParameterDictionaries::StringEnumParameter(L"Billy"));
    md->TypeIdEnumMember()[DotsTest::MemberSequences::ClassTypeId].SetVal(DotsTest::ParameterDictionaries::StringEnumParameter(L"Svarre"));

    md->EnumInstanceIdMember().Insert(DotsTest::TestEnum::MyFirst,DotsTest::ParameterDictionaries::EnumInstanceIdParameter(DotsTest::TestEnum::MyFirst));
    md->EnumInstanceIdMember()[DotsTest::TestEnum::MySecond].SetVal(DotsTest::ParameterDictionaries::EnumInstanceIdParameter(DotsTest::TestEnum::MySecond));

    md->InstanceIdEntityIdMember().Insert(ts::InstanceId(L"FirstInstance"),DotsTest::ParameterDictionaries::HandlerIdEntityIdParameter(ts::HandlerId(L"handlerOne")));
    md->InstanceIdEntityIdMember()[ts::InstanceId(L"SecondInstance")].SetVal(DotsTest::ParameterDictionaries::HandlerIdEntityIdParameter(ts::HandlerId(2)));

    DotsTest::MemberDictionariesPtr item1=DotsTest::MemberDictionaries::Create();
    item1->EntityIdHandlerIdMember().Insert(ts::EntityId(Safir::Dob::Entity::ClassTypeId, ts::InstanceId(L"first")),DotsTest::ParameterDictionaries::EntityIdHandlerIdParameter(ts::EntityId(Safir::Dob::Entity::ClassTypeId, ts::InstanceId(L"first"))));
    item1->EntityIdHandlerIdMember()[ts::EntityId(Safir::Dob::Entity::ClassTypeId, ts::InstanceId(2))].SetVal(DotsTest::ParameterDictionaries::EntityIdHandlerIdParameter(ts::EntityId(Safir::Dob::Entity::ClassTypeId, ts::InstanceId(L"second"))));

    md->StringItemMember().Insert(L"Karl",item1);
    md->StringItemMember()[L"Philip"].SetNull();
    md->StringItemMember()[L"Gustav"].SetPtr(item1);

    md->StringObjectMember().Insert(L"Dilbert",DotsTest::ParameterDictionaries::Int32ObjectParameter(10));
    md->StringObjectMember()[L"Wally"].SetPtr(DotsTest::ParameterDictionaries::Int32ObjectParameter(20));

    PrintDictionaries(md);

    std::wcout<<L"------ To Xml -----"<<std::endl;
    std::wstring xml=ts::Serialization::ToXml(md);
    std::wcout<<xml<<std::endl;

    std::wcout<<L"------ From Xml -----"<<std::endl;
    DotsTest::MemberDictionariesPtr fromXml=std::dynamic_pointer_cast<DotsTest::MemberDictionaries>(ts::Serialization::ToObject(xml));
    PrintDictionaries(fromXml);


    std::wcout<<L"------ To Json -----"<<std::endl;
    std::wstring json=ts::Serialization::ToJson(md);
    std::wcout<<json<<std::endl;

    std::wcout<<L"------ From Json -----"<<std::endl;
    DotsTest::MemberDictionariesPtr fromJson=std::dynamic_pointer_cast<DotsTest::MemberDictionaries>(ts::Serialization::ToObjectFromJson(json));
    PrintDictionaries(fromJson);
}

void Test_SequenceDiff()
{
    DotsTest::MemberSequencesPtr ms1=DotsTest::MemberSequences::Create();
    ms1->TestClassMember().push_back(DotsTest::TestItem::Create());
    ms1->SetChanged(false);

    DotsTest::MemberSequencesPtr ms2=DotsTest::MemberSequences::Create();
    ms2->SetChanged(false);

    std::vector<char> bin1, bin2;
    Safir::Dob::Typesystem::Serialization::ToBinary(ms1,bin1);
    Safir::Dob::Typesystem::Serialization::ToBinary(ms2,bin2);
    Safir::Dob::Typesystem::Internal::BlobWriteHelper helper(&bin1[0]);
    helper.Diff(&bin2[0]);
    boost::shared_ptr<char> blobHolder(helper.ToBlob(),
                                       Safir::Dob::Typesystem::Internal::BlobOperations::Delete);

    DotsTest::MemberSequencesPtr ms = std::static_pointer_cast<DotsTest::MemberSequences>
        (Safir::Dob::Typesystem::ObjectFactory::Instance().CreateObject(blobHolder.get()));

    Check (ms->TestClassMember().IsChanged());
    Check (ms->TestClassMember().IsChangedHere());
    Check (ms->TestClassMember().size() == 1);
    Check (ms->TestClassMember()[0]->MyInt().IsChanged());
}

BOOST_AUTO_TEST_CASE (old_style_tests)
{
    std::wcout << std::boolalpha;

    MT1 = DotsTest::MemberTypes::Create();
    MT2 = DotsTest::MemberTypes::Create();
    MA1 = DotsTest::MemberArrays::Create();
    MA2 = DotsTest::MemberArrays::Create();
    MS1 = DotsTest::MemberSequences::Create();
    MI = DotsTest::MemberItems::Create();
    MIA = DotsTest::MemberItemsArray::Create();
    EO = DotsTest::EmptyObject::Create();
    AEO = DotsTest::AnotherEmptyObject::Create();

    try
    {
        Test_SequenceDiff();
        Test_Has_Property();
        Test_PropertyMappingKind();
        Test_Property_GetParameterReference();
        Test_GetName();
        Test_GetNumberOfMembers();
        Test_GetNumberOfParameters();
        Test_Create_Routines();
        Test_Int32();
        Test_Int64();
        Test_Float32();
        Test_Float64();
        Test_Boolean();
        Test_Enumeration();
        Test_String();
        Test_EntityId();
        Test_InstanceId();
        Test_TypeId();
        Test_ChannelId();
        Test_HandlerId();
        Test_Object();
        Test_Binary();
        Test_TestClass();
        Test_Ampere32();
        Test_CubicMeter32();
        Test_Hertz32();
        Test_Joule32();
        Test_Kelvin32();
        Test_Kilogram32();
        Test_Meter32();
        Test_MeterPerSecond32();
        Test_MeterPerSecondSquared32();
        Test_Newton32();
        Test_Pascal32();
        Test_Radian32();
        Test_RadianPerSecond32();
        Test_RadianPerSecondSquared32();
        Test_Second32();
        Test_SquareMeter32();
        Test_Steradian32();
        Test_Volt32();
        Test_Watt32();
        Test_Ampere64();
        Test_CubicMeter64();
        Test_Hertz64();
        Test_Joule64();
        Test_Kelvin64();
        Test_Kilogram64();
        Test_Meter64();
        Test_MeterPerSecond64();
        Test_MeterPerSecondSquared64();
        Test_Newton64();
        Test_Pascal64();
        Test_Radian64();
        Test_RadianPerSecond64();
        Test_RadianPerSecondSquared64();
        Test_Second64();
        Test_SquareMeter64();
        Test_Steradian64();
        Test_Volt64();
        Test_Watt64();
        Test_TestException();
        Test_LibraryExceptions();
        Test_IsProperty();
        Test_IsEnumeration();
        Test_IsException();
        Test_GetDouFilePath();
        TestSequences();
        TestDictionaries();
        Test_DeserializeUnlinkedObject();

        ContainerTest();
    }
    catch (const Safir::Dob::Typesystem::FundamentalException & e)
    {
        std::wcout << "Caught exception in _tmain: "<<std::endl;
        std::wcout << e.GetExceptionInfo() << std::endl;
    }
    catch (...)
    {
        std::wcout << "Caught ... exception in _tmain: "<<std::endl;
    }
}

using namespace DotsTest;
using namespace Safir;
using namespace Safir::Dob;
using namespace Safir::Dob::Typesystem;

template<class T>
void run_serialization_checks(const T& before,
                              std::function<void(const T& obj)> checks)
{
    BOOST_TEST_MESSAGE("-- BEFORE --");
    checks(before);
    BinarySerialization bin;
    Serialization::ToBinary(before,bin);

    T after = std::static_pointer_cast<typename T::element_type>(ObjectFactory::Instance().CreateObject(&bin[0]));
    BOOST_TEST_MESSAGE("-- AFTER --");
    checks(after);
}

BOOST_AUTO_TEST_CASE(member_types)
{
    MemberTypesPtr before = MemberTypes::Create();
    before->Int32Member() = 10;
    before->StringMember() = L"asdf";
    before->ObjectMember() = Object::Create();

    //change flag only set inside item, not on the member itself
    before->TestClassMember() = TestItem::Create();
    before->TestClassMember()->MyInt() = 1;
    before->TestClassMember().SetChangedHere(false);

    auto checks = [](const MemberTypesPtr& obj)
    {
        BOOST_CHECK(obj->IsChanged());
        BOOST_CHECK(obj->Int32Member().IsChanged());
        BOOST_CHECK(obj->StringMember().IsChanged());
        BOOST_CHECK(obj->ObjectMember().IsChanged());
        BOOST_CHECK(!obj->ObjectMember()->IsChanged());
        BOOST_CHECK(obj->TestClassMember().IsChanged());
        BOOST_CHECK(!obj->TestClassMember().IsChangedHere());
        BOOST_CHECK(obj->TestClassMember()->MyInt().IsChanged());
    };

    run_serialization_checks<MemberTypesPtr>(before,checks);

}

BOOST_AUTO_TEST_CASE(member_types_2)
{
    MemberTypesPtr before = MemberTypes::Create();
    //change flag only set on member, not inside item
    before->TestClassMember() = TestItem::Create();
    before->TestClassMember()->MyInt() = 1;
    before->SetChanged(false);
    before->TestClassMember().SetChangedHere(true);

    auto checks = [](const MemberTypesPtr& obj)
    {
        BOOST_CHECK(obj->IsChanged());
        BOOST_CHECK(obj->TestClassMember().IsChanged());
        BOOST_CHECK(obj->TestClassMember().IsChangedHere());
        BOOST_CHECK(!obj->TestClassMember()->MyInt().IsChanged());
    };

    run_serialization_checks<MemberTypesPtr>(before,checks);
}


BOOST_AUTO_TEST_CASE(member_arrays)
{
    MemberArraysPtr before = MemberArrays::Create();
    before->Int32Member()[0].SetVal(10);
    before->StringMember()[0].SetVal(L"asdf");
    before->ObjectMember()[1].SetPtr(Object::Create());

    //change flag only set inside item, not on the member itself
    before->TestClassMember()[0].SetPtr(TestItem::Create());
    before->TestClassMember()[0]->MyInt() = 1;
    before->TestClassMember()[0].SetChangedHere(false);

    //change flag only set on member, not inside item
    before->TestClassMember()[1].SetPtr(TestItem::Create());
    before->TestClassMember()[1]->MyInt() = 1;
    before->TestClassMember()[1]->SetChanged(false);
    before->TestClassMember()[1].SetChangedHere(true);

    auto checks = [](const MemberArraysPtr& obj)
    {
        BOOST_CHECK(obj->IsChanged());
        BOOST_CHECK(obj->Int32Member().IsChanged());
        BOOST_CHECK(obj->Int32Member()[0].IsChanged());
        BOOST_CHECK(!obj->Int32Member()[1].IsChanged());
        BOOST_CHECK(obj->StringMember().IsChanged());
        BOOST_CHECK(obj->StringMember()[0].IsChanged());
        BOOST_CHECK(!obj->StringMember()[1].IsChanged());
        BOOST_CHECK(obj->ObjectMember().IsChanged());
        BOOST_CHECK(!obj->ObjectMember()[0].IsChanged());
        BOOST_CHECK(obj->ObjectMember()[1].IsChanged());

        BOOST_CHECK(obj->TestClassMember().IsChanged());
        BOOST_CHECK(obj->TestClassMember()[0].IsChanged());
        BOOST_CHECK(!obj->TestClassMember()[0].IsChangedHere());
        BOOST_CHECK(obj->TestClassMember()[0]->MyInt().IsChanged());
        BOOST_CHECK(obj->TestClassMember()[1].IsChanged());
        BOOST_CHECK(obj->TestClassMember()[1].IsChangedHere());
        BOOST_CHECK(!obj->TestClassMember()[1]->MyInt().IsChanged());
    };

    run_serialization_checks<MemberArraysPtr>(before,checks);
}


BOOST_AUTO_TEST_CASE(member_sequences)
{
    MemberSequencesPtr before = MemberSequences::Create();
    BOOST_CHECK(!before->IsChanged());

    BOOST_CHECK(!before->Int32Member().IsChanged());
    BOOST_CHECK(!before->StringMember().IsChanged());
    BOOST_CHECK(!before->ObjectMember().IsChanged());
    BOOST_CHECK(!before->ObjectMember().IsChangedHere());

    before->Int32Member().push_back(10);
    before->StringMember().push_back(L"asdf");
    before->ObjectMember().push_back(Object::Create());


    //change flag only set inside item, not on the member itself
    before->TestClassMember().push_back(TestItem::Create());
    before->TestClassMember()[0]->MyInt() = 1;
    before->TestClassMember().SetChangedHere(false);

    auto checks = [](const MemberSequencesPtr& obj)
    {
        BOOST_CHECK(obj->IsChanged());
        BOOST_CHECK(obj->Int32Member().IsChanged());
        BOOST_CHECK(obj->StringMember().IsChanged());
        BOOST_CHECK(obj->ObjectMember().IsChanged());
        BOOST_CHECK(obj->ObjectMember().IsChangedHere());
        BOOST_CHECK(!obj->ObjectMember()[0]->IsChanged());

        BOOST_CHECK(obj->TestClassMember().IsChanged());
        BOOST_CHECK(obj->TestClassMember()[0]->IsChanged());
        BOOST_CHECK(!obj->TestClassMember().IsChangedHere());
        BOOST_CHECK(obj->TestClassMember()[0]->MyInt().IsChanged());
    };

    run_serialization_checks<MemberSequencesPtr>(before,checks);
}


BOOST_AUTO_TEST_CASE(member_sequences_2)
{
    MemberSequencesPtr before = MemberSequences::Create();

    //change flag only set on member, not inside item
    before->TestClassMember().push_back(TestItem::Create());
    before->TestClassMember()[0]->MyInt() = 1;
    before->SetChanged(false);
    before->TestClassMember().SetChangedHere(true);

    auto checks = [](const MemberSequencesPtr& obj)
    {
        BOOST_CHECK(obj->IsChanged());
        BOOST_CHECK(obj->TestClassMember().IsChanged());
        BOOST_CHECK(!obj->TestClassMember()[0]->IsChanged());
        BOOST_CHECK(obj->TestClassMember().IsChangedHere());
        BOOST_CHECK(!obj->TestClassMember()[0]->MyInt().IsChanged());
    };

    run_serialization_checks<MemberSequencesPtr>(before,checks);

}


BOOST_AUTO_TEST_CASE(member_dictionaries)
{
    MemberDictionariesPtr before = MemberDictionaries::Create();
    BOOST_CHECK(!before->IsChanged());

    BOOST_CHECK(!before->Int32Int32Member().IsChanged());
    BOOST_CHECK(!before->Int32ObjectMember().IsChanged());
    BOOST_CHECK(!before->Int32ObjectMember().IsChangedHere());

    // change flag only set inside value, not on the member itself
    before->Int32Int32Member().Insert(10,10);
    before->Int32Int64Member().Insert(20,20);
    before->Int32Int64Member().SetChangedHere(false);

    // change flag only set on dict, not on value
    before->Int64Int32Member().Insert(30,30);
    before->Int64Int32Member().SetChanged(false);
    before->Int64Int32Member().SetChangedHere(true);

    //change flags set all over the place
    before->StringStringMember().Insert(L"asdf",L"adsf");
    before->Int32ObjectMember().Insert(1,Object::Create());


    //On item dict members there are three change flag levels:
    //A: the dictionary, B: the item container, C: the members inside the item.

    // Only C set
    before->Int64ItemMember().Insert(10,TestItem::Create());
    before->Int64ItemMember()[10]->MyInt() = 1;
    before->Int64ItemMember().SetChangedHere(false);
    before->Int64ItemMember()[10].SetChangedHere(false);

    // Only B set
    before->TypeIdItemMember().Insert(10,TestItem::Create());
    before->TypeIdItemMember()[10]->MyInt() = 1;
    before->TypeIdItemMember().SetChanged(false);
    before->TypeIdItemMember()[10].SetChangedHere(true);

    // Only A set
    before->EnumItemMember().Insert(TestEnum::MyFirst,TestItem::Create());
    before->EnumItemMember()[TestEnum::MyFirst]->MyInt() = 1;
    before->EnumItemMember().SetChanged(false);
    before->EnumItemMember().SetChangedHere(true);

    auto checks = [](const MemberDictionariesPtr& obj)
    {
        BOOST_CHECK(obj->IsChanged());
        BOOST_CHECK(obj->Int32Int32Member().IsChanged());
        BOOST_CHECK(obj->Int32Int32Member().IsChangedHere());
        BOOST_CHECK(obj->Int32Int32Member()[10].IsChanged());
        BOOST_CHECK(obj->Int32Int64Member().IsChanged());
        BOOST_CHECK(!obj->Int32Int64Member().IsChangedHere());
        BOOST_CHECK(obj->Int32Int64Member()[20].IsChanged());
        BOOST_CHECK(obj->Int64Int32Member().IsChanged());
        BOOST_CHECK(obj->Int64Int32Member().IsChangedHere());
        BOOST_CHECK(!obj->Int64Int32Member()[30].IsChanged());
        BOOST_CHECK(obj->StringStringMember().IsChanged());
        BOOST_CHECK(obj->Int32ObjectMember().IsChanged());
        BOOST_CHECK(obj->Int32ObjectMember().IsChangedHere());
        BOOST_CHECK(!obj->Int32ObjectMember()[1]->IsChanged());

        BOOST_CHECK(obj->Int64ItemMember().IsChanged());
        BOOST_CHECK(!obj->Int64ItemMember().IsChangedHere());
        BOOST_CHECK(!obj->Int64ItemMember()[10].IsChangedHere());
        BOOST_CHECK(obj->Int64ItemMember()[10]->MyInt().IsChanged());

        BOOST_CHECK(obj->TypeIdItemMember().IsChanged());
        BOOST_CHECK(!obj->TypeIdItemMember().IsChangedHere());
        BOOST_CHECK(obj->TypeIdItemMember()[10].IsChangedHere());
        BOOST_CHECK(!obj->TypeIdItemMember()[10]->MyInt().IsChanged());

        BOOST_CHECK(obj->EnumItemMember().IsChanged());
        BOOST_CHECK(obj->EnumItemMember().IsChangedHere());
        BOOST_CHECK(!obj->EnumItemMember()[TestEnum::MyFirst].IsChangedHere());
        BOOST_CHECK(!obj->EnumItemMember()[TestEnum::MyFirst]->MyInt().IsChanged());
    };

    run_serialization_checks<MemberDictionariesPtr>(before,checks);
}

BOOST_AUTO_TEST_CASE(MergeChanges_Simple)
{
    auto from = MemberTypes::Create();
    auto into = MemberTypes::Create();

    from->Int32Member() = 10;
    from->Int64Member() = 20;
    from->Int64Member().SetChanged(false);

    from->StringMember() = L"asdf";
    from->EnumerationMember() = TestEnum::MyFirst;

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK(into->Int32Member().IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Member(),10);
    BOOST_CHECK(!into->Int64Member().IsChanged());
    BOOST_CHECK(into->Int64Member().IsNull());
    BOOST_CHECK(into->StringMember() == L"asdf");
    BOOST_CHECK_EQUAL(into->EnumerationMember(), TestEnum::MyFirst);
}

BOOST_AUTO_TEST_CASE(MergeChanges_Arrays)
{
    auto from = MemberArrays::Create();
    auto into = MemberArrays::Create();

    from->Int32Member()[0].SetVal(10);
    from->Int64Member()[0].SetVal(20);
    from->Int64Member()[0].SetChanged(false);

    from->StringMember()[0].SetVal(L"asdf");
    from->StringMember()[1].SetVal(L"asdfasdf");
    from->StringMember()[1].SetChanged(false);
    from->EnumerationMember()[0].SetVal(TestEnum::MyFirst);
    from->EnumerationMember()[1].SetVal(TestEnum::MySecond);
    from->EnumerationMember()[1].SetChanged(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK(into->Int32Member()[0].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Member()[0].GetVal(),10);
    BOOST_CHECK(!into->Int64Member().IsChanged());
    BOOST_CHECK(into->Int64Member()[0].IsNull());
    BOOST_CHECK(into->StringMember()[0].GetVal() == L"asdf");
    BOOST_CHECK(into->StringMember()[1].IsNull());
    BOOST_CHECK_EQUAL(into->EnumerationMember()[0].GetVal(), TestEnum::MyFirst);
    BOOST_CHECK(into->EnumerationMember()[1].IsNull());
}


BOOST_AUTO_TEST_CASE(MergeChanges_Objects)
{
    auto from = MemberTypes::Create();
    auto into = MemberTypes::Create();

    from->TestClassMember() = TestItem::Create();
    from->TestClassMember()->MyInt() = 10;
    from->TestClassMember().SetChangedHere(false);
    from->ObjectMember() = TestItem::Create();
    std::static_pointer_cast<TestItem>(from->ObjectMember().GetPtr())->MyInt() = 20;
    from->ObjectMember().SetChangedHere(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK_EQUAL(into->TestClassMember()->MyInt(),10);
    BOOST_CHECK(into->TestClassMember()->MyInt().IsChanged());
    BOOST_CHECK(into->TestClassMember().IsChanged());
    BOOST_CHECK(into->TestClassMember().IsChangedHere());
    BOOST_CHECK_EQUAL(std::dynamic_pointer_cast<TestItem>(into->ObjectMember().GetPtr())->MyInt(),20);
}

BOOST_AUTO_TEST_CASE(MergeChanges_Objects_BothHaveData)
{
    auto from = MemberTypes::Create();

    from->TestClassMember() = TestItem::Create();
    from->ObjectMember() = TestItem::Create();
    from->SetChanged(false);

    auto into = std::static_pointer_cast<MemberTypes>(from->Clone());

    from->TestClassMember()->MyInt() = 10;
    from->TestClassMember().SetChangedHere(false);
    std::static_pointer_cast<TestItem>(from->ObjectMember().GetPtr())->MyInt() = 20;
    from->ObjectMember().SetChangedHere(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK_EQUAL(into->TestClassMember()->MyInt(),10);
    BOOST_CHECK(into->TestClassMember()->MyInt().IsChanged());
    BOOST_CHECK(into->TestClassMember().IsChanged());
    BOOST_CHECK(!into->TestClassMember().IsChangedHere());
    BOOST_CHECK_EQUAL(std::dynamic_pointer_cast<TestItem>(into->ObjectMember().GetPtr())->MyInt(),20);
    BOOST_CHECK(!into->ObjectMember().IsChangedHere());
}


BOOST_AUTO_TEST_CASE(MergeChanges_Sequences)
{
    auto from = MemberSequences::Create();
    auto into = MemberSequences::Create();

    into->Int32Member().push_back(20);
    into->SetChanged(false);

    from->Int32Member().push_back(10);
    from->Int64Member().push_back(20);
    from->Int64Member().SetChanged(false);

    from->StringMember().push_back(L"asdf");

    from->EnumerationMember().push_back(TestEnum::MyFirst);
    from->EnumerationMember().SetChanged(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK(into->Int32Member().IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Member()[0],10);
    BOOST_CHECK(!into->Int64Member().IsChanged());
    BOOST_CHECK(into->Int64Member().empty());
    BOOST_CHECK(into->StringMember()[0] == L"asdf");
    BOOST_CHECK(into->EnumerationMember().empty());
}

BOOST_AUTO_TEST_CASE(MergeChanges_Dictionaries_IntoEmpty)
{
    auto from = MemberDictionaries::Create();
    auto into = MemberDictionaries::Create();

    from->Int32Int32Member().Insert(10,10);
    from->Int32Int32Member().Insert(20,20);

    from->Int32Int64Member().Insert(10,10);
    from->Int32Int64Member().Insert(20,20);
    from->Int32Int64Member()[20].SetChanged(false);

    from->Int64Int64Member().Insert(10,10);
    from->Int64Int64Member().Insert(20,20);
    from->Int64Int64Member().SetChanged(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK(into->Int32Int32Member().IsChanged());
    BOOST_CHECK(into->Int32Int32Member().IsChangedHere());
    BOOST_CHECK(into->Int32Int32Member()[10].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Int32Member()[10].GetVal(),10);
    BOOST_CHECK(into->Int32Int32Member()[20].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Int32Member()[20].GetVal(),20);

    BOOST_CHECK(into->Int32Int64Member().IsChanged());
    BOOST_CHECK(into->Int32Int64Member().IsChangedHere());
    BOOST_CHECK(into->Int32Int64Member()[10].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Int64Member()[10].GetVal(),10);
    BOOST_CHECK(!into->Int32Int64Member()[20].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Int64Member()[20].GetVal(),20);

    BOOST_CHECK(!into->Int64Int32Member().IsChanged());
    BOOST_CHECK_EQUAL(into->Int64Int32Member().size(), 0U);
}

BOOST_AUTO_TEST_CASE(MergeChanges_Dictionaries_IntoEmpty_Error)
{
    auto from = MemberDictionaries::Create();
    auto into = MemberDictionaries::Create();

    from->Int64Int32Member().Insert(10,10);
    from->Int64Int32Member().Insert(20,20);
    from->Int64Int32Member().SetChangedHere(false);

    BOOST_CHECK_THROW(ts::Utilities::MergeChanges(into,from), ts::SoftwareViolationException);
}

BOOST_AUTO_TEST_CASE(MergeChanges_Dictionaries_IntoNonEmpty_1)
{
    auto into = MemberDictionaries::Create();
    into->Int32Int32Member().Insert(1,10);
    into->Int32Int32Member().Insert(2,20);

    into->Int32Int64Member().Insert(1,10);
    into->Int32Int64Member().Insert(2,20);

    into->Int64Int32Member().Insert(1,10);
    into->Int64Int32Member().Insert(2,20);

    into->Int64Int64Member().Insert(1,10);
    into->Int64Int64Member().Insert(2,20);

    into->SetChanged(false);

    auto from = MemberDictionaries::Create();

    from->Int32Int32Member().Insert(3,30);

    from->Int32Int64Member().Insert(1,100);
    from->Int32Int64Member().Insert(2,20);
    from->Int32Int64Member()[2].SetChanged(false);
    from->Int32Int64Member().SetChangedHere(false);

    from->Int64Int32Member().Insert(1,100);
    from->Int64Int32Member().Insert(2,555);
    from->Int64Int32Member()[2].SetChanged(false);
    from->Int64Int32Member().SetChangedHere(false);

    from->Int64Int64Member().Insert(1,100);
    from->Int64Int64Member().SetChangedHere(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK(into->Int32Int32Member().IsChanged());
    BOOST_CHECK(into->Int32Int32Member().IsChangedHere());
    BOOST_CHECK_EQUAL(into->Int32Int32Member().size(), 1U);
    BOOST_CHECK(into->Int32Int32Member()[3].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Int32Member()[3].GetVal(),30);

    BOOST_CHECK(into->Int32Int64Member().IsChanged());
    BOOST_CHECK(!into->Int32Int64Member().IsChangedHere());
    BOOST_CHECK_EQUAL(into->Int32Int64Member().size(), 2U);
    BOOST_CHECK(into->Int32Int64Member()[1].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Int64Member()[1].GetVal(),100);
    BOOST_CHECK(!into->Int32Int64Member()[2].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Int64Member()[2].GetVal(),20);

    BOOST_CHECK(into->Int64Int32Member().IsChanged());
    BOOST_CHECK(!into->Int64Int32Member().IsChangedHere());
    BOOST_CHECK_EQUAL(into->Int64Int32Member().size(), 2U);
    BOOST_CHECK(into->Int64Int32Member()[1].IsChanged());
    BOOST_CHECK_EQUAL(into->Int64Int32Member()[1].GetVal(),100);
    BOOST_CHECK(!into->Int64Int32Member()[2].IsChanged());
    BOOST_CHECK_EQUAL(into->Int64Int32Member()[2].GetVal(),20);

    BOOST_CHECK(into->Int64Int64Member().IsChanged());
    BOOST_CHECK(!into->Int64Int64Member().IsChangedHere());
    BOOST_CHECK_EQUAL(into->Int64Int64Member().size(), 2U);
    BOOST_CHECK(into->Int64Int64Member()[1].IsChanged());
    BOOST_CHECK_EQUAL(into->Int64Int64Member()[1].GetVal(),100);
    BOOST_CHECK(!into->Int64Int64Member()[2].IsChanged());
    BOOST_CHECK_EQUAL(into->Int64Int64Member()[2].GetVal(),20);

}


BOOST_AUTO_TEST_CASE(MergeChanges_Dictionaries_IntoNonEmpty_2)
{
    auto into = MemberDictionaries::Create();
    into->Int32Int32Member().Insert(1,10);
    into->Int32Int32Member().Insert(2,20);

    into->Int32Int64Member().Insert(1,10);
    into->Int32Int64Member().Insert(2,20);

    into->Int64Int32Member().Insert(1,10);
    into->Int64Int32Member().Insert(2,20);

    into->Int64Int64Member().Insert(1,10);
    into->Int64Int64Member().Insert(2,20);

    into->SetChanged(false);

    auto from = MemberDictionaries::Create();

    from->Int32Int32Member().Insert(1,10);
    from->Int32Int32Member()[1].SetChanged(false);

    from->Int32Int64Member().Insert(1,100);

    from->Int64Int32Member().Insert(1,100);
    from->Int64Int32Member()[1].SetChanged(false);

    from->Int64Int64Member().Insert(1,100);
    from->Int64Int64Member().SetChanged(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK(into->Int32Int32Member().IsChanged());
    BOOST_CHECK(into->Int32Int32Member().IsChangedHere());
    BOOST_CHECK_EQUAL(into->Int32Int32Member().size(), 1U);
    BOOST_CHECK(!into->Int32Int32Member()[1].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Int32Member()[1].GetVal(),10);

    BOOST_CHECK(into->Int32Int64Member().IsChanged());
    BOOST_CHECK(into->Int32Int64Member().IsChangedHere());
    BOOST_CHECK_EQUAL(into->Int32Int64Member().size(), 1U);
    BOOST_CHECK(into->Int32Int64Member()[1].IsChanged());
    BOOST_CHECK_EQUAL(into->Int32Int64Member()[1].GetVal(),100);

    BOOST_CHECK(into->Int64Int32Member().IsChanged());
    BOOST_CHECK(into->Int64Int32Member().IsChangedHere());
    BOOST_CHECK_EQUAL(into->Int64Int32Member().size(), 1U);
    BOOST_CHECK(!into->Int64Int32Member()[1].IsChanged());
    BOOST_CHECK_EQUAL(into->Int64Int32Member()[1].GetVal(),100);

    BOOST_CHECK(!into->Int64Int64Member().IsChanged());
    BOOST_CHECK_EQUAL(into->Int64Int64Member().size(), 2U);
    BOOST_CHECK_EQUAL(into->Int64Int64Member()[1].GetVal(),10);
    BOOST_CHECK_EQUAL(into->Int64Int64Member()[2].GetVal(),20);
}



BOOST_AUTO_TEST_CASE(MergeChanges_Dictionaries_IntoNonEmpty_Error)
{
    auto into = MemberDictionaries::Create();
    into->Int32Int32Member().Insert(1,10);
    into->Int32Int32Member().Insert(2,20);
    into->SetChanged(false);

    auto from = MemberDictionaries::Create();

    from->Int32Int32Member().Insert(3,30);
    from->Int32Int32Member().SetChangedHere(false);

    BOOST_CHECK_THROW(ts::Utilities::MergeChanges(into,from), ts::SoftwareViolationException);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoEmpty_1)
{
    auto from = MemberDictionaries::Create();
    auto into = MemberDictionaries::Create();

    //empty + x[x1:(xM=10)] => x[x1:(xM=10)]
    from->Int64ItemMember().Insert(1,TestItem::Create());
    from->Int64ItemMember().at(1)->MyInt() = 10;

    //empty + x[1:(xM=10)] => x[1:(xM=10)]
    from->TypeIdItemMember().Insert(1,TestItem::Create());
    from->TypeIdItemMember().at(1)->MyInt() = 20;
    from->TypeIdItemMember().at(1).SetChangedHere(false);

    //empty + x[x1:(M=10)] => x[x1:(M=10)]
    from->EnumItemMember().Insert(TestEnum::MySecond,TestItem::Create());
    from->EnumItemMember().at(TestEnum::MySecond)->MyInt() = 30;
    from->EnumItemMember().at(TestEnum::MySecond).SetChanged(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK(into->Int64ItemMember().IsChangedHere());
    BOOST_CHECK(into->Int64ItemMember().at(1).IsChangedHere());
    BOOST_CHECK(into->Int64ItemMember().at(1)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->Int64ItemMember().at(1)->MyInt(),10);

    BOOST_CHECK(into->TypeIdItemMember().IsChangedHere());
    BOOST_CHECK(!into->TypeIdItemMember().at(1).IsChangedHere());
    BOOST_CHECK(into->TypeIdItemMember().at(1)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(1)->MyInt(),20);

    BOOST_CHECK(into->EnumItemMember().IsChangedHere());
    BOOST_CHECK(!into->EnumItemMember().at(TestEnum::MySecond).IsChangedHere());
    BOOST_CHECK(!into->EnumItemMember().at(TestEnum::MySecond)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->EnumItemMember().at(TestEnum::MySecond)->MyInt(),30);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoEmpty_Error_1)
{
    auto from = MemberDictionaries::Create();
    auto into = MemberDictionaries::Create();

    //empty + [x1:(xM=10)] => error
    from->Int64ItemMember().Insert(1,TestItem::Create());
    from->Int64ItemMember().at(1)->MyInt() = 10;
    from->Int64ItemMember().SetChangedHere(false);

    BOOST_CHECK_THROW(ts::Utilities::MergeChanges(into,from),
                      ts::SoftwareViolationException);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoEmpty_Error_2)
{
    auto from = MemberDictionaries::Create();
    auto into = MemberDictionaries::Create();

    //empty + [x1:(M=10)] => error
    from->Int64ItemMember().Insert(1,TestItem::Create());
    from->Int64ItemMember().at(1)->MyInt() = 10;
    from->Int64ItemMember().SetChangedHere(false);
    from->Int64ItemMember().at(1)->MyInt().SetChanged(false);

    BOOST_CHECK_THROW(ts::Utilities::MergeChanges(into,from),
                      ts::SoftwareViolationException);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoEmpty_Error_3)
{
    auto from = MemberDictionaries::Create();
    auto into = MemberDictionaries::Create();

    //empty + [1:(xM=10)] => error
    from->Int64ItemMember().Insert(1,TestItem::Create());
    from->Int64ItemMember().at(1)->MyInt() = 10;
    from->Int64ItemMember().SetChangedHere(false);
    from->Int64ItemMember().at(1).SetChangedHere(false);

    BOOST_CHECK_THROW(ts::Utilities::MergeChanges(into,from),
                      ts::SoftwareViolationException);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoEmpty_2)
{
    auto from = MemberDictionaries::Create();
    auto into = MemberDictionaries::Create();

    //empty + [1:(M=10)] => empty
    from->Int64ItemMember().Insert(1,TestItem::Create());
    from->Int64ItemMember().at(1)->MyInt() = 10;
    from->Int64ItemMember().SetChanged(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK(!into->Int64ItemMember().IsChangedHere());
    BOOST_CHECK_EQUAL(into->Int64ItemMember().size(), 0U);
}

struct NonEmptyObjectDictionariesFixture
{
    MemberDictionariesPtr into;
    MemberDictionariesPtr from;

    NonEmptyObjectDictionariesFixture()
        : into(MemberDictionaries::Create())
        , from(MemberDictionaries::Create())
    {
        into->Int64ItemMember().Insert(1,TestItem::Create());
        into->Int64ItemMember().at(1)->MyInt() = 10;
        into->Int64ItemMember().at(1)->MyString() = L"one";
        into->Int64ItemMember().Insert(2,TestItem::Create());
        into->Int64ItemMember().at(2)->MyInt() = 20;
        into->Int64ItemMember().at(2)->MyString() = L"two";

        into->TypeIdItemMember().Insert(1,TestItem::Create());
        into->TypeIdItemMember().at(1)->MyInt() = 10;
        into->TypeIdItemMember().at(1)->MyString() = L"one";
        into->TypeIdItemMember().Insert(2,TestItem::Create());
        into->TypeIdItemMember().at(2)->MyInt() = 20;
        into->TypeIdItemMember().at(2)->MyString() = L"two";

        into->EnumItemMember().Insert(TestEnum::MyFirst,TestItem::Create());
        into->EnumItemMember().at(TestEnum::MyFirst)->MyInt() = 10;
        into->EnumItemMember().at(TestEnum::MyFirst)->MyString() = L"one";
        into->EnumItemMember().Insert(TestEnum::MySecond,TestItem::Create());
        into->EnumItemMember().at(TestEnum::MySecond)->MyInt() = 20;
        into->EnumItemMember().at(TestEnum::MySecond)->MyString() = L"two";

        into->SetChanged(false);
    }
};

BOOST_FIXTURE_TEST_SUITE( s, NonEmptyObjectDictionariesFixture )

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoNonEmpty_1)
{
    //[1:(M=10,S=one),2:(M=20,S=two)] + x[x1:(xM=100)] => x[x1:(xM=100)]
    from->Int64ItemMember().Insert(1,TestItem::Create());
    from->Int64ItemMember().at(1)->MyInt() = 100;

    //[1:(M=10,S=one),2:(M=20,S=two)] + x[ 1:(xM=200)] => x[ 1:(xM=200)]
    from->TypeIdItemMember().Insert(1,TestItem::Create());
    from->TypeIdItemMember().at(1)->MyInt() = 200;
    from->TypeIdItemMember().at(1).SetChangedHere(false);

    //[1:(M=10,S=one),2:(M=20,S=two)] + x[ 1:( M=300)] => x[ 1:( M=300)]
    from->EnumItemMember().Insert(TestEnum::MySecond,TestItem::Create());
    from->EnumItemMember().at(TestEnum::MySecond)->MyInt() = 300;
    from->EnumItemMember().at(TestEnum::MySecond).SetChanged(false);
    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK_EQUAL(into->Int64ItemMember().size(), 1U);
    BOOST_CHECK(into->Int64ItemMember().IsChangedHere());
    BOOST_CHECK(into->Int64ItemMember().at(1).IsChangedHere());
    BOOST_CHECK(into->Int64ItemMember().at(1)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->Int64ItemMember().at(1)->MyInt(),100);
    BOOST_CHECK(into->Int64ItemMember().at(1)->MyString().IsNull());

    BOOST_CHECK_EQUAL(into->TypeIdItemMember().size(), 1U);
    BOOST_CHECK(into->TypeIdItemMember().IsChangedHere());
    BOOST_CHECK(!into->TypeIdItemMember().at(1).IsChangedHere());
    BOOST_CHECK(into->TypeIdItemMember().at(1)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(1)->MyInt(),200);
    BOOST_CHECK(into->TypeIdItemMember().at(1)->MyString().IsNull());

    BOOST_CHECK_EQUAL(into->EnumItemMember().size(), 1U);
    BOOST_CHECK(into->EnumItemMember().IsChangedHere());
    BOOST_CHECK(!into->EnumItemMember().at(TestEnum::MySecond).IsChangedHere());
    BOOST_CHECK(!into->EnumItemMember().at(TestEnum::MySecond)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->EnumItemMember().at(TestEnum::MySecond)->MyInt(),300);
    BOOST_CHECK(into->EnumItemMember().at(TestEnum::MySecond)->MyString().IsNull());
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoNonEmpty_2)
{
    //[1:(M=10,S=one),2:(M=20,S=two)] + x[ 3:( M=300)] => x[ 3:( M=300)]
    from->Int64ItemMember().Insert(3,TestItem::Create());
    from->Int64ItemMember().at(3)->MyInt() = 300;
    from->Int64ItemMember().at(3).SetChanged(false);

    //[1:(M=10,S=one),2:(M=20,S=two)] +  [x2:(xM=200)] =>  [ 1:( M=10, S=one),x2:(xM:200)]
    from->TypeIdItemMember().Insert(2,TestItem::Create());
    from->TypeIdItemMember().at(2)->MyInt() = 200;
    from->TypeIdItemMember().SetChangedHere(false);

    //[1:(M=10,S=one),2:(M=20,S=two)] +  [ 2:(xM=300)] =>  [ 1:( M=10, S=one), 2:(xM:300, S=two)]
    from->EnumItemMember().Insert(TestEnum::MySecond,TestItem::Create());
    from->EnumItemMember().at(TestEnum::MySecond)->MyInt() = 300;
    from->EnumItemMember().SetChangedHere(false);
    from->EnumItemMember().at(TestEnum::MySecond).SetChangedHere(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK_EQUAL(into->Int64ItemMember().size(), 1U);
    BOOST_CHECK(into->Int64ItemMember().IsChangedHere());
    BOOST_CHECK(!into->Int64ItemMember().at(3).IsChangedHere());
    BOOST_CHECK(!into->Int64ItemMember().at(3)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->Int64ItemMember().at(3)->MyInt(),300);
    BOOST_CHECK(into->Int64ItemMember().at(3)->MyString().IsNull());

    BOOST_CHECK_EQUAL(into->TypeIdItemMember().size(), 2U);
    BOOST_CHECK(!into->TypeIdItemMember().IsChangedHere());
    BOOST_CHECK(!into->TypeIdItemMember().at(1).IsChanged());
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(1)->MyInt(),10);
    BOOST_CHECK(into->TypeIdItemMember().at(1)->MyString() == L"one");
    BOOST_CHECK(into->TypeIdItemMember().at(2).IsChangedHere());
    BOOST_CHECK(into->TypeIdItemMember().at(2)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(2)->MyInt(),200);
    BOOST_CHECK(!into->TypeIdItemMember().at(2)->MyString().IsChanged());
    BOOST_CHECK(into->TypeIdItemMember().at(2)->MyString().IsNull());

    BOOST_CHECK_EQUAL(into->EnumItemMember().size(), 2U);
    BOOST_CHECK(!into->EnumItemMember().IsChangedHere());
    BOOST_CHECK(!into->EnumItemMember().at(TestEnum::MyFirst).IsChanged());
    BOOST_CHECK_EQUAL(into->EnumItemMember().at(TestEnum::MyFirst)->MyInt(),10);
    BOOST_CHECK(into->EnumItemMember().at(TestEnum::MyFirst)->MyString() == L"one");
    BOOST_CHECK(!into->EnumItemMember().at(TestEnum::MySecond).IsChangedHere());
    BOOST_CHECK(into->EnumItemMember().at(TestEnum::MySecond)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->EnumItemMember().at(TestEnum::MySecond)->MyInt(),300);
    BOOST_CHECK(into->EnumItemMember().at(TestEnum::MySecond)->MyString() == L"two");
    BOOST_CHECK(!into->EnumItemMember().at(TestEnum::MySecond)->MyString().IsChanged());
}


BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoNonEmpty_3)
{
    //[1:(M=10,S=one),2:(M=20,S=two)] +  [x2:( M=200)] =>  [ 1:( M=10, S=one),x2:( M:200)]
    from->Int64ItemMember().Insert(2,TestItem::Create());
    from->Int64ItemMember().at(2)->MyInt() = 200;
    from->Int64ItemMember().SetChangedHere(false);
    from->Int64ItemMember().at(2)->MyInt().SetChanged(false);

    //[1:(M=10,S=one),2:(M=20,S=two)] +  [ 3:( M=300)] =>  [ 1:( M=10, S=one), 2:( M:20, S=two)]
    from->TypeIdItemMember().Insert(3,TestItem::Create());
    from->TypeIdItemMember().at(3)->MyInt() = 300;
    from->TypeIdItemMember().SetChanged(false);

    //[1:(M=10,S=one),2:(M=20,S=two)] +  [ 2:( M=200)] =>  [ 1:( M=10, S=one), 2:( M:20, S=two)]
    from->EnumItemMember().Insert(TestEnum::MySecond,TestItem::Create());
    from->EnumItemMember().at(TestEnum::MySecond)->MyInt() = 200;
    from->EnumItemMember().SetChanged(false);

    ts::Utilities::MergeChanges(into,from);
    BOOST_CHECK_EQUAL(into->Int64ItemMember().size(), 2U);
    BOOST_CHECK(!into->Int64ItemMember().IsChangedHere());
    BOOST_CHECK(!into->Int64ItemMember().at(1).IsChanged());
    BOOST_CHECK_EQUAL(into->Int64ItemMember().at(1)->MyInt(),10);
    BOOST_CHECK_EQUAL(into->Int64ItemMember().at(1)->MyString(),L"one");
    BOOST_CHECK(into->Int64ItemMember().at(2).IsChangedHere());
    BOOST_CHECK(!into->Int64ItemMember().at(2)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->Int64ItemMember().at(2)->MyInt(),200);
    BOOST_CHECK(into->Int64ItemMember().at(2)->MyString().IsNull());

    BOOST_CHECK_EQUAL(into->TypeIdItemMember().size(), 2U);
    BOOST_CHECK(!into->TypeIdItemMember().IsChanged());
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(1)->MyInt(),10);
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(1)->MyString(),L"one");
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(2)->MyInt(),20);
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(2)->MyString(),L"two");

    BOOST_CHECK_EQUAL(into->EnumItemMember().size(), 2U);
    BOOST_CHECK(!into->EnumItemMember().IsChanged());
    BOOST_CHECK_EQUAL(into->EnumItemMember().at(TestEnum::MyFirst)->MyInt(),10);
    BOOST_CHECK_EQUAL(into->EnumItemMember().at(TestEnum::MyFirst)->MyString(),L"one");
    BOOST_CHECK_EQUAL(into->EnumItemMember().at(TestEnum::MySecond)->MyInt(),20);
    BOOST_CHECK_EQUAL(into->EnumItemMember().at(TestEnum::MySecond)->MyString(),L"two");
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoNonEmpty_Error_1)
{
    //[1:(M=10,S=one),2:(M=20,S=two)] +  [x3:( M=300)] =>  error
    from->Int64ItemMember().Insert(3,TestItem::Create());
    from->Int64ItemMember().at(3)->MyInt() = 300;
    from->Int64ItemMember().SetChanged(false);
    from->Int64ItemMember().at(3).SetChangedHere(true);

    BOOST_CHECK_THROW(ts::Utilities::MergeChanges(into,from),
                      ts::SoftwareViolationException);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_IntoNonEmpty_Error_2)
{
    //[1:(M=10,S=one),2:(M=20,S=two)] +  [ 3:(xM=300)] =>  error
    from->Int64ItemMember().Insert(3,TestItem::Create());
    from->Int64ItemMember().at(3)->MyInt() = 300;
    from->Int64ItemMember().SetChanged(false);
    from->Int64ItemMember().at(3)->MyInt().SetChanged(true);

    BOOST_CHECK_THROW(ts::Utilities::MergeChanges(into,from),
                      ts::SoftwareViolationException);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectDictionaries_EmptyIntoNonEmpty)
{
    //[1:(M=10,S=one),2:(M=20,S=two)] +  x[] =>  x[]
    from->Int64ItemMember().SetChangedHere(true);

    //[1:(M=10,S=one),2:(M=20,S=two)] +  [] =>  [1:(M=10,S=one),2:(M:20,S=two)]
    //nothing to do...

    ts::Utilities::MergeChanges(into,from);
    BOOST_CHECK_EQUAL(into->Int64ItemMember().size(), 0U);
    BOOST_CHECK(into->Int64ItemMember().IsChangedHere());

    BOOST_CHECK_EQUAL(into->TypeIdItemMember().size(), 2U);
    BOOST_CHECK(!into->TypeIdItemMember().IsChanged());
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(1)->MyInt(),10);
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(1)->MyString(),L"one");
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(2)->MyInt(),20);
    BOOST_CHECK_EQUAL(into->TypeIdItemMember().at(2)->MyString(),L"two");
}

BOOST_AUTO_TEST_SUITE_END()

struct EmptyObjectSequenceFixture
{
    EmptyObjectSequenceFixture()
        : from(MemberSequences::Create())
        , into(MemberSequences::Create())
    {

    }
    MemberSequencesPtr from;
    MemberSequencesPtr into;
};

BOOST_FIXTURE_TEST_SUITE( seq, EmptyObjectSequenceFixture)

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectSequences_1)
{
    //[] + x{(xM=10)} => x{(xM=10)}
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().at(0)->MyInt() = 10;

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK_EQUAL(into->TestClassMember().size(), 1U);
    BOOST_CHECK(into->TestClassMember().IsChangedHere());
    BOOST_CHECK(into->TestClassMember().at(0)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->TestClassMember().at(0)->MyInt(),10);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectSequences_2)
{
    //[] + {(xM=10)} => ERROR
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().at(0)->MyInt() = 10;
    from->TestClassMember().SetChangedHere(false);

    BOOST_CHECK_THROW(ts::Utilities::MergeChanges(into,from),
                      ts::SoftwareViolationException);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectSequences_3)
{
    //[] + {( M=10)} => []
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().at(0)->MyInt() = 10;
    from->TestClassMember().SetChanged(false);

    ts::Utilities::MergeChanges(into,from);
    BOOST_CHECK_EQUAL(into->TestClassMember().size(), 0U);
    BOOST_CHECK(!into->TestClassMember().IsChanged());
}


BOOST_AUTO_TEST_CASE(MergeChanges_ObjectSequences_4)
{
    //[] + x[] => x[]
    from->TestClassMember().SetChanged(true);

    ts::Utilities::MergeChanges(into,from);
    BOOST_CHECK_EQUAL(into->TestClassMember().size(), 0U);
    BOOST_CHECK(into->TestClassMember().IsChanged());
}

BOOST_AUTO_TEST_SUITE_END()

struct NonEmptyObjectSequenceFixture
{
    NonEmptyObjectSequenceFixture()
        : from(MemberSequences::Create())
        , into(MemberSequences::Create())
    {
        into->TestClassMember().push_back(TestItem::Create());
        into->TestClassMember().at(0)->MyInt() = 10;
        into->TestClassMember().at(0)->MyString() = L"one";
        into->TestClassMember().push_back(TestItem::Create());
        into->TestClassMember().at(1)->MyInt() = 20;
        into->TestClassMember().at(1)->MyString() = L"two";
        into->SetChanged(false);
    }
    MemberSequencesPtr from;
    MemberSequencesPtr into;
};

BOOST_FIXTURE_TEST_SUITE( seq2, NonEmptyObjectSequenceFixture)

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectSequences_IntoNonEmpty_1)
{
    //{(M=10,S=one),(M=20,S=two)} + x{(M=30)} => x{(M=30)}
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().at(0)->MyInt() = 30;
    from->TestClassMember().at(0)->MyInt().SetChanged(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK_EQUAL(into->TestClassMember().size(), 1U);
    BOOST_CHECK(into->TestClassMember().IsChangedHere());
    BOOST_CHECK(!into->TestClassMember().at(0)->MyInt().IsChanged());
    BOOST_CHECK_EQUAL(into->TestClassMember().at(0)->MyInt(),30);
}


BOOST_AUTO_TEST_CASE(MergeChanges_ObjectSequences_IntoNonEmpty_2)
{
    //{(M=10,S=one),(M=20,S=two)} + {(M=30)} => {(M=10,S=one),(M=20,S=two)}
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().at(0)->MyInt() = 30;
    from->TestClassMember().SetChanged(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK_EQUAL(into->TestClassMember().size(), 2U);
    BOOST_CHECK(!into->TestClassMember().IsChanged());
    BOOST_CHECK_EQUAL(into->TestClassMember().at(0)->MyInt(),10);
    BOOST_CHECK_EQUAL(into->TestClassMember().at(0)->MyString(),L"one");
    BOOST_CHECK_EQUAL(into->TestClassMember().at(1)->MyInt(),20);
    BOOST_CHECK_EQUAL(into->TestClassMember().at(1)->MyString(),L"two");
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectSequences_IntoNonEmpty_3)
{
    //{(M=10,S=one),(M=20,S=two)} + {(xM=30)} => error??
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().at(0)->MyInt() = 30;
    from->TestClassMember().SetChangedHere(false);

    BOOST_CHECK_THROW(ts::Utilities::MergeChanges(into,from),
                      ts::SoftwareViolationException);
}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectSequences_IntoNonEmpty_4)
{
    //{(M=10,S=one),(M=20,S=two)} + {(xM=30),()} => {(xM=30,S=one),(M=20,S=two)}
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().at(0)->MyInt() = 30;
    from->TestClassMember().SetChangedHere(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK_EQUAL(into->TestClassMember().size(), 2U);
    BOOST_CHECK(!into->TestClassMember().IsChangedHere());
    BOOST_CHECK(!into->TestClassMember().at(1)->IsChanged());
    BOOST_CHECK(into->TestClassMember().at(0)->MyInt().IsChanged());
    BOOST_CHECK(!into->TestClassMember().at(0)->MyString().IsChanged());
    BOOST_CHECK_EQUAL(into->TestClassMember().at(0)->MyInt(),30);
    BOOST_CHECK_EQUAL(into->TestClassMember().at(0)->MyString(),L"one");
    BOOST_CHECK_EQUAL(into->TestClassMember().at(1)->MyInt(),20);
    BOOST_CHECK_EQUAL(into->TestClassMember().at(1)->MyString(),L"two");

}

BOOST_AUTO_TEST_CASE(MergeChanges_ObjectSequences_IntoNonEmpty_5)
{
    //{(M=10,S=one),(M=20,S=two)} + {(xM=30),(S=blahonga)} => {(xM=30,S=one),(M=20,S=two)}
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().push_back(TestItem::Create());
    from->TestClassMember().SetChangedHere(false);
    from->TestClassMember().at(0)->MyInt() = 30;
    from->TestClassMember().at(1)->MyString() = L"blahonga";
    from->TestClassMember().at(1)->MyString().SetChanged(false);

    ts::Utilities::MergeChanges(into,from);

    BOOST_CHECK_EQUAL(into->TestClassMember().size(), 2U);
    BOOST_CHECK(!into->TestClassMember().IsChangedHere());
    BOOST_CHECK(!into->TestClassMember().at(1)->IsChanged());
    BOOST_CHECK(into->TestClassMember().at(0)->MyInt().IsChanged());
    BOOST_CHECK(!into->TestClassMember().at(0)->MyString().IsChanged());
    BOOST_CHECK_EQUAL(into->TestClassMember().at(0)->MyInt(),30);
    BOOST_CHECK_EQUAL(into->TestClassMember().at(0)->MyString(),L"one");
    BOOST_CHECK_EQUAL(into->TestClassMember().at(1)->MyInt(),20);
    BOOST_CHECK_EQUAL(into->TestClassMember().at(1)->MyString(),L"two");
}



BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(EnumerationSequenceReflection)
{
    MemberSequencesPtr seq = MemberSequences::Create();
    seq->EnumerationMember().push_back(DotsTest::TestEnum::MyFirst);
    seq->EnumerationMember().push_back(DotsTest::TestEnum::MySecond);

    {
        EnumerationSequenceContainerBase& base = seq->EnumerationMember();
        BOOST_CHECK_EQUAL(base.size(), 2U);
        BOOST_CHECK_EQUAL(base.GetOrdinal(0), DotsTest::TestEnum::MyFirst);
        BOOST_CHECK_EQUAL(base.GetOrdinal(1), DotsTest::TestEnum::MySecond);

        base.SetOrdinal(0, 1);
        base.PushBackOrdinal(0);
    }

    BOOST_CHECK_EQUAL(seq->EnumerationMember().size(), 3U);
    BOOST_CHECK_EQUAL(seq->EnumerationMember()[0], DotsTest::TestEnum::MySecond);
    BOOST_CHECK_EQUAL(seq->EnumerationMember().GetVal(1), DotsTest::TestEnum::MySecond);
    BOOST_CHECK_EQUAL(seq->EnumerationMember()[2], DotsTest::TestEnum::MyFirst);

    {
        EnumerationSequenceContainerBase& base = seq->EnumerationMember();
        base.clear();
        BOOST_CHECK_EQUAL(seq->EnumerationMember().size(), 0U);
    }
}

BOOST_AUTO_TEST_CASE(ObjectSequenceReflection)
{
    MemberSequencesPtr seq = MemberSequences::Create();

    seq->TestClassMember().push_back(TestItem::Create());
    seq->TestClassMember().at(0)->MyInt() = 10;
    seq->TestClassMember().push_back(TestItem::Create());
    seq->TestClassMember().at(1)->MyInt() = 20;

    {
        GenericObjectSequenceContainerBase& base = seq->TestClassMember();
        BOOST_CHECK_EQUAL(base.size(), 2U);
        BOOST_CHECK_EQUAL(dynamic_cast<Int32Container&>(base.GetObjectPointer(0)->
                                                        GetMember(TestItem::MyIntMemberIndex(),0)).GetVal(), 10);
        BOOST_CHECK_EQUAL(dynamic_cast<Int32Container&>(base.GetObjectPointer(1)->
                                                        GetMember(TestItem::MyIntMemberIndex(),0)).GetVal(), 20);
        dynamic_cast<Int32Container&>(base.GetObjectPointer(1)->
                                      GetMember(TestItem::MyIntMemberIndex(),0)).SetVal(30);

        auto item1 = TestItem::Create();
        auto item3 = TestItem::Create();
        item1->MyInt() = 400;
        item3->MyInt() = 500;
        base.PushBackObjectPointer(item3);
        base.SetObjectPointer(0, item1);
    }

    {
        const GenericObjectSequenceContainerBase& base = seq->TestClassMember();
        BOOST_CHECK_EQUAL(base.size(), 3U);
        BOOST_CHECK_EQUAL(dynamic_cast<const Int32Container&>(base.GetObjectPointer(0)->
                                                              GetMember(TestItem::MyIntMemberIndex(),0)).GetVal(), 400);
        BOOST_CHECK_EQUAL(dynamic_cast<const Int32Container&>(base.GetObjectPointer(1)->
                                                              GetMember(TestItem::MyIntMemberIndex(),0)).GetVal(), 30);
        BOOST_CHECK_EQUAL(dynamic_cast<const Int32Container&>(base.GetObjectPointer(2)->
                                                              GetMember(TestItem::MyIntMemberIndex(),0)).GetVal(), 500);
    }
}


#ifndef SAFIR_NO_DEPRECATED
BOOST_AUTO_TEST_CASE(ObjectSequenceReflectionDeprecated)
{
    MemberSequencesPtr seq = MemberSequences::Create();

    seq->TestClassMember().push_back(TestItem::Create());
    seq->TestClassMember().at(0)->MyInt() = 10;
    seq->TestClassMember().push_back(TestItem::Create());
    seq->TestClassMember().at(1)->MyInt() = 20;
    seq->TestClassMember().SetVal(1,TestItem::Create());
    seq->TestClassMember().at(1)->MyInt() = 40;
    {
        GenericObjectSequenceContainerBase& base = seq->TestClassMember();
        BOOST_CHECK_EQUAL(base.size(), 2U);
        BOOST_CHECK_EQUAL(dynamic_cast<Int32Container&>(base.GetObj(0)->
                                                        GetMember(TestItem::MyIntMemberIndex(),0)).GetVal(), 10);
        BOOST_CHECK_EQUAL(dynamic_cast<Int32Container&>(base.GetObj(1)->
                                                        GetMember(TestItem::MyIntMemberIndex(),0)).GetVal(), 40);
        dynamic_cast<Int32Container&>(base.GetObj(1)->
                                      GetMember(TestItem::MyIntMemberIndex(),0)).SetVal(30);
    }

    {
        const GenericObjectSequenceContainerBase& base = seq->TestClassMember();
        BOOST_CHECK_EQUAL(base.size(), 2U);
        BOOST_CHECK_EQUAL(dynamic_cast<const Int32Container&>(base.GetObj(0)->
                                                              GetMember(TestItem::MyIntMemberIndex(),0)).GetVal(), 10);
        BOOST_CHECK_EQUAL(dynamic_cast<const Int32Container&>(base.GetObj(1)->
                                                              GetMember(TestItem::MyIntMemberIndex(),0)).GetVal(), 30);
    }
}
#endif

BOOST_AUTO_TEST_CASE(DictionaryReflection)
{
    MemberDictionariesPtr dict = MemberDictionaries::Create();
    dict->Int64ItemMember().Insert(1,TestItem::Create());
    dict->Int64ItemMember().at(1)->MyInt() = 10;
    dict->Int64ItemMember().at(1)->MyString() = L"one";
    dict->Int64ItemMember().Insert(2,TestItem::Create());
    dict->Int64ItemMember().at(2)->MyInt() = 20;
    dict->Int64ItemMember().at(2)->MyString() = L"two";

    dict->EnumItemMember().Insert(TestEnum::MyFirst,TestItem::Create());
    dict->EnumItemMember().at(TestEnum::MyFirst)->MyInt() = 10;
    dict->EnumItemMember().at(TestEnum::MyFirst)->MyString() = L"one";
    dict->EnumItemMember().Insert(TestEnum::MySecond,TestItem::Create());
    dict->EnumItemMember().at(TestEnum::MySecond)->MyInt() = 20;
    dict->EnumItemMember().at(TestEnum::MySecond)->MyString() = L"two";

    dict->Int64StringMember().Insert(1, L"foo");
    dict->Int64StringMember().Insert(20, L"bar");
    dict->Int64StringMember().Insert(111, L"bart");
    dict->Int64StringMember().at(111).SetNull();

    {
        DictionaryContainerBase& base = dict->Int64ItemMember();
        BOOST_CHECK_EQUAL(base.size(), 2U);
        BOOST_CHECK_EQUAL(base.GetKeyAt<Int64>(0), 1);
        BOOST_CHECK_EQUAL(base.GetKeyAt<Int64>(1), 2);
        auto* container = dynamic_cast<TestItemContainer*>(&base.GetValueContainerAt(0));
        BOOST_CHECK_NE(container, nullptr);
        BOOST_CHECK_EQUAL(container->IsNull(), false);
        BOOST_CHECK_EQUAL(container->GetPtr()->MyInt(), 10);
        BOOST_CHECK_EQUAL(container->GetPtr()->MyString(), L"one");
        container = dynamic_cast<TestItemContainer*>(&base.GetValueContainerAt(1));
        BOOST_CHECK_NE(container, nullptr);
        BOOST_CHECK_EQUAL(container->IsNull(), false);
        BOOST_CHECK_EQUAL(container->GetPtr()->MyInt(), 20);
        BOOST_CHECK_EQUAL(container->GetPtr()->MyString(), L"two");

    }

    {
        DictionaryContainerBase& base = dict->EnumItemMember();
        BOOST_CHECK_EQUAL(base.size(), 2U);
        BOOST_CHECK_EQUAL(base.GetKeyAt<Int32>(0), 0);
        BOOST_CHECK_EQUAL(base.GetKeyAt<Int32>(1), 1);
        auto* container = dynamic_cast<TestItemContainer*>(&base.GetValueContainerAt(0));
        BOOST_CHECK_NE(container, nullptr);
        BOOST_CHECK_EQUAL(container->IsNull(), false);
        BOOST_CHECK_EQUAL(container->GetPtr()->MyInt(), 10);
        BOOST_CHECK_EQUAL(container->GetPtr()->MyString(), L"one");
        container = dynamic_cast<TestItemContainer*>(&base.GetValueContainerAt(1));
        BOOST_CHECK_NE(container, nullptr);
        BOOST_CHECK_EQUAL(container->IsNull(), false);
        BOOST_CHECK_EQUAL(container->GetPtr()->MyInt(), 20);
        BOOST_CHECK_EQUAL(container->GetPtr()->MyString(), L"two");
    }

    {
        DictionaryContainerBase& base = dict->Int64StringMember();
        BOOST_CHECK_EQUAL(base.size(), 3U);
        BOOST_CHECK_EQUAL(base.GetKeyAt<Int64>(0), 1);
        BOOST_CHECK_EQUAL(base.GetKeyAt<Int64>(1), 20);
        BOOST_CHECK_EQUAL(base.GetKeyAt<Int64>(2), 111);
        auto* container = dynamic_cast<StringContainer*>(&base.GetValueContainerAt(0));
        BOOST_CHECK_NE(container, nullptr);
        BOOST_CHECK_EQUAL(container->IsNull(), false);
        BOOST_CHECK_EQUAL(container->GetVal(), L"foo");
        container = dynamic_cast<StringContainer*>(&base.GetValueContainerAt(1));
        BOOST_CHECK_NE(container, nullptr);
        BOOST_CHECK_EQUAL(container->IsNull(), false);
        BOOST_CHECK_EQUAL(container->GetVal(), L"bar");
        container = dynamic_cast<StringContainer*>(&base.GetValueContainerAt(2));
        BOOST_CHECK_NE(container, nullptr);
        BOOST_CHECK_EQUAL(container->IsNull(), true);
    }
}

BOOST_AUTO_TEST_CASE(DictionaryReflection_InsertNull)
{
    MemberDictionariesPtr dict = MemberDictionaries::Create();

    // Int64 key
    {
        DictionaryContainerBase& base = dict->Int64ItemMember();

        auto& valCont = base.InsertNull(Int64(3));
        base.InsertNull(Int64(4));

        BOOST_CHECK_EQUAL(base.size(), 2U);
        auto objCont = static_cast<Safir::Dob::Typesystem::ObjectContainerBase*>(&valCont);
        auto testItem = TestItem::Create();
        testItem->MyInt() = 30;
        testItem->MyString() = L"Three";
        ObjectPtr obj = testItem;
        objCont->SetPtr(obj);

        Safir::Dob::Typesystem::DictionaryContainer<Safir::Dob::Typesystem::Int64, DotsTest::TestItemContainer> container;
        container.Copy(base);
        BOOST_CHECK_EQUAL(container.size(), 2U);

        auto itemCont3 = container.at(Int64(3));
        BOOST_CHECK_EQUAL(itemCont3.IsNull(), false);
        BOOST_CHECK_EQUAL(itemCont3.GetPtr()->MyInt().GetVal(), 30);
        BOOST_CHECK_EQUAL(itemCont3.GetPtr()->MyString().GetVal(), L"Three");

        auto itemCont4 = container.at(Int64(4));
        BOOST_CHECK_EQUAL(itemCont4.IsNull(), true);
    }


    // Enum key
    {
        DictionaryContainerBase& base = dict->EnumItemMember();
        auto& valCont = base.InsertNull(DotsTest::TestEnum::MySecond);
        BOOST_CHECK_EQUAL(base.size(), 1U);
        auto objCont = static_cast<Safir::Dob::Typesystem::ObjectContainerBase*>(&valCont);
        auto testItem = TestItem::Create();
        testItem->MyInt() = 30;
        testItem->MyString() = L"Three";
        ObjectPtr obj = testItem;
        objCont->SetPtr(obj);

        Safir::Dob::Typesystem::DictionaryContainer<DotsTest::TestEnum::Enumeration, DotsTest::TestItemContainer> container;
        container.Copy(base);
        BOOST_CHECK_EQUAL(container.size(), 1U);

        auto itemCont = container.at(DotsTest::TestEnum::MySecond);
        BOOST_CHECK_EQUAL(itemCont.IsNull(), false);
        BOOST_CHECK_EQUAL(itemCont.GetPtr()->MyInt().GetVal(), 30);
        BOOST_CHECK_EQUAL(itemCont.GetPtr()->MyString().GetVal(), L"Three");
    }

    // String key
    {
        DictionaryContainerBase& base = dict->StringItemMember();
        std::wstring key = L"MyKey";
        auto& valCont = base.InsertNull(key);
        BOOST_CHECK_EQUAL(base.size(), 1U);
        auto objCont = static_cast<Safir::Dob::Typesystem::ObjectContainerBase*>(&valCont);

        auto testItem = TestItem::Create();
        testItem->MyInt() = 30;
        testItem->MyString() = L"Three";
        ObjectPtr obj = testItem;
        auto val = MemberDictionaries::Create();
        val->Int64ItemMember()[1].SetPtr(obj);
        objCont->SetPtr(val);

        Safir::Dob::Typesystem::DictionaryContainer<std::wstring, DotsTest::MemberDictionariesContainer> container;
        container.Copy(base);
        BOOST_CHECK_EQUAL(container.size(), 1U);

        auto itemCont = container.at(key);
        BOOST_CHECK_EQUAL(itemCont.IsNull(), false);
        BOOST_CHECK_EQUAL(itemCont.GetPtr()->Int64ItemMember()[1]->MyInt().GetVal(), 30);
        BOOST_CHECK_EQUAL(itemCont.GetPtr()->Int64ItemMember()[1]->MyString().GetVal(), L"Three");
    }

    // InstanceId key
    {
        DictionaryContainerBase& base = dict->InstanceIdItemMember();
        InstanceId key(L"someInstance");

        auto& valCont = base.InsertNull(key);
        BOOST_CHECK_EQUAL(base.size(), 1U);
        auto objCont = static_cast<Safir::Dob::Typesystem::ObjectContainerBase*>(&valCont);

        auto testItem = TestItem::Create();
        testItem->MyInt() = 30;
        testItem->MyString() = L"Three";
        ObjectPtr obj = testItem;
        auto val = MemberDictionaries::Create();
        val->Int64ItemMember()[1].SetPtr(obj);
        objCont->SetPtr(val);

        Safir::Dob::Typesystem::DictionaryContainer<InstanceId, DotsTest::MemberDictionariesContainer> container;
        container.Copy(base);
        BOOST_CHECK_EQUAL(container.size(), 1U);

        auto itemCont = container.at(key);
        BOOST_CHECK_EQUAL(itemCont.IsNull(), false);
        BOOST_CHECK_EQUAL(itemCont.GetPtr()->Int64ItemMember()[1]->MyInt().GetVal(), 30);
        BOOST_CHECK_EQUAL(itemCont.GetPtr()->Int64ItemMember()[1]->MyString().GetVal(), L"Three");
    }

    // EntityId key
    {
        DictionaryContainerBase& base = dict->EntityIdItemMember();
        Safir::Dob::Typesystem::EntityId key(Safir::Dob::Entity::ClassTypeId, InstanceId(3));

        auto& valCont = base.InsertNull(key);
        BOOST_CHECK_EQUAL(base.size(), 1U);
        auto objCont = static_cast<Safir::Dob::Typesystem::ObjectContainerBase*>(&valCont);

        auto testItem = TestItem::Create();
        testItem->MyInt() = 30;
        testItem->MyString() = L"Three";
        ObjectPtr obj = testItem;
        auto val = MemberDictionaries::Create();
        val->Int64ItemMember()[1].SetPtr(obj);
        objCont->SetPtr(val);

        Safir::Dob::Typesystem::DictionaryContainer<EntityId, DotsTest::MemberDictionariesContainer> container;
        container.Copy(base);
        BOOST_CHECK_EQUAL(container.size(), 1U);

        auto itemCont = container.at(key);
        BOOST_CHECK_EQUAL(itemCont.IsNull(), false);
        BOOST_CHECK_EQUAL(itemCont.GetPtr()->Int64ItemMember()[1]->MyInt().GetVal(), 30);
        BOOST_CHECK_EQUAL(itemCont.GetPtr()->Int64ItemMember()[1]->MyString().GetVal(), L"Three");
    }

    // Int32Object dictionary
    {
        DictionaryContainerBase& base = dict->Int32ObjectMember();

        auto& valCont1 = base.InsertNull(1);
        auto& valCont2 = base.InsertNull(2);
        base.InsertNull(3);
        BOOST_CHECK_EQUAL(base.size(), 3U);

        auto objCont1 = static_cast<Safir::Dob::Typesystem::ObjectContainerBase*>(&valCont1);
        auto obj1 = TestItem::Create();
        obj1->MyInt() = 30;
        obj1->MyString() = L"Three";
        objCont1->SetPtr(obj1);

        auto objCont2 = static_cast<Safir::Dob::Typesystem::ObjectContainerBase*>(&valCont2);
        auto obj2 = TestItem::Create();
        obj2->MyInt() = 40;
        obj2->MyString() = L"Four";
        auto val = MemberDictionaries::Create();
        val->Int64ItemMember()[1].SetPtr(obj2);
        objCont2->SetPtr(val);

        Safir::Dob::Typesystem::DictionaryContainer<Safir::Dob::Typesystem::Int32, Safir::Dob::Typesystem::ObjectContainer> container;
        container.Copy(base);
        BOOST_CHECK_EQUAL(container.size(), 3U);

        auto itemCont1 = container.at(1);
        BOOST_CHECK_EQUAL(itemCont1.IsNull(), false);
        auto testItem = std::dynamic_pointer_cast<TestItem>(itemCont1.GetPtr());
        BOOST_CHECK(testItem.get());
        BOOST_CHECK_EQUAL(testItem->MyInt().GetVal(), 30);
        BOOST_CHECK_EQUAL(testItem->MyString().GetVal(), L"Three");

        auto itemCont2 = container.at(2);
        BOOST_CHECK_EQUAL(itemCont2.IsNull(), false);
        auto memDict = std::dynamic_pointer_cast<MemberDictionaries>(itemCont2.GetPtr());
        BOOST_CHECK(memDict.get());

        BOOST_CHECK_EQUAL(memDict->Int64ItemMember()[1].GetPtr()->MyInt().GetVal(), 40);
        BOOST_CHECK_EQUAL(memDict->Int64ItemMember()[1].GetPtr()->MyString().GetVal(), L"Four");

        auto itemCont4 = container.at(3);
        BOOST_CHECK_EQUAL(itemCont4.IsNull(), true);
    }

    // Wrong key type throws SoftwareViolationException
    {
        BOOST_CHECK_THROW(dict->Int64ItemMember().InsertNull(3), Safir::Dob::Typesystem::SoftwareViolationException);
        BOOST_CHECK_THROW(dict->EntityIdItemMember().InsertNull(L"Hello"), Safir::Dob::Typesystem::SoftwareViolationException);
        BOOST_CHECK_THROW(dict->StringItemMember().InsertNull(L"hej"), Safir::Dob::Typesystem::SoftwareViolationException); // expects w_char* is not a wstring
    }

}

BOOST_AUTO_TEST_CASE(ParserExceptions)
{
    const std::wstring brokenXml = L"<?xml version=\"1.0\" encoding=\"utf-8\"?><DotsTest.MemberSequences><Int32WRONGMember><Int32>10</Int32></Int32Member></DotsTest.MemberSequences>";
    const std::wstring brokenJson = L"{\"_DouType\":\"DotsTest.MemberSequences\",\"Int32WRONGMember\":[10,20]}";

    BOOST_CHECK_EXCEPTION(ts::Serialization::ToObject(brokenXml),
                          ts::IllegalValueException,
                          [](const auto& exc) {return boost::algorithm::contains(exc.what(),"does not contain a member named");});
    BOOST_CHECK_EXCEPTION(ts::Serialization::ToObjectFromJson(brokenJson),
                          ts::IllegalValueException,
                          [](const auto& exc) {return boost::algorithm::contains(exc.what(),"does not contain a member named");});
    BOOST_CHECK_THROW(ts::Serialization::ToObject(L""),
                      ts::IllegalValueException);
    BOOST_CHECK_THROW(ts::Serialization::ToObjectFromJson(L""),
                      ts::IllegalValueException);
}


BOOST_AUTO_TEST_CASE(ParameterGetInfo)
{
    Dob::Typesystem::MemberType parameterType;
    Dob::Typesystem::MemberType keyType;
    std::wstring parameterName;
    Dob::Typesystem::TypeId parameterTypeId;
    Dob::Typesystem::TypeId keyTypeId;
    Dob::Typesystem::CollectionType collectionType;
    Dob::Typesystem::Int32 numberOfValues;

    Safir::Dob::Typesystem::Parameters::GetInfo(DotsTest::ParameterDictionaries::ClassTypeId,
                                                Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterDictionaries::ClassTypeId,
                                                                                             L"Int32StringParameter"),
                                                parameterType,
                                                keyType,
                                                parameterName,
                                                parameterTypeId,
                                                keyTypeId,
                                                collectionType,
                                                numberOfValues);
    BOOST_CHECK_EQUAL(parameterType, StringMemberType);
    BOOST_CHECK_EQUAL(keyType, Int32MemberType);
    BOOST_CHECK_EQUAL(parameterName, L"Int32StringParameter");
    BOOST_CHECK_EQUAL(collectionType, DictionaryCollectionType);
    BOOST_CHECK_EQUAL(numberOfValues, 2);

    Safir::Dob::Typesystem::Parameters::GetInfo(DotsTest::ParameterDictionaries::ClassTypeId,
                                                Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterDictionaries::ClassTypeId,
                                                                                             L"StringEnumParameter"),
                                                parameterType,
                                                keyType,
                                                parameterName,
                                                parameterTypeId,
                                                keyTypeId,
                                                collectionType,
                                                numberOfValues);
    BOOST_CHECK_EQUAL(parameterType, EnumerationMemberType);
    BOOST_CHECK_EQUAL(keyType, StringMemberType);
    BOOST_CHECK_EQUAL(parameterName, L"StringEnumParameter");
    BOOST_CHECK_EQUAL(collectionType, DictionaryCollectionType);
    BOOST_CHECK_EQUAL(parameterTypeId, DotsTest::TestEnum::EnumerationTypeId);
    BOOST_CHECK_EQUAL(numberOfValues, 2);

    Safir::Dob::Typesystem::Parameters::GetInfo(DotsTest::ParameterDictionaries::ClassTypeId,
                                                Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterDictionaries::ClassTypeId,
                                                                                             L"EnumObjectParameter"),
                                                parameterType,
                                                keyType,
                                                parameterName,
                                                parameterTypeId,
                                                keyTypeId,
                                                collectionType,
                                                numberOfValues);
    BOOST_CHECK_EQUAL(parameterType, ObjectMemberType);
    BOOST_CHECK_EQUAL(keyType, EnumerationMemberType);
    BOOST_CHECK_EQUAL(parameterName, L"EnumObjectParameter");
    BOOST_CHECK_EQUAL(collectionType, DictionaryCollectionType);
    BOOST_CHECK_EQUAL(parameterTypeId, Safir::Dob::Typesystem::Object::ClassTypeId);
    BOOST_CHECK_EQUAL(keyTypeId, DotsTest::TestEnum::EnumerationTypeId);
    BOOST_CHECK_EQUAL(numberOfValues, 2);


    Safir::Dob::Typesystem::Parameters::GetInfo(DotsTest::ParameterTypes::ClassTypeId,
                                                Safir::Dob::Typesystem::Parameters::GetIndex(DotsTest::ParameterTypes::ClassTypeId,
                                                                                             L"EnumerationParameter"),
                                                parameterType,
                                                keyType,
                                                parameterName,
                                                parameterTypeId,
                                                keyTypeId,
                                                collectionType,
                                                numberOfValues);
    BOOST_CHECK_EQUAL(parameterType, EnumerationMemberType);
    BOOST_CHECK_EQUAL(parameterName, L"EnumerationParameter");
    BOOST_CHECK_EQUAL(collectionType, SingleValueCollectionType);
    BOOST_CHECK_EQUAL(parameterTypeId, DotsTest::TestEnum::EnumerationTypeId);
    BOOST_CHECK_EQUAL(numberOfValues, 1);
}

BOOST_AUTO_TEST_CASE(ParameterDictionaryReflection)
{
    BOOST_CHECK_EQUAL(2, DotsTest::ParameterDictionaries::Int32StringParameterDictionarySize());
    BOOST_CHECK_EQUAL(10, DotsTest::ParameterDictionaries::Int32StringParameterKeyFromIndex(0));
    BOOST_CHECK_EQUAL(L"Safir", DotsTest::ParameterDictionaries::Int32StringParameterValueFromIndex(0));
    BOOST_CHECK_EQUAL(20, DotsTest::ParameterDictionaries::Int32StringParameterKeyFromIndex(1));
    BOOST_CHECK_EQUAL(L"rifaS", DotsTest::ParameterDictionaries::Int32StringParameterValueFromIndex(1));

    BOOST_CHECK_EQUAL(2, DotsTest::ParameterDictionaries::Int32Float64ParameterDictionarySize());
    BOOST_CHECK_EQUAL(10, DotsTest::ParameterDictionaries::Int32Float64ParameterKeyFromIndex(0));
    BOOST_CHECK_EQUAL(64.64, DotsTest::ParameterDictionaries::Int32Float64ParameterValueFromIndex(0));
    BOOST_CHECK_EQUAL(20, DotsTest::ParameterDictionaries::Int32Float64ParameterKeyFromIndex(1));
    BOOST_CHECK_EQUAL(-64.64, DotsTest::ParameterDictionaries::Int32Float64ParameterValueFromIndex(1));

    BOOST_CHECK_EQUAL(2, DotsTest::ParameterDictionaries::Int32Ampere64ParameterDictionarySize());
    BOOST_CHECK_EQUAL(10, DotsTest::ParameterDictionaries::Int32Ampere64ParameterKeyFromIndex(0));
    BOOST_CHECK_EQUAL(64.64, DotsTest::ParameterDictionaries::Int32Ampere64ParameterValueFromIndex(0));
    BOOST_CHECK_EQUAL(20, DotsTest::ParameterDictionaries::Int32Ampere64ParameterKeyFromIndex(1));
    BOOST_CHECK_EQUAL(-64.64, DotsTest::ParameterDictionaries::Int32Ampere64ParameterValueFromIndex(1));

    BOOST_CHECK_EQUAL(2, DotsTest::ParameterDictionaries::StringEnumParameterDictionarySize());
    BOOST_CHECK_EQUAL(L"Billy", DotsTest::ParameterDictionaries::StringEnumParameterKeyFromIndex(0));
    BOOST_CHECK_EQUAL(DotsTest::TestEnum::MyFirst, DotsTest::ParameterDictionaries::StringEnumParameterValueFromIndex(0));
    BOOST_CHECK_EQUAL(L"Svarre", DotsTest::ParameterDictionaries::StringEnumParameterKeyFromIndex(1));
    BOOST_CHECK_EQUAL(DotsTest::TestEnum::MySecond, DotsTest::ParameterDictionaries::StringEnumParameterValueFromIndex(1));

    BOOST_CHECK_EQUAL(2, DotsTest::ParameterDictionaries::EnumObjectParameterDictionarySize());
    BOOST_CHECK_EQUAL(DotsTest::TestEnum::MyFirst, DotsTest::ParameterDictionaries::EnumObjectParameterKeyFromIndex(0));
    BOOST_CHECK_NE(nullptr, DotsTest::ParameterDictionaries::EnumObjectParameterValueFromIndex(0));
    BOOST_CHECK_EQUAL(Safir::Dob::Typesystem::Object::ClassTypeId, DotsTest::ParameterDictionaries::EnumObjectParameterValueFromIndex(0)->GetTypeId());
    BOOST_CHECK_EQUAL(DotsTest::TestEnum::MySecond, DotsTest::ParameterDictionaries::EnumObjectParameterKeyFromIndex(1));
    BOOST_CHECK_NE(nullptr, std::dynamic_pointer_cast<DotsTest::MemberDictionaries>(DotsTest::ParameterDictionaries::EnumObjectParameterValueFromIndex(1)));
}


BOOST_AUTO_TEST_CASE(Base64Conversion1)
{
    const char bin [] = "asdfasdfasdfasdf";
    const std::vector<char> binv (bin, bin+strlen(bin));
    const auto base64 = Safir::Dob::Typesystem::Utilities::BinaryToBase64(bin,static_cast<int>(strlen(bin)));
    const auto base64v = Safir::Dob::Typesystem::Utilities::BinaryToBase64(binv);
    BOOST_CHECK_EQUAL(base64,"YXNkZmFzZGZhc2RmYXNkZg==");
    BOOST_CHECK_EQUAL(base64v,"YXNkZmFzZGZhc2RmYXNkZg==");

    std::vector<char> back, backv;
    Safir::Dob::Typesystem::Utilities::Base64ToBinary(base64,back);
    Safir::Dob::Typesystem::Utilities::Base64ToBinary(base64v,backv);
    BOOST_CHECK_EQUAL(back,binv);
    BOOST_CHECK_EQUAL(backv,binv);
}

BOOST_AUTO_TEST_CASE(Base64Conversion2)
{
    const char bin [] = "asdfasdfasdfasdfaa";
    const std::vector<char> binv (bin, bin+strlen(bin));
    const auto base64 = Safir::Dob::Typesystem::Utilities::BinaryToBase64(bin,static_cast<int>(strlen(bin)));
    const auto base64v = Safir::Dob::Typesystem::Utilities::BinaryToBase64(binv);
    BOOST_CHECK_EQUAL(base64,"YXNkZmFzZGZhc2RmYXNkZmFh");
    BOOST_CHECK_EQUAL(base64v,"YXNkZmFzZGZhc2RmYXNkZmFh");

    std::vector<char> back, backv;
    Safir::Dob::Typesystem::Utilities::Base64ToBinary(base64,back);
    Safir::Dob::Typesystem::Utilities::Base64ToBinary(base64v,backv);
    BOOST_CHECK_EQUAL(back,binv);
    BOOST_CHECK_EQUAL(backv,binv);
}

BOOST_AUTO_TEST_CASE(Base64Conversion3)
{
    const char bin [] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
    const std::vector<char> binv (bin, bin+strlen(bin));
    const auto base64 = Safir::Dob::Typesystem::Utilities::BinaryToBase64(bin,static_cast<int>(strlen(bin)));
    const auto base64v = Safir::Dob::Typesystem::Utilities::BinaryToBase64(binv);
    BOOST_CHECK_EQUAL(base64,"TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIGNvbnNlY3RldHVyIGFkaXBpc2NpbmcgZWxpdCwgc2VkIGRvIGVpdXNtb2QgdGVtcG9yIGluY2lkaWR1bnQgdXQgbGFib3JlIGV0IGRvbG9yZSBtYWduYSBhbGlxdWEuIFV0IGVuaW0gYWQgbWluaW0gdmVuaWFtLCBxdWlzIG5vc3RydWQgZXhlcmNpdGF0aW9uIHVsbGFtY28gbGFib3JpcyBuaXNpIHV0IGFsaXF1aXAgZXggZWEgY29tbW9kbyBjb25zZXF1YXQuIER1aXMgYXV0ZSBpcnVyZSBkb2xvciBpbiByZXByZWhlbmRlcml0IGluIHZvbHVwdGF0ZSB2ZWxpdCBlc3NlIGNpbGx1bSBkb2xvcmUgZXUgZnVnaWF0IG51bGxhIHBhcmlhdHVyLiBFeGNlcHRldXIgc2ludCBvY2NhZWNhdCBjdXBpZGF0YXQgbm9uIHByb2lkZW50LCBzdW50IGluIGN1bHBhIHF1aSBvZmZpY2lhIGRlc2VydW50IG1vbGxpdCBhbmltIGlkIGVzdCBsYWJvcnVtLg==");
    BOOST_CHECK_EQUAL(base64v,"TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIGNvbnNlY3RldHVyIGFkaXBpc2NpbmcgZWxpdCwgc2VkIGRvIGVpdXNtb2QgdGVtcG9yIGluY2lkaWR1bnQgdXQgbGFib3JlIGV0IGRvbG9yZSBtYWduYSBhbGlxdWEuIFV0IGVuaW0gYWQgbWluaW0gdmVuaWFtLCBxdWlzIG5vc3RydWQgZXhlcmNpdGF0aW9uIHVsbGFtY28gbGFib3JpcyBuaXNpIHV0IGFsaXF1aXAgZXggZWEgY29tbW9kbyBjb25zZXF1YXQuIER1aXMgYXV0ZSBpcnVyZSBkb2xvciBpbiByZXByZWhlbmRlcml0IGluIHZvbHVwdGF0ZSB2ZWxpdCBlc3NlIGNpbGx1bSBkb2xvcmUgZXUgZnVnaWF0IG51bGxhIHBhcmlhdHVyLiBFeGNlcHRldXIgc2ludCBvY2NhZWNhdCBjdXBpZGF0YXQgbm9uIHByb2lkZW50LCBzdW50IGluIGN1bHBhIHF1aSBvZmZpY2lhIGRlc2VydW50IG1vbGxpdCBhbmltIGlkIGVzdCBsYWJvcnVtLg==");

    std::vector<char> back, backv;
    Safir::Dob::Typesystem::Utilities::Base64ToBinary(base64,back);
    Safir::Dob::Typesystem::Utilities::Base64ToBinary(base64v,backv);
    BOOST_CHECK_EQUAL(back,binv);
    BOOST_CHECK_EQUAL(backv,binv);
}
