//******************************************************************************************************
//  Convert.h - Gbtc
//
//  Copyright Â© 2019, Grid Protection Alliance.  All Rights Reserved.
//
//  Licensed to the Grid Protection Alliance (GPA) under one or more contributor license agreements. See
//  the NOTICE file distributed with this work for additional information regarding copyright ownership.
//  The GPA licenses this file to you under the MIT License (MIT), the "License"; you may not use this
//  file except in compliance with the License. You may obtain a copy of the License at:
//
//      http://opensource.org/licenses/MIT
//
//  Unless agreed to in writing, the subject software distributed under the License is distributed on an
//  "AS-IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. Refer to the
//  License for the specific language governing permissions and limitations.
//
//  Code Modification History:
//  ----------------------------------------------------------------------------------------------------
//  04/06/2012 - Stephen C. Wills
//       Generated original version of source code.
//
//******************************************************************************************************

#pragma once

#include "CommonTypes.h"

namespace sttp
{
    // Converts a timestamp, in Ticks, to Unix second of century and milliseconds
    void ToUnixTime(int64_t ticks, time_t& unixSOC, uint16_t& milliseconds);

    // Convert Unix second of century and milliseconds to DateTime
    datetime_t FromUnixTime(time_t unixSOC, uint16_t milliseconds);

    // Convert Unix second of century and microseconds to DateTime
    datetime_t FromUnixTimeMicro(time_t unixSOC, uint32_t microseconds);

    // Converts a timestamp, in Ticks, to DateTime
    datetime_t FromTicks(int64_t ticks);

    // Converts a DateTime to Ticks
    int64_t ToTicks(const datetime_t& time);

    // Determines if the deserialized Ticks value represents a leap second, i.e., second 60.
    bool IsLeapSecond(int64_t ticks);

    // Flags a Ticks value to represent a leap second, i.e., second 60, before wire serialization.
    void SetLeapSecond(int64_t& ticks);

    // Determines if the deserialized Ticks value represents a negative leap second, i.e., checks flag on second 58 to see if second 59 will be missing.
    bool IsNegativeLeapSecond(int64_t ticks);

    // Flags a Ticks value to represent a negative leap second, i.e., sets flag on second 58 to mark that second 59 will be missing, before wire serialization.
    void SetNegativeLeapSecond(int64_t& ticks);

    // Determines if timestamp, in ticks, is reasonable as compared to local clock within specified tolerances, in seconds
    // lagTime and leadTime must be greater than zero, but can be less than one.
    bool TimestampIsReasonable(int64_t value, float64_t lagTime = 5.0, float64_t leadTime = 5.0, bool utc = true);

    // Determines if timestamp is reasonable as compared to local clock within specified tolerances, in seconds
    // lagTime and leadTime must be greater than zero, but can be less than one.
    bool TimestampIsReasonable(const datetime_t& value, float64_t lagTime = 5.0, float64_t leadTime = 5.0, bool utc = true);

    // Returns the nearest sub-second distribution timestamp, in ticks, for provided timestamp, in ticks.
    int64_t RoundToSubsecondDistribution(int64_t ticks, int32_t samplesPerSecond);

    // Thin wrapper around strftime to provide formats for milliseconds (%f) and full-resolution ticks (%t)
    uint32_t TicksToString(char* ptr, uint32_t maxsize, std::string format, int64_t ticks);

    datetime_t LocalFromUtc(const datetime_t& timestamp);

    // Converts an object to a string
    template<class T>
    std::string ToString(const T& obj)
    {
        std::stringstream stream;
        stream << obj;
        return stream.str();
    }

    std::string ToString(const Guid& value);

    std::string ToString(const datetime_t& value, const char* format = "%Y-%m-%d %H:%M:%S%F");

    std::string ToString(const TimeSpan& value);

    std::string ToString(const decimal_t& value);

    std::wstring ToUTF16(const std::string& value);

    std::string ToUTF8(const std::wstring& value);

