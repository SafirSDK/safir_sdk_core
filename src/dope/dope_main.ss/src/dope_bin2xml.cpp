/******************************************************************************
*
* Copyright Saab AB, 2006-2013 (http://safir.sourceforge.net)
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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning (disable: 4251)
#pragma warning (disable: 4275)
#pragma warning (disable: 4100)
#endif

#include <boost/program_options.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <boost/scoped_array.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/filesystem/fstream.hpp>

#include <Safir/Dob/Typesystem/Serialization.h>
#include <Safir/Dob/Typesystem/ObjectFactory.h>

#include <Safir/Dob/PersistenceParameters.h>


#include <Safir/Logging/Log.h>


// Somehow we need this included inorder to compile the ODBC header files below.
// I don't know why but it probably includes windows.h somehow...
#include <Safir/Utilities/AsioDispatcher.h>

#include <sqltypes.h>
#include <sql.h>
#include <sqlext.h>

#include <iostream>
enum WhatToConvert {db, files};

WhatToConvert g_whatToConvert;

// Odbc helper functions. See below
void AllocStatement(SQLHSTMT * hStmt, SQLHDBC hConnection);
void BindColumnInt64( SQLHSTMT hStmt,
                      unsigned short usColumnNumber,
                      Safir::Dob::Typesystem::Int64 * value,
                      SQLINTEGER * size);
void BindColumnBinary( SQLHSTMT hStmt,
                       unsigned short usColumnNumber,
                       const int maxSize,
                       unsigned char * buffer,
                       SQLINTEGER * sizePtr );
void BindParamInt64(SQLHSTMT hStmt, const SQLUSMALLINT paramNumber, Safir::Dob::Typesystem::Int64 * value, SQLINTEGER * size);
void BindParamString(SQLHSTMT hStmt,
                     const SQLUSMALLINT paramNumber,
                     const SQLUINTEGER maxSize,
                     wchar_t * string,
                     SQLINTEGER * sizePtr);
void Connect(SQLHDBC hConnection, const std::wstring & connectionString);
void Execute(SQLHSTMT hStmt);
bool Fetch( SQLHSTMT hStmt );
void Prepare(SQLHSTMT hStmt, const std::wstring & sql);
void ThrowException(SQLSMALLINT   HandleType,
                    SQLHANDLE     Handle);

void ParseCommandLine(int argc, char * argv[])
{
    namespace po = boost::program_options;
    // Declare the supported options.
    po::options_description options("Allowed options");
    options.add_options()
        ("help", "produce help message")
        ("db", "Convert the db")
        ("files", "Convert the files")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::ostringstream ostr;
        ostr << options;
        std::wcout << "Converts persistent data in binary format to XML\n\n"
                     "If no option is given the choice between db or files\n"
                     "is determined by Safir.Dob.PersistenceParameters.Backend.\n\n"
                   << ostr.str().c_str() << std::endl;
        exit(1);
    }

    if (vm.count("db") && vm.count("files"))
    {
        std::wcerr << "You can only convert db or files, not both at the same time" << std::endl;
        exit(1);
    }
    else if (vm.count("db"))
    {
        g_whatToConvert = db;
    }
    else if (vm.count("files"))
    {
        g_whatToConvert = files;
    }
    else
    {
        // No command line parameter given. We read the persistence
        // configuration parameter instead.
        switch (Safir::Dob::PersistenceParameters::Backend())
        {
            case Safir::Dob::PersistenceBackend::File:
            {
                g_whatToConvert = files;
            }
            break;

            case Safir::Dob::PersistenceBackend::Odbc:
            {
                g_whatToConvert = db;
            }
            break;

            default:
                throw Safir::Dob::Typesystem::SoftwareViolationException(L"Unknown backend!",__WFILE__,__LINE__);
        }
    }
}


void ConvertDb()
{
    SQLHENV                             hEnvironment;
    SQLHDBC                             hReadConnection;
    SQLHDBC                             hUpdateConnection;

    SQLINTEGER                          currentInt64Size(sizeof(Safir::Dob::Typesystem::Int64));

    SQLHSTMT                            hGetAllStatement;
    Safir::Dob::Typesystem::Int64       typeIdColumn;
    Safir::Dob::Typesystem::Int64       instanceColumn;
    boost::scoped_array<unsigned char>  largeBinaryDataColumn(new unsigned char[Safir::Dob::PersistenceParameters::BinaryDataColumnSize()]);
    SQLINTEGER                          largeBinaryDataColumnSize(0);
    boost::scoped_array<unsigned char>  smallBinaryDataColumn(new unsigned char[Safir::Dob::PersistenceParameters::BinarySmallDataColumnSize()]);
    SQLINTEGER                          smallBinaryDataColumnSize(0);

    SQLHSTMT                            hUpdateStatement;
    Safir::Dob::Typesystem::Int64       updateTypeIdParam;
    Safir::Dob::Typesystem::Int64       updateInstanceParam;
    boost::scoped_array<wchar_t>        updateXmlDataParam(new wchar_t[Safir::Dob::PersistenceParameters::XmlDataColumnSize()]);
    SQLINTEGER                          updateXmlDataParamSize(0);

    SQLRETURN ret = ::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvironment);
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_ENV, SQL_NULL_HANDLE);
    }

    ret = ::SQLSetEnvAttr(  hEnvironment,
                            SQL_ATTR_ODBC_VERSION,
                            reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3),
                            SQL_IS_UINTEGER );
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_ENV, hEnvironment);
    }

    ret = ::SQLAllocHandle(SQL_HANDLE_DBC, hEnvironment, &hReadConnection);
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_ENV, hEnvironment);
    }

    ret = ::SQLAllocHandle(SQL_HANDLE_DBC, hEnvironment, &hUpdateConnection);
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_ENV, hEnvironment);
    }

    Connect( hReadConnection, Safir::Dob::PersistenceParameters::OdbcStorageConnectString() );
    Connect( hUpdateConnection, Safir::Dob::PersistenceParameters::OdbcStorageConnectString() );

    AllocStatement( &hUpdateStatement, hUpdateConnection );
    Prepare( hUpdateStatement, L"UPDATE PersistentEntity SET xmlData=?, binarySmallData=NULL, binaryData=NULL WHERE typeId=? AND instance=?");

    BindParamString( hUpdateStatement, 1, Safir::Dob::PersistenceParameters::XmlDataColumnSize(), updateXmlDataParam.get(), &updateXmlDataParamSize );
    BindParamInt64( hUpdateStatement, 2, &updateTypeIdParam, &currentInt64Size );
    BindParamInt64( hUpdateStatement, 3, &updateInstanceParam, &currentInt64Size );

    AllocStatement( &hGetAllStatement, hReadConnection );
    Prepare( hGetAllStatement, L"SELECT typeId, instance, binarySmallData, binaryData from PersistentEntity where binaryData is not null or binarySmallData is not null");

    BindColumnInt64( hGetAllStatement, 1, &typeIdColumn, &currentInt64Size );
    BindColumnInt64( hGetAllStatement, 2, &instanceColumn, &currentInt64Size );
    BindColumnBinary( hGetAllStatement, 3, Safir::Dob::PersistenceParameters::BinarySmallDataColumnSize(), smallBinaryDataColumn.get(), &smallBinaryDataColumnSize );
    BindColumnBinary( hGetAllStatement, 4, Safir::Dob::PersistenceParameters::BinaryDataColumnSize(), largeBinaryDataColumn.get(), &largeBinaryDataColumnSize );

    Execute( hGetAllStatement );

    while (Fetch(hGetAllStatement))
    {
        const Safir::Dob::Typesystem::EntityId entityId
            (typeIdColumn, Safir::Dob::Typesystem::InstanceId(instanceColumn));

        Safir::Dob::Typesystem::ObjectPtr object;

        if (smallBinaryDataColumnSize != SQL_NULL_DATA)
        {
            const char * const data = reinterpret_cast<const char * const>(smallBinaryDataColumn.get());
            object = Safir::Dob::Typesystem::ObjectFactory::Instance().CreateObject(data);
        }
        else
        {
            if (largeBinaryDataColumnSize != SQL_NULL_DATA)
            {   //some binarypersistent data set
                const char * const data = reinterpret_cast<const char * const>(largeBinaryDataColumn.get());
                object = Safir::Dob::Typesystem::ObjectFactory::Instance().CreateObject(data);
            }
        }
        if (object != nullptr)
        {
            std::wstring xml = Safir::Dob::Typesystem::Serialization::ToXml(object);

            updateTypeIdParam = entityId.GetTypeId();
            updateInstanceParam = entityId.GetInstanceId().GetRawValue();

            const int size = (xml.size() + 1)* sizeof (wchar_t);
            memcpy(updateXmlDataParam.get(), xml.c_str(), size );
            updateXmlDataParamSize = SQL_NTS;

            Execute( hUpdateStatement );
        }
        else
        {
            //  m_debug << "No data set for " << objectId <<std::endl;
        }
    }
}


//-------------------------------------------------------
boost::filesystem::path GetStorageDirectory()
{
    try
    {
        boost::filesystem::path path = boost::filesystem::path(Safir::Dob::Typesystem::Utilities::ToUtf8(Safir::Dob::PersistenceParameters::FileStoragePath()));

        if (boost::filesystem::exists(path))
        {
            if (!boost::filesystem::is_directory(path))
            {
                throw Safir::Dob::Typesystem::SoftwareViolationException
                    (Safir::Dob::Typesystem::Utilities::ToWstring(path.string()) + L" is not a directory",__WFILE__,__LINE__);
            }
        }
        else
        {
            boost::filesystem::create_directory(path);
        }

        return path;
    }
    catch (const boost::filesystem::filesystem_error & e)
    {
        throw Safir::Dob::Typesystem::SoftwareViolationException
            (L"Failed to get hold of the directory for file persistence. Got this info from boost::filesystem::filesystem_error" +
            Safir::Dob::Typesystem::Utilities::ToWstring(e.what()),__WFILE__,__LINE__);
    }
}

typedef std::pair<Safir::Dob::Typesystem::EntityId, Safir::Dob::Typesystem::HandlerId> EntityIdAndHandlerId;

//-------------------------------------------------------
const EntityIdAndHandlerId
Filename2EntityIdAndHandlerId(const boost::filesystem::path & filename)
{
    if (!filename.has_filename())
    {
        throw Safir::Dob::Typesystem::IllegalValueException
            (L"Filename2EntityAndHandler: Could not decompose filename : " +
            Safir::Dob::Typesystem::Utilities::ToWstring(filename.string()),__WFILE__,__LINE__);
    }

    const std::string leaf = filename.filename().string();

    size_t separatorIndex = leaf.find('@');
    if (separatorIndex == std::string::npos)
    {
        throw Safir::Dob::Typesystem::IllegalValueException
            (L"Filename2EntityAndHandler: Could not decompose filename : " +
            Safir::Dob::Typesystem::Utilities::ToWstring(filename.string()),__WFILE__,__LINE__);
    }

    const std::string typeName = leaf.substr(0,separatorIndex);

    size_t secondSeparatorIndex = leaf.find('@',separatorIndex+1);

    if (secondSeparatorIndex == std::string::npos)
    {
        throw Safir::Dob::Typesystem::IllegalValueException
            (L"Filename2EntityAndHandler: Could not decompose filename : " +
            Safir::Dob::Typesystem::Utilities::ToWstring(filename.string()),__WFILE__,__LINE__);
    }
    const std::string instance = leaf.substr(separatorIndex + 1,secondSeparatorIndex - separatorIndex - 1);

    size_t extIndex = leaf.find('.',secondSeparatorIndex);

    if (extIndex == std::string::npos)
    {
        throw Safir::Dob::Typesystem::IllegalValueException
            (L"Filename2EntityAndHandler: Could not decompose filename : " +
            Safir::Dob::Typesystem::Utilities::ToWstring(filename.string()),__WFILE__,__LINE__);
    }
    const std::string handler = leaf.substr(secondSeparatorIndex + 1,extIndex - secondSeparatorIndex - 1);

    return std::make_pair(Safir::Dob::Typesystem::EntityId
        (Safir::Dob::Typesystem::Operations::GetTypeId(Safir::Dob::Typesystem::Utilities::ToWstring(typeName)),
        Safir::Dob::Typesystem::InstanceId( boost::lexical_cast<Safir::Dob::Typesystem::Int64>(instance))),
        Safir::Dob::Typesystem::HandlerId(boost::lexical_cast<Safir::Dob::Typesystem::Int64>(handler)));
}

//-------------------------------------------------------
boost::filesystem::path
EntityId2Filename(const EntityIdAndHandlerId& entityAndHandler,
                  const std::string & extension)
{
    std::ostringstream out;
    out << Safir::Dob::Typesystem::Utilities::ToUtf8(Safir::Dob::Typesystem::Operations::GetName(entityAndHandler.first.GetTypeId()))
        << "@"
        << entityAndHandler.first.GetInstanceId().GetRawValue()
        << "@"
        << entityAndHandler.second.GetRawValue()
        << extension;
    return out.str();
}


void ConvertFiles()
{
    Safir::Dob::Typesystem::BinarySerialization bin;
    boost::filesystem::path storagePath = GetStorageDirectory();

    for (boost::filesystem::directory_iterator it (storagePath);
         it != boost::filesystem::directory_iterator(); ++it)
    {
        const boost::filesystem::path path = it->path();
        const EntityIdAndHandlerId id = Filename2EntityIdAndHandlerId(*it);

        if (path.extension() == ".bin")
        {
            const size_t fileSize = static_cast<size_t>(boost::filesystem::file_size(path));
            if (fileSize == 0)
            {
                continue;
            }
            bin.resize(fileSize);

            boost::filesystem::ifstream file(path, std::ios::in | std::ios::binary);
            file.read(&bin[0],fileSize);

            const Safir::Dob::Typesystem::ObjectPtr object = Safir::Dob::Typesystem::Serialization::ToObject(bin);

            const std::wstring xml = Safir::Dob::Typesystem::Serialization::ToXml(object);
            boost::filesystem::path xmlFileName(path);
            xmlFileName.replace_extension(".xml");
            boost::filesystem::wofstream xmlFile(xmlFileName);
            xmlFile << xml;
        }
    }
}

void AllocStatement(SQLHSTMT * hStmt, SQLHDBC hConnection)
{
    SQLRETURN ret = ::SQLAllocHandle(SQL_HANDLE_STMT, hConnection, hStmt);
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_STMT,hStmt);
    }
}

void BindColumnInt64( SQLHSTMT hStmt,
                      unsigned short usColumnNumber,
                      Safir::Dob::Typesystem::Int64 * value,
                      SQLINTEGER * size)
{
    SQLRETURN ret;

    ret = ::SQLBindCol( hStmt,                                  // StatementHandle
                        usColumnNumber,                         // ColumnNumber,
                        SQL_C_SBIGINT,                          // TargetType,
                        value,                                  // TargetValuePtr,
                        sizeof(Safir::Dob::Typesystem::Int64),  // BufferLength,
                        size);                                  // StrLen_or_Ind
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_STMT,hStmt);
    }
}

void BindColumnBinary( SQLHSTMT hStmt,
                       unsigned short usColumnNumber,
                       const int maxSize,
                       unsigned char * buffer,
                       SQLINTEGER * sizePtr )
{
    SQLRETURN ret;

    ret = ::SQLBindCol( hStmt,                      // StatementHandle
                        usColumnNumber,             // ColumnNumber,
                        SQL_C_BINARY,               // TargetType,
                        buffer,                     // TargetValuePtr,
                        maxSize,                    // BufferLength,
                        sizePtr);                   // StrLen_or_Ind
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_STMT,hStmt);
    }
}

void BindParamInt64(SQLHSTMT hStmt, const SQLUSMALLINT paramNumber, Safir::Dob::Typesystem::Int64 * value, SQLINTEGER * size)
{
    SQLRETURN ret = ::SQLBindParameter(
        hStmt,                                  // StatementHandle
        paramNumber,                            // ParameterNumber,
        SQL_PARAM_INPUT,                        // InputOutputType
        SQL_C_SBIGINT,                          // ValueType
        SQL_BIGINT,                             // ParameterType
        20,                                     // ColumnSize
        0,                                      // DecimalDigits
        value,                                  // ParameterValuePtr
        sizeof(Safir::Dob::Typesystem::Int64),  // BufferLength
        size );                                 // StrLen_or_Ind
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_STMT,hStmt);
    }
}

void BindParamString(SQLHSTMT hStmt,
                     const SQLUSMALLINT paramNumber,
                     const SQLUINTEGER maxSize,
                     wchar_t * string,
                     SQLINTEGER * sizePtr)
{
    const SQLUINTEGER number_of_chars = static_cast<SQLUINTEGER>( maxSize ) + 1;
    const SQLUINTEGER size_of_char = static_cast<SQLUINTEGER>(sizeof(wchar_t));
    const SQLUINTEGER columnSize = number_of_chars * size_of_char;

    SQLRETURN ret = ::SQLBindParameter(
        hStmt,                          // StatementHandle
        paramNumber,                    // ParameterNumber,
        SQL_PARAM_INPUT,                // InputOutputType
        SQL_C_WCHAR,                    // ValueType
        SQL_WLONGVARCHAR,               // ParameterType
        //SQL_WVARCHAR,                   // ParameterType
        columnSize,                     // ColumnSize
        0,                              // DecimalDigits
        string,                         // ParameterValuePtr
        maxSize,                        // BufferLength
        sizePtr );                      // StrLen_or_Ind
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_STMT,hStmt);
    }
}


void Connect(SQLHDBC hConnection, const std::wstring & connectionString)
{
    //m_debug << L"Connecting to database"    << std::endl;

    SQLRETURN ret = ::SQLDriverConnectW(
        hConnection,
        NULL,
        const_cast<wchar_t *>(connectionString.c_str()),
        SQL_NTS,
        NULL,
        0,
        NULL,
        SQL_DRIVER_NOPROMPT );
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_DBC, hConnection );
    }

    // SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_ON);
    ret = ::SQLSetConnectAttr(  hConnection,
                                SQL_ATTR_AUTOCOMMIT,
                                reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON),
                                SQL_IS_UINTEGER );
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_DBC, hConnection );
    }
}

void Execute(SQLHSTMT hStmt)
{
    SQLRETURN ret = ::SQLExecute( hStmt );
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_STMT,hStmt);
    }
}

bool Fetch( SQLHSTMT hStmt )
{
    SQLRETURN ret;
    bool bDataFound = true;

    ret = ::SQLFetch( hStmt );
    if (ret==SQL_NO_DATA_FOUND)
    {
        bDataFound = false;
    }
    else if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_STMT,hStmt);
    }
    return bDataFound;
}

void Prepare(SQLHSTMT hStmt, const std::wstring & sql)
{
    // const_cast is used because StatementText is declared as input in the ODBC
    // specification and should be a const wchar_t *.
    SQLRETURN ret = ::SQLPrepareW(
        hStmt,
        const_cast<wchar_t *>(sql.c_str()),
        SQL_NTS );
    if (!SQL_SUCCEEDED(ret))
    {
        ThrowException(SQL_HANDLE_STMT,hStmt);
    }
}

void ThrowException(SQLSMALLINT   HandleType,
                    SQLHANDLE     Handle)
{
    wchar_t     wszSqlState[6];
    SQLINTEGER  lpNativeErrorPtr;
    wchar_t     wszMessageText[512];
    SQLRETURN   ret;

    ret = ::SQLGetDiagRecW( HandleType,
                            Handle,
                            1,
                            wszSqlState,
                            &lpNativeErrorPtr,
                            wszMessageText,
                            256,
                            0 );
    if (SQL_SUCCEEDED(ret))
    {
        std::wstring string = wszSqlState;
        string += L":";
        string += wszMessageText;

        throw std::exception(Safir::Dob::Typesystem::Utilities::ToUtf8(string).c_str());
    }
}

int main(int argc, char * argv[])
{
    try
    {
        ParseCommandLine(argc,argv);
        if (g_whatToConvert == db)
        {
            ConvertDb();
        }
        else if (g_whatToConvert == files)
        {
            ConvertFiles();
        }
    }
    catch (const std::exception & exc)
    {
        std::wcout << "Caught exception: " << exc.what() << std::endl;
        return 1;
    }
    return 0;
}
