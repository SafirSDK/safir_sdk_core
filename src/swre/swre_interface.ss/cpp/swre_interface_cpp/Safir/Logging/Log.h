/******************************************************************************
*
* Copyright Saab AB, 2013 (http://www.safirsdk.com)
*
* Created by: Anders Widén
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
#ifndef __LOG_H
#define __LOG_H

#include <Safir/Application/Internal/SwReportExportDefs.h>
#include <Safir/Logging/Severity.h>
#include <string>

namespace Safir
{
namespace Logging
{

    /**
     * Service for sending log messages to the native system logging mechanism.
     *
     * The service takes a severity and an arbitrary string.
     * The severity levels conforms to the ones used by the well known syslog format as specified
     * in http://www.ietf.org/rfc/rfc3164.txt.
     *
     * @param [in] severity Severity according to RFC 3164.
     * @param [in] logMsg Log text.
     */
    SWRE_API void SendSystemLog(const Safir::Logging::Severity::Enumeration severity,
                                const std::wstring&                         logMsg);

}
}

#endif
