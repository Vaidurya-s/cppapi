//******************************************************************************************************
//  CommonTypes.h - Gbtc
//
//  Copyright © 2019, Grid Protection Alliance.  All Rights Reserved.
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
//  03/09/2012 - Stephen C. Wills
//       Generated original version of source code.
//
//******************************************************************************************************

// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
// ReSharper disable CppClangTidyBugproneReservedIdentifier
#pragma once

#include <climits>
#include <cstddef>
#include <map>
#include <unordered_map>
#include <string>

// Reduce boost code analysis warnings
#pragma warning(push)
#pragma warning(disable:4068)
#pragma warning(disable:6001)
#pragma warning(disable:6031)
#pragma warning(disable:6255)
#pragma warning(disable:6258)
#pragma warning(disable:6386)
#pragma warning(disable:6387)
#pragma warning(disable:26439)
#pragma warning(disable:26444)
#pragma warning(disable:26451)
#pragma warning(disable:26495)
#pragma warning(disable:26498)
#pragma warning(disable:26812)
#pragma warning(disable:26819)

#if defined(_MSC_VER)
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma diag_suppress 70
#pragma diag_suppress 1440
#endif

#include <boost/any.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/exception/exception.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>

// Fix boost v1.75 duplicate symbol issue for MSVC
#if defined(_MSC_VER) && BOOST_VERSION == 107500
#include "boost/use_awaitable.hpp"
#endif

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/device/array.hpp>

#pragma warning(pop)

#if _WIN32 || _WIN64
#if _WIN64
#define _32BIT 0
#define _64BIT 1
#else
#define _32BIT 1
#define _64BIT 0
#endif
#endif

#if __GNUC__ // NOLINT
#if __x86_64__ || __ppc64__
#define _32BIT 0
#define _64BIT 1
#else
#define _32BIT 1
#define _64BIT 0
#endif
#endif

#define BOOST_LEGACY (BOOST_VERSION < 106600)

namespace sttp
{
    static_assert(sizeof(float) * CHAR_BIT == 32, "float not defined as 32-bits");
    static_assert(sizeof(double) * CHAR_BIT == 64, "double not defined as 64-bits");

    // Floating-point types
    typedef float float32_t;
    typedef double float64_t;
    typedef boost::multiprecision::cpp_dec_float_100 decimal_t;
    static const float64_t NaN = std::numeric_limits<double>::quiet_NaN();

    // DateTime type
    typedef boost::posix_time::ptime datetime_t;

    struct Int8
    {
        static constexpr int8_t MaxValue = static_cast<int8_t>(127);
        static constexpr int8_t MinValue = static_cast<int8_t>(-128);
    };

    struct UInt8
    {
        static constexpr uint8_t MaxValue = static_cast<uint8_t>(255);
        static constexpr uint8_t MinValue = static_cast<uint8_t>(0);
    };

    struct Int16
    {
        static constexpr int16_t MaxValue = static_cast<int16_t>(32767);
        static constexpr int16_t MinValue = static_cast<int16_t>(-32768);
    };

    struct UInt16
    {
        static constexpr uint16_t MaxValue = static_cast<uint16_t>(65535);
        static constexpr uint16_t MinValue = static_cast<uint16_t>(0);
    };

    struct Int32
    {
        static constexpr int32_t MaxValue = static_cast<int32_t>(2147483647);
        static constexpr int32_t MinValue = static_cast<int32_t>(-2147483647) - 1;
    };

    struct UInt32
    {
        static constexpr uint32_t MaxValue = static_cast<uint32_t>(4294967295U);
        static constexpr uint32_t MinValue = static_cast<uint32_t>(0U);
    };

    struct Int64
    {
        static constexpr int64_t MaxValue = static_cast<int64_t>(9223372036854775807LL);
        static constexpr int64_t MinValue = static_cast<int64_t>(-9223372036854775807LL) - 1LL;
    };

    struct UInt64
    {
        static constexpr uint64_t MaxValue = static_cast<uint64_t>(18446744073709551615ULL);
        static constexpr uint64_t MinValue = static_cast<uint64_t>(0ULL);
    };

    struct Decimal
    {
        // ReSharper disable CppVariableCanBeMadeConstexpr
        static const decimal_t MaxValue;
        static const decimal_t MinValue;
        