    // Converts an integer value to a hex representation
    template<class T>
    std::string ToHex(const T& value)
    {
        std::stringstream stream;
        stream << "0x" << std::hex << std::uppercase << static_cast<int32_t>(value);
        return stream.str();
    }

    bool ParseBoolean(const std::string& value);

    bool TryParseBoolean(const std::string& value, bool& result, bool defaultValue = false);

    bool IsInteger(const std::string& value);

    bool IsNumeric(const std::string& value);

    bool TryParseUInt16(const std::string& value, uint16_t& result, uint16_t defaultValue = 0);

    bool TryParseInt32(const std::string& value, int32_t& result, int32_t defaultValue = 0);

    bool TryParseUInt32(const std::string& value, uint32_t& result, uint32_t defaultValue = 0U);

    bool TryParseInt64(const std::string& value, int64_t& result, int64_t defaultValue = 0LL);
    
    bool TryParseUInt64(const std::string& value, uint64_t& result, uint64_t defaultValue = 0ULL);

    bool TryParseDouble(const std::string& value, float64_t& result, float64_t defaultValue = 0.0);

    bool TryParseDecimal(const std::string& value, decimal_t& result, decimal_t defaultValue = decimal_t(0));

    // Encodes a character value into an escaped RegEx value
    std::string RegExEncode(char value);

    bool IsGuid(const std::string& value);

    // Converts 16 contiguous bytes of character data into a globally unique identifier
    Guid ParseGuid(const uint8_t* data, bool swapEndianness = false);

    Guid ParseGuid(const char* data);

    bool TryParseGuid(const std::string& value, Guid& result, Guid defaultValue = Empty::Guid);
    
    // Convert RFC encoding to Microsoft or vice versa
    void SwapGuidEndianness(Guid& value);

    // Returns a non-empty nor null value
    const char* Coalesce(const char* data, const char* nonEmptyValue);

    // Attempts to parse an time string in several common formats
    bool TryParseTimestamp(const char* time, datetime_t& timestamp, const datetime_t& defaultValue = DateTime::MinValue, bool parseAsUTC = true);

    // Converts a string to a date-time that may be in several common formats
    datetime_t ParseTimestamp(const char* time, bool parseAsUTC = true);

    // Parses a string formatted as an absolute or relative timestamp where relative
    // times are parsed based on an offset to current time (UTC) specified by an "*"
    // and an offset interval with a time unit suffix of "s" for seconds, "m" for
    // minutes, "h" for hours or "d" for days, see examples:
    //
    //  Time Format Example      Description
    //  -----------------------  ---------------------------------------
    //  12-30-2000 23:59:59.033  Absolute date and time
    //  *                        Evaluates to UtcNow()
    //  *-20s                    Evaluates to 20 seconds before UtcNow()
    //  *-10m                    Evaluates to 10 minutes before UtcNow()
    //  *-1h                     Evaluates to 1 hour before UtcNow()
    //  *-1d                     Evaluates to 1 date before UtcNow()
    //  *+2d                     Evaluates to 2 days after UtcNow()
    //
    // If function fails to parse a valid timestamp, returns defaultValue
    datetime_t ParseRelativeTimestamp(const char* time, const datetime_t& defaultValue = DateTime::MaxValue);

    // Parses a string of key/value pairs into a case-insensitive string dictionary
    StringMap<std::string> ParseKeyValuePairs(const std::string& value, char parameterDelimiter = ';', char keyValueDelimiter = '=', char startValueDelimiter = '{', char endValueDelimiter = '}');

    // Returns a DNS resolved host name and port for the specified end point
    std::string ResolveDNSName(IOContext& service, const TcpEndPoint& source);

    // Returns a DNS resolved host name and port for the specified end point.
    // Out parameter hostName is set to the DNS host name, if resolvable,
    // otherwise will be set to the IP address of the end point.
    std::string ResolveDNSName(IOContext& service, const TcpEndPoint& source, std::string& hostName);
}
