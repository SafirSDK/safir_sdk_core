/******************************************************************************
*
* Copyright Saab AB, 2006-2008 (http://www.safirsdk.com)
*
* Created by: Erik Adolfsson / sterad
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
#include <Safir/Dob/Typesystem/Defs.h>
#include <boost/date_time/time_duration.hpp>
#include <boost/date_time/gregorian/greg_date.hpp>
#include <Safir/Time/TimeProvider.h>
#include <Safir/Time/Internal/Interface.h>

using namespace boost::date_time;

namespace Safir
{
namespace Time
{
    const boost::posix_time::ptime _1_JAN_1970 (boost::gregorian::date(1970,Jan,1));

    //---------------------------------------------------------------------------
    Safir::Dob::Typesystem::Si64::Second TimeProvider::GetUtcTime()
    {
        Safir::Dob::Typesystem::Si64::Second utcTime;
        DoufTimeC_GetUtcTime(utcTime);
        return utcTime;
    }

    //---------------------------------------------------------------------------
    boost::posix_time::ptime TimeProvider::ToLocalTime(const Safir::Dob::Typesystem::Si64::Second utcTime)
    {
        Safir::Dob::Typesystem::Int32 offset;
        DoufTimeC_GetLocalTimeOffset(offset);

        // Convert seconds to localtime
        Dob::Typesystem::Si64::Second localTime = utcTime + offset;

        return ToPtime(localTime);
    }

    //---------------------------------------------------------------------------
    Safir::Dob::Typesystem::Si64::Second TimeProvider::ToUtcTime(const boost::posix_time::ptime& localTime)
    {
        boost::posix_time::time_duration duration = localTime - _1_JAN_1970;
        Dob::Typesystem::Int64 seconds = duration.total_seconds();
        Dob::Typesystem::Float64 fraction = duration.fractional_seconds() / pow(10.0,duration.num_fractional_digits());

        Safir::Dob::Typesystem::Int32 offset;
        DoufTimeC_GetLocalTimeOffset(offset);

        return seconds + fraction - offset;
    }

    //---------------------------------------------------------------------------
    boost::posix_time::ptime TimeProvider::ToPtime(const Safir::Dob::Typesystem::Si64::Second utcTime)
    {
        boost::posix_time::ptime t = _1_JAN_1970; // Set time to 1970-Jan-01 (UTC start)

        Dob::Typesystem::Int64 sec;
        Dob::Typesystem::Float64 fraction;

        sec = (Dob::Typesystem::Int64)utcTime;
        fraction = utcTime - sec;

        long long hour    = sec/3600;
        long long minutes = (sec - hour*3600)/60;
        long long seconds = sec - hour*3600 - minutes*60;

        // Return the duration since 1970-Jan-01
        boost::posix_time::time_duration duration((long)hour, (long)minutes, (long)seconds,
            Dob::Typesystem::Int64(fraction * pow(10.0,boost::posix_time::time_duration::num_fractional_digits()) + 0.5));

        return _1_JAN_1970 + duration;
    };

    //---------------------------------------------------------------------------
    Safir::Dob::Typesystem::Si64::Second TimeProvider::ToDouble(const boost::posix_time::ptime& utcTime)
    {
        boost::posix_time::time_duration d = utcTime - _1_JAN_1970;
        return(double)d.ticks() / d.ticks_per_second();
    }

}; // namespace Time
}; // namespace Safir