        static const decimal_t DotNetMaxValue;
        static const decimal_t DotNetMinValue;
        // ReSharper restore CppVariableCanBeMadeConstexpr
    };

    struct DateTime
    {
        static const datetime_t MaxValue;
        static const datetime_t MinValue;
    };

    struct Ticks
    {
        static constexpr int64_t MaxValue = 3155378975999999999LL;       // 12/31/1999 11:59:59.999
        static constexpr int64_t MinValue = 0LL;                         // 01/01/0001 00:00:00.000

        static constexpr int64_t UnixBaseOffset = 621355968000000000LL;  // 01/01/1970 00:00:00.000
        static constexpr int64_t PTimeBaseOffset = 441481536000000000LL; // 01/01/1400 00:00:00.000

        static constexpr int64_t PerSecond = 10000000LL;
        static constexpr int64_t PerMillisecond = Ticks::PerSecond / 1000LL;
        static constexpr int64_t PerMicrosecond = Ticks::PerSecond / 1000000LL;
        static constexpr int64_t PerMinute = 60LL * Ticks::PerSecond;
        static constexpr int64_t PerHour = 60LL * Ticks::PerMinute;
        static constexpr int64_t PerDay = 24LL * Ticks::PerHour;

        // Flag (64th bit) that marks a Ticks value as a leap second, i.e., second 60 (one beyond normal second 59).
        static constexpr int64_t LeapSecondFlag = 1LL << 63;  // NOLINT(clang-diagnostic-shift-sign-overflow)

        // Flag (63rd bit) that indicates if leap second is positive or negative; 0 for add, 1 for delete.
        static constexpr int64_t LeapSecondDirection = 1LL << 62;

        static constexpr int64_t ValueMask = ~LeapSecondFlag & ~LeapSecondDirection;
    };

    inline int32_t ConvertInt32(const size_t value)
    {
    #if _64BIT
        if (value > static_cast<size_t>(Int32::MaxValue))
            throw std::runtime_error("Cannot cast size_t value to int32_t: value is out of range");
    #endif

        return static_cast<int32_t>(value);
    }

    inline uint32_t ConvertUInt32(const size_t value)
    {
    #if _64BIT
        if (value > static_cast<size_t>(UInt32::MaxValue))
            throw std::runtime_error("Cannot cast size_t value to uint32_t: value is out of range");
    #endif

        return static_cast<uint32_t>(value);
    }

    template<class T>
    using SharedPtr = boost::shared_ptr<T>;

    template<class T>
    using EnableSharedThisPtr = boost::enable_shared_from_this<T>;

    template<class T>
    SharedPtr<T> NewSharedPtr()
    {
        return boost::make_shared<T>();
    }

    template<class T, class P1>
    SharedPtr<T> NewSharedPtr(P1 p1)
    {
        return boost::make_shared<T>(p1);
    }

    template<class T, class P1, class P2>
    SharedPtr<T> NewSharedPtr(P1 p1, P2 p2)
    {
        return boost::make_shared<T>(p1, p2);
    }

    template<class T, class P1, class P2, class P3>
    SharedPtr<T> NewSharedPtr(P1 p1, P2 p2, P3 p3)
    {
        return boost::make_shared<T>(p1, p2, p3);
    }

    template<class T, class P1, class P2, class P3, class P4>
    SharedPtr<T> NewSharedPtr(P1 p1, P2 p2, P3 p3, P4 p4)
    {
        return boost::make_shared<T>(p1, p2, p3, p4);
    }

