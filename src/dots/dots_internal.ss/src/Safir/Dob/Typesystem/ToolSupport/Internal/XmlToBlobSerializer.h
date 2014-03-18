/******************************************************************************
*
* Copyright Saab AB, 2004-2013 (http://safir.sourceforge.net)
*
* Created by: Joel Ottosson / joot
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
* GNU General Public License for more Internals.
*
* You should have received a copy of the GNU General Public License
* along with Safir SDK Core.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/
#ifndef __DOTS_INTERNAL_XML_TO_BLOB_H__
#define __DOTS_INTERNAL_XML_TO_BLOB_H__

#include <string>
#include <vector>
#include <sstream>
#include <boost/noncopyable.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <Safir/Utilities/Internal/Id.h>
#include <Safir/Dob/Typesystem/ToolSupport/TypeRepository.h>
#include <Safir/Dob/Typesystem/ToolSupport/TypeUtilities.h>
#include <Safir/Dob/Typesystem/ToolSupport/Internal/BlobLayoutImpl.h>
#include <Safir/Dob/Typesystem/ToolSupport/Internal/UglyXmlToBlobSerializer.h>

namespace Safir
{
namespace Dob
{
namespace Typesystem
{
namespace ToolSupport
{
namespace Internal
{
    template <class RepT, class Traits=Safir::Dob::Typesystem::ToolSupport::TypeRepositoryTraits<RepT> >
    class XmlToBlobSerializer : private boost::noncopyable
    {
    public:
        typedef typename Traits::RepositoryType RepositoryType;
        typedef typename Traits::ClassDescriptionType ClassDescriptionType;
        typedef typename Traits::MemberDescriptionType MemberDescriptionType;
        typedef typename Traits::EnumDescriptionType EnumDescriptionType;

        XmlToBlobSerializer(const RepositoryType* repository)
            :m_repository(repository)
            ,m_blobLayout(repository)
        {
        }

        void operator()(const char* xml, std::vector<char>& blob) const
        {
            boost::property_tree::ptree pt;
            boost::iostreams::array_source src(xml, strlen(xml));
            boost::iostreams::stream<boost::iostreams::array_source> stream(src);
            boost::property_tree::xml_parser::read_xml(stream, pt, boost::property_tree::xml_parser::no_comments);
            this->operator ()(pt, blob);
        }

        void operator()(boost::property_tree::ptree& xml, std::vector<char>& blob) const
        {
            //*********************** One day this should be removed: Handle old deprecated XML format ***********************
            if (xml.front().first=="object")
            {
                //redirect to the UglyXml xml parser
                (UglyXmlToBlobSerializer<RepositoryType>(m_repository))(xml, blob);
                return;
            }
            //*****************************End To be removed *********************************************************

            boost::property_tree::ptree& members=xml.front().second;
            boost::optional<std::string> xsiType=members.get_optional<std::string>("<xmlattr>.type");
            std::string typeName;
            if (xsiType)
            {
                typeName=*xsiType;
            }
            else
            {
                typeName=xml.front().first;
            }
            SerializationUtils::Trim(typeName);
            SerializeObjectContent(typeName, blob, members);

        }

        //This one is for internal use and should be considered private. It parses an xml-object but starts one level in from the root
        //i.e <myObject type="anyType><myInt>4</myInt><myString>hello</myString></myObject>, when using this mehtod the ptree members
        // must be <myInt>4</myInt><myString>hello</myString> and typeName must be handed as in-parameter. In then normal case when we
        //have an ptree at root level use operator()(const boost::property_tree::ptree& xml, std::vector<char>& blob)
        void SerializeObjectContent(const std::string& typeName,
                                    std::vector<char>& blob,
                                    boost::property_tree::ptree& members) const
        {
            DotsC_TypeId typeId=LlufId_Generate64(typeName.c_str());
            const ClassDescriptionType* cd=m_repository->GetClass(typeId);
            if (!cd)
            {
                throw ParseError("XmlToBinary serialization error", "Xml does not contain a known type. Typename: "+typeName, "", 151);
            }

            char* beginningOfUnused=NULL;

            size_t blobInitSize=std::max(size_t(1000), static_cast<size_t>(2*cd->InitialSize()));
            blob.reserve(blobInitSize); //Note: maybe xmlSize/2 would be enogh in almost all cases
            blob.resize(cd->InitialSize(), 0);
            m_blobLayout.FormatBlob(&blob[0], static_cast<Size>(blob.size()), typeId, beginningOfUnused);

            for (boost::property_tree::ptree::iterator memIt=members.begin(); memIt!=members.end(); ++memIt)
            {
                const std::string& elementName=memIt->first;
                int memIx=cd->GetMemberIndex(elementName);
                if (memIx<0)
                {
                    if (elementName=="<xmlattr>")
                    {
                        continue; //we ignore attributes.
                    }

                    std::ostringstream os;
                    os<<"Failed to serialize xml to binary. The class '"<<cd->GetName()<<"' does not contain a member named '"<<elementName<<"'";
                    throw ParseError("XmlToBinary serialization error", os.str(), "", 152);
                }

                const MemberDescriptionType* md=cd->GetMember(memIx);

                if (!md->IsArray())
                {
                    //non-array, then the inner propertyTree contains the content, i.e <myInt>123</myInt>
                    try
                    {
                        SetMember(md, memIx, 0, memIt->second, blob, beginningOfUnused);
                    }
                    catch (const boost::property_tree::ptree_error&)
                    {
                        std::ostringstream os;
                        os<<"Failed to serialize member '"<<cd->GetName()<<"."<<md->GetName()<<"' from xml to binary. Type is incorrect.";
                        throw ParseError("XmlToBinary serialization error", os.str(), "", 153);
                    }
                }
                else
                {
                    //array, then the inner propertyTree contains array element and the array elements contains the content
                    //i.e <myIntArray><Int32 index=0>1</Int32><Int32 index=5>2</Int32></myIntArray>
                    int arrayIndex=0;
                    bool usesIndexAttr=(!memIt->second.empty() && memIt->second.begin()->second.get_optional<int>("<xmlattr>.index")) ? true : false;

                    for (boost::property_tree::ptree::iterator arrIt=memIt->second.begin(); arrIt!=memIt->second.end(); ++arrIt)
                    {
                        boost::optional<int> index=arrIt->second.get_optional<int>("<xmlattr>.index");
                        if (usesIndexAttr)
                        {
                            //we expect an index attribute on every array element
                            if (index)
                            {
                                arrayIndex=*index;
                            }
                            else
                            {
                                std::ostringstream os;
                                os<<"Serialization from xml to binary failed because the xml of array member '"<<md->GetName()<<"' is missing index-attribute";
                                throw ParseError("XmlToBinary serialization error", os.str(), "", 156);
                            }
                        }
                        else if (index) //not usesIndexAttr but got it anyway
                        {
                            //We got an index attribute but does not expect it since it has not been present for every previous array elements
                            std::ostringstream os;
                            os<<"Serialization from xml to binary failed because the xml of array member '"<<md->GetName()<<"' is contains an unexpected index-attribute. Index must be present on every array element or none. Not just some of them!";
                            throw ParseError("XmlToBinary serialization error", os.str(), "", 176);
                        }

                        if (md->GetArraySize()<=arrayIndex)
                        {
                            std::ostringstream os;
                            os<<"Failed to serialize array member '"<<cd->GetName()<<"."<<md->GetName()<<"' with index="<<arrayIndex<<" from xml to binary. Index out of range. ArraySize is "<<md->GetArraySize();
                            throw ParseError("XmlToBinary serialization error", os.str(), "", 154);
                        }

                        try
                        {
                            SetMember(md, memIx, arrayIndex, arrIt->second, blob, beginningOfUnused);
                        }
                        catch (const boost::property_tree::ptree_error&)
                        {
                            std::ostringstream os;
                            os<<"Failed to serialize array member '"<<cd->GetName()<<"."<<md->GetName()<<"' with index="<<arrayIndex<<" from xml to binary. Type is incorrect.";
                            throw ParseError("XmlToBinary serialization error", os.str(), "", 155);
                        }

                        ++arrayIndex;
                    }
                }
            }
        }

    private:
        const RepositoryType* m_repository;
        const BlobLayoutImpl<RepositoryType> m_blobLayout;

        void SetMember(const MemberDescriptionType* md,
                       DotsC_MemberIndex memIx,
                       DotsC_ArrayIndex arrIx,
                       boost::property_tree::ptree& memberContent,
                       std::vector<char>& blob,
                       char* &beginningOfUnused) const
        {
            boost::optional<std::string> valueRef=memberContent.get_optional<std::string>("<xmlattr>.valueRef");
            int valueRefIndex=memberContent.get<int>("<xmlattr>.valueRefIndex", 0);
            if (valueRef)
            {
                SerializationUtils::Trim(*valueRef);

                if (memberContent.size()>1 || !memberContent.data().empty())
                {
                    std::ostringstream os;
                    os<<"Member '"<<md->GetName()<<"' is referencing a parameter and hence is not allowed to contain data or sub elements";
                    throw ParseError("XmlToBinary serialization error", os.str(), "", 109);
                }
                else if (md->GetMemberType()==ObjectMemberType)
                {
                    std::ostringstream os;
                    os<<"Only members of non-object types can use the valueRef mechanism. Member '"<<md->GetName()<<"' has type "<<m_repository->GetClass(md->GetTypeId())->GetName();
                    throw ParseError("XmlToBinary serialization error", os.str(), "", 110);
                }
                SerializationUtils::SetMemberFromParameter(m_repository, m_blobLayout, md, memIx, arrIx, *valueRef, valueRefIndex, blob, beginningOfUnused);
            }
            else if (md->GetMemberType()==ObjectMemberType)
            {
                //If object we must find the exact type. Inheritance possible.
                const ClassDescriptionType* cd=NULL;
                boost::optional<std::string> xsiType=memberContent.get_optional<std::string>("<xmlattr>.type");
                if (xsiType)
                {
                    SerializationUtils::Trim(*xsiType);
                    cd=m_repository->GetClass(LlufId_Generate64(xsiType->c_str()));
                    if (!cd)
                    {
                        std::ostringstream os;
                        os<<"Attribute 'type' on member "<<md->GetName()<<" does not specifying a known class. type="<<(*xsiType);
                        throw ParseError("XmlToBinary serialization error", os.str(), "", 157);
                    }
                    else if (!BasicTypeOperations::IsOfType(m_repository, ObjectMemberType, cd->GetTypeId(), ObjectMemberType, md->GetTypeId()))
                    {
                        std::ostringstream os;
                        os<<"Attribute 'type' on member "<<md->GetName()<<" is specitying an invalid type. "<<cd->GetName()<<" is not a subtype of "<<m_repository->GetClass(md->GetTypeId())->GetName();
                        throw ParseError("XmlToBinary serialization error", os.str(), "", 150);
                    }
                }
                else
                {
                    cd=m_repository->GetClass(md->GetTypeId());
                }

                std::vector<char> insideBlob;
                SerializeObjectContent(cd->GetName(), insideBlob, memberContent);
                SerializationUtils::CreateSpaceForDynamicMember(m_blobLayout, blob, beginningOfUnused, insideBlob.size());
                char* writeObj=beginningOfUnused;
                m_blobLayout.CreateObjectMember(&blob[0], static_cast<Size>(insideBlob.size()), cd->GetTypeId(), memIx, arrIx, false, beginningOfUnused);
                beginningOfUnused=writeObj+insideBlob.size(); //This is a hack. BlobLayout is not moving beginningOfUnused by the blobSize but instead only by the initialSize. Has to do with genated code.
                memcpy(writeObj, &insideBlob[0], insideBlob.size());
                m_blobLayout.SetStatus(false, true, &blob[0], memIx, arrIx);
            }
            else
            {
                SerializationUtils::SetMemberValue(m_repository, m_blobLayout, md, memIx, arrIx, memberContent, blob, beginningOfUnused);
            }

        }
    };
}
}
}
}
} //end namespace Safir::Dob::Typesystem::Internal:Internal

#endif