    template<class T, class P1, class P2, class P3, class P4, class P5>
    SharedPtr<T> NewSharedPtr(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
    {
        return boost::make_shared<T>(p1, p2, p3, p4, p5);
    }

    template<class T, class S>
    SharedPtr<T> CastSharedPtr(SharedPtr<S> const& source)
    {
        return boost::dynamic_pointer_cast<T>(source);
    }

    typedef boost::any Object;
    typedef boost::uuids::uuid Guid;
    typedef boost::posix_time::time_duration TimeSpan;
    typedef boost::posix_time::milliseconds Milliseconds;
    typedef boost::posix_time::microseconds Microseconds;
    typedef boost::system::error_code ErrorCode;
    typedef boost::system::system_error SystemError;
    typedef boost::exception Exception;
    typedef boost::thread Thread;
    typedef boost::mutex Mutex;
    typedef boost::shared_mutex SharedMutex;
    typedef boost::condition_variable WaitHandle;
    typedef boost::lock_guard<Mutex> ScopeLock;
    typedef boost::unique_lock<Mutex> UniqueLock;
    typedef boost::unique_lock<SharedMutex> WriterLock;
    typedef boost::shared_lock<SharedMutex> ReaderLock;
    #if BOOST_LEGACY
    typedef boost::asio::io_service IOContext;
    typedef boost::asio::io_service::strand Strand;
    #else
    typedef boost::asio::io_context IOContext;
    typedef boost::asio::io_context::strand Strand;
    #endif
    typedef boost::asio::steady_timer SteadyTimer;
    typedef boost::asio::ip::address IPAddress;
    typedef boost::asio::ip::tcp::socket TcpSocket;
    typedef boost::asio::ip::udp::socket UdpSocket;
    typedef boost::asio::ip::tcp::acceptor TcpAcceptor;
    typedef boost::asio::ip::tcp::endpoint TcpEndPoint;
    typedef boost::asio::ip::tcp::resolver DnsResolver;
    typedef boost::iostreams::filtering_streambuf<boost::iostreams::input> StreamBuffer;
    typedef boost::iostreams::gzip_decompressor GZipDecompressor;
    typedef boost::iostreams::gzip_compressor GZipCompressor;

    typedef SharedPtr<Thread> ThreadPtr;
    static const ThreadPtr ThreadNullPtr;
    
    #define ThreadSleep(ms) boost::this_thread::sleep(boost::posix_time::milliseconds(ms))

    #if BOOST_LEGACY
    #define bind_executor(ex, ...) ex.wrap(__VA_ARGS__)
    #define post(ex, ...) ex.post(__VA_ARGS__)
    #endif

    // Empty types
    struct Empty
    {
        static const std::string String;
        static const sttp::datetime_t DateTime;
        static const sttp::Guid Guid;
        static const sttp::Object Object;
        static const sttp::IPAddress IPAddress;
    };

    // std::unordered_map string hasher
    struct StringHash : std::unary_function<std::string, size_t>
    {
        size_t operator()(const std::string& value) const;
    };

    // std::unordered_map string equality tester
    struct StringEqual : std::binary_function<std::string, std::string, bool>
    {
        bool operator()(const std::string& left, const std::string& right) const;
    };

    template<class T>
    using StringMap = std::unordered_map<std::string, T, StringHash, StringEqual>;

    // std::map string comparer
    struct StringComparer : std::binary_function<std::string, std::string, bool>
    {
        bool operator()(const std::string& left, const std::string& right) const;
    };

    template<class T>
    using SortedStringMap = std::map<std::string, T, StringComparer>;

    template<class TKey, class TValue>
    bool TryGetValue(const std::map<TKey, TValue>& dictionary, const TKey& key, TValue& value, const TValue& defaultValue)
    {
        auto iterator = dictionary.find(key);

        if (iterator != dictionary.end())
        {
            value = iterator->second;
            return true;
        }

        value = defaultValue;
        return false;
    }

    template<class TKey, class TValue>
    bool TryGetValue(const std::unordered_map<TKey, TValue>& dictionary, const TKey& key, TValue& value, const TValue& defaultValue)
    {
        auto iterator = dictionary.find(key);

        if (iterator != dictionary.end())
        {
            value = iterator->second;
            return true;
        }

        value = defaultValue;
        return false;
    }

    template<class TValue>
    bool TryGetValue(const StringMap<TValue>& dictionary, const std::string& key, TValue& value, const TValue& defaultValue)
    {
        auto iterator = dictionary.find(key);

        if (iterator != dictionary.end())
        {
            value = iterator->second;
            return true;
        }

        value = defaultValue;
        return false;
    }

    inline bool TryGetValue(const StringMap<std::string>& dictionary, const std::string& key, std::string& value)
    {
        return TryGetValue<std::string>(dictionary, key, value, Empty::String);
    }

    template<class T>
    T Cast(const Object& source)
    {
        return boost::any_cast<T>(source);
    }

    struct MemoryStream : boost::iostreams::array_source
    {
        MemoryStream(const std::vector<uint8_t>& buffer) : boost::iostreams::array_source(reinterpret_cast<const char*>(buffer.data()), buffer.size())
        {
        }

        MemoryStream(const uint8_t* buffer, const uint32_t offset, const uint32_t length) : boost::iostreams::array_source(reinterpret_cast<const char*>(buffer + offset), length)
        {
        }
    };

    template<class T, class TElem = char>
    void CopyStream(T* source, std::vector<uint8_t>& sink)
    {
        std::istreambuf_iterator<TElem> it{ source };
        std::istreambuf_iterator<TElem> eos{};

        for (; it != eos; ++it)
            sink.push_back(static_cast<uint8_t>(*it));
    }

    template<class T, class TElem = char>
    void CopyStream(T& source, std::vector<uint8_t>& sink)
    {
        std::istreambuf_iterator<TElem> it{ source };
        std::istreambuf_iterator<TElem> eos{};

        for (; it != eos; ++it)
            sink.push_back(static_cast<uint8_t>(*it));
    }

    template<class T>
    static uint32_t WriteBytes(std::vector<uint8_t>& buffer, const T& value)
    {
        static const int32_t length = sizeof(T);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);

        for (int32_t i = 0; i < length; i++)
            buffer.push_back(bytes[i]);

        return length;
    }

    uint32_t WriteBytes(std::vector<uint8_t>& buffer, const uint8_t* source, uint32_t offset, uint32_t length);
    uint32_t WriteBytes(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& source);
    uint32_t WriteBytes(std::vector<uint8_t>& buffer, const Guid& value);

    Guid NewGuid();

    // Handy string functions (boost wrappers)
    bool IsEmptyOrWhiteSpace(const std::string& value);
    bool IsEqual(const std::string& left, const std::string& right, bool ignoreCase = true);
    bool StartsWith(const std::string& value, const std::string& findValue, bool ignoreCase = true);
    bool EndsWith(const std::string& value, const std::string& findValue, bool ignoreCase = true);
    bool Contains(const std::string& value, const std::string& findValue, bool ignoreCase = true);
    int32_t Count(const std::string& value, const std::string& findValue, bool ignoreCase = true);
    int32_t Compare(const std::string& leftValue, const std::string& rightValue, bool ignoreCase = true);
    int32_t IndexOf(const std::string& value, const std::string& findValue, bool ignoreCase = true);
    int32_t IndexOf(const std::string& value, const std::string& findValue, int32_t index, bool ignoreCase = true);
    int32_t LastIndexOf(const std::string& value, const std::string& findValue, bool ignoreCase = true);
    std::vector<std::string> Split(const std::string& value, const std::string& delimiterValue, bool ignoreCase = true);
    std::string Split(const std::string& value, const std::string& delimiterValue, int32_t index, bool ignoreCase = true);
    std::string Replace(const std::string& value, const std::string& findValue, const std::string& replaceValue, bool ignoreCase = true);
    std::string ToUpper(const std::string& value);
    std::string ToLower(const std::string& value);
    std::string Trim(const std::string& value);
    std::string Trim(const std::string& value, const std::string& trimValues);
    std::string TrimRight(const std::string& value);
    std::string TrimRight(const std::string& value, const std::string& trimValues);
    std::string TrimLeft(const std::string& value);
    std::string TrimLeft(const std::string& value, const std::string& trimValues);
    std::string PadLeft(const std::string& value, uint32_t count, char padChar);
    std::string PadRight(const std::string& value, uint32_t count, char padChar);
    
    #ifndef __STDC_WANT_SECURE_LIB__
    #define strcpy_s(dest, size, src) strncpy(dest, src, size)
    #endif

    // Handy date/time functions (boost wrappers)
    enum class TimeInterval
    {
        Year,
        Month,
        DayOfYear,
        Day,
        Week,
        WeekDay,
        Hour,
        Minute,
        Second,
        Millisecond
    };

    datetime_t DateAdd(const datetime_t& value, int32_t addValue, TimeInterval interval);
    int32_t DateDiff(const datetime_t& startTime, const datetime_t& endTime, TimeInterval interval);
    int32_t DatePart(const datetime_t& value, TimeInterval interval);
    datetime_t Now();
    datetime_t UtcNow();
    float32_t TimeSince(const datetime_t& value);
}

// Note: std::hash<boost::uuids::uuid> is already provided by Boost UUID library
