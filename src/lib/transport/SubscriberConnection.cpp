//******************************************************************************************************
//  SubscriberConnection.cpp - Gbtc
//
//  Copyright � 2019, Grid Protection Alliance.  All Rights Reserved.
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
//  02/07/2019 - J. Ritchie Carroll
//       Generated original version of source code.
//
//******************************************************************************************************

#include "../filterexpressions/FilterExpressionParser.h"
#include "SubscriberConnection.h"
#include "DataPublisher.h"
#include "CompactMeasurement.h"
#include "ActiveMeasurementsSchema.h"
#include "../EndianConverter.h"
#include "../data/DataSet.h"

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace sttp;
using namespace sttp::data;
using namespace sttp::filterexpressions;
using namespace sttp::transport;

static constexpr uint32_t MaxPacketSize = 32768U;
static constexpr float64_t DefaultLagTime = 10.0;
static constexpr float64_t DefaultLeadTime = 5.0;
static constexpr float64_t DefaultPublishInterval = 1.0;

const SubscriberConnectionPtr SubscriberConnection::NullPtr = nullptr;

SubscriberConnection::SubscriberConnection(DataPublisherPtr parent, IOContext& commandChannelService) : //NOLINT
    m_parent(std::move(parent)),
    m_commandChannelService(commandChannelService),
    m_tcpWriteStrand(commandChannelService),
    m_subscriberID(NewGuid()),
    m_instanceID(NewGuid()),
    m_operationalModes(OperationalModes::NoFlags),
    m_encoding(OperationalEncoding::UTF8),
    m_version(0),
    m_startTimeConstraint(DateTime::MaxValue),
    m_stopTimeConstraint(DateTime::MaxValue),
    m_processingInterval(-1),
    m_temporalSubscriptionCanceled(false),
    m_usingPayloadCompression(false),
    m_includeTime(true),
    m_useLocalClockAsRealTime(false),
    m_enableTimeReasonabilityCheck(false),
    m_lagTime(DefaultLagTime),
    m_leadTime(DefaultLeadTime),
    m_publishInterval(DefaultPublishInterval),
    m_useMillisecondResolution(false), // Defaults to microsecond resolution
    m_trackLatestMeasurements(false),
    m_isNaNFiltered(m_parent->GetIsNaNValueFilterAllowed() && m_parent->GetIsNaNValueFilterForced()),
    m_validated(false),
    m_connectionAccepted(false),
    m_subscribed(false),
    m_startTimeSent(false),
    m_dataChannelActive(false),
    m_stopped(true),
    m_commandChannelSocket(m_commandChannelService),
    m_readBuffer(Common::MaxPacketSize),
    m_udpPort(0),
    m_dataChannelSocket(m_dataChannelService),
    m_udpWriteStrand(m_dataChannelService),
    m_totalCommandChannelBytesSent(0LL),
    m_totalDataChannelBytesSent(0LL),
    m_totalMeasurementsSent(0LL),
    m_signalIndexCache(SignalIndexCache::NullPtr),
    m_nextSignalIndexCache(SignalIndexCache::NullPtr),
    m_pendingSignalIndexCache(SignalIndexCache::NullPtr),
    m_currentCacheIndex(0),
    m_nextCacheIndex(0),
    m_baseTimeRotationTimer(Timer::NullPtr),
    m_timeIndex(0),
    m_baseTimeOffsets{ 0LL, 0LL },
    m_latestTimestamp(0LL),
    m_lastPublishTime(Empty::DateTime),
    m_throttledPublicationTimer(Timer::NullPtr),
    m_tsscResetRequested(false),
    m_tsscSequenceNumber(0),
    m_disconnecting(false)
{
    for (size_t i = 0; i < TSSCBufferSize; i++)
        m_tsscWorkingBuffer[i] = static_cast<uint8_t>(0);
 
    // Setup ping timer
    m_pingTimer.SetInterval(5000);
    m_pingTimer.SetAutoReset(true);
    m_pingTimer.SetCallback(&SubscriberConnection::PingTimerElapsed);
    m_pingTimer.SetUserData(this);
}

SubscriberConnection::~SubscriberConnection()
{
    if (!m_parent->IsReverseConnection())
        return;

    // Handle reverse connection style shutdown
    try
    {
        m_parent->m_disposing = true;
        m_connector.Cancel();
        Disconnect(true, false);
    }
    catch (...)
    {
        // ReSharper disable once CppRedundantControlFlowJump
        return;
    }
}

const DataPublisherPtr& SubscriberConnection::GetParent() const
{
    return m_parent;
}

SubscriberConnectionPtr SubscriberConnection::GetReference()
{
    return shared_from_this();
}

TcpSocket& SubscriberConnection::CommandChannelSocket()
{
    return m_commandChannelSocket;
}

bool SubscriberConnection::IsValidated() const
{
    return m_validated;
}

bool SubscriberConnection::IsConnected() const
{
    return m_connectionAccepted;
}

uint8_t SubscriberConnection::GetVersion() const
{
    return m_version;
}

const sttp::Guid& SubscriberConnection::GetSubscriberID() const
{
    return m_subscriberID;
}

void SubscriberConnection::SetSubscriberID(const sttp::Guid& id)
{
    m_subscriberID = id;
}

const sttp::Guid& SubscriberConnection::GetInstanceID() const
{
    return m_instanceID;
}

const string& SubscriberConnection::GetConnectionID() const
{
    return m_connectionID;
}

const sttp::IPAddress& SubscriberConnection::GetIPAddress() const
{
    return m_ipAddress;
}

const string& SubscriberConnection::GetHostName() const
{
    return m_hostName;
}

uint32_t SubscriberConnection::GetOperationalModes() const
{
    return m_operationalModes;
}

void SubscriberConnection::SetOperationalModes(const uint32_t value)
{
    m_operationalModes = value;
    m_encoding = value & OperationalModes::EncodingMask;

    string message;

    switch (m_encoding)
    {
        case OperationalEncoding::UTF8:
            break;
        case OperationalEncoding::UTF16LE:
            message = "WARNING: Client requested UTF16 little-endian character encoding, this feature is deprecated and may be removed from future builds. IEEE 2664 will only support UTF8 encoding.";
            break;
        case OperationalEncoding::UTF16BE:
            message = "WARNING: Client requested UTF16 big-endian character encoding, this feature is deprecated and may be removed from future builds. IEEE 2664 will only support UTF8 encoding.";
            break;
        default:
            message = "WARNING: Unsupported character encoding detected: " + ToHex(m_encoding) + " -- defaulting to UTF8";
            m_encoding = OperationalEncoding::UTF8;
            break;
    }

    if (!message.empty())
        m_parent->DispatchStatusMessage(message);
}

uint32_t SubscriberConnection::GetEncoding() const
{
    return m_encoding;
}

bool SubscriberConnection::GetIsTemporalSubscription() const
{
    return m_startTimeConstraint < DateTime::MaxValue;
}

const sttp::datetime_t& SubscriberConnection::GetStartTimeConstraint() const
{
    return m_startTimeConstraint;
}

void SubscriberConnection::SetStartTimeConstraint(const sttp::datetime_t& value)
{
    m_startTimeConstraint = value;
}

const sttp::datetime_t& SubscriberConnection::GetStopTimeConstraint() const
{
    return m_stopTimeConstraint;
}

void SubscriberConnection::SetStopTimeConstraint(const sttp::datetime_t& value)
{
    m_stopTimeConstraint = value;
}

int32_t SubscriberConnection::GetProcessingInterval() const
{
    return m_processingInterval;
}

void SubscriberConnection::SetProcessingInterval(const int32_t value)
{
    m_processingInterval = value;
    m_parent->DispatchProcessingIntervalChangeRequested(m_parent->AddDispatchReference(GetReference()));
    m_parent->DispatchStatusMessage(m_connectionID + " was assigned a new processing interval of " + ToString(value) + "ms.");
}

bool SubscriberConnection::GetUsingPayloadCompression() const
{
    return m_usingPayloadCompression;
}

bool SubscriberConnection::GetUsingCompactMeasurementFormat() const
{
    return !m_usingPayloadCompression;
}

bool SubscriberConnection::GetIncludeTime() const
{
    return m_includeTime;
}

void SubscriberConnection::SetIncludeTime(const bool value)
{
    m_includeTime = value;
}

bool SubscriberConnection::GetUseLocalClockAsRealTime() const
{
    return m_useLocalClockAsRealTime;
}

void SubscriberConnection::SetUseLocalClockAsRealTime(const bool value)
{
    m_useLocalClockAsRealTime = value;
}

float64_t SubscriberConnection::GetLagTime() const
{
    return m_lagTime;
}

void SubscriberConnection::SetLagTime(const float64_t value)
{
    m_lagTime = value;
}

float64_t SubscriberConnection::GetLeadTime() const
{
    return m_leadTime;
}

void SubscriberConnection::SetLeadTime(const float64_t value)
{
    m_leadTime = value;
}

float64_t SubscriberConnection::GetPublishInterval() const
{
    return m_publishInterval;
}

void SubscriberConnection::SetPublishInterval(const float64_t value)
{
    m_publishInterval = value;
}

bool SubscriberConnection::GetUseMillisecondResolution() const
{
    return m_useMillisecondResolution;
}

void SubscriberConnection::SetUseMillisecondResolution(const bool value)
{
    m_useMillisecondResolution = value;
}

bool SubscriberConnection::GetTrackLatestMeasurements() const
{
    return m_trackLatestMeasurements;
}

void SubscriberConnection::SetTrackLatestMeasurements(const bool value)
{
    m_trackLatestMeasurements = value;
}

bool SubscriberConnection::GetIsNaNFiltered() const
{
    return m_isNaNFiltered;
}

void SubscriberConnection::SetIsNaNFiltered(const bool value)
{
    if (value)
    {
        if (m_parent->GetIsNaNValueFilterAllowed() || m_parent->GetIsNaNValueFilterForced())
            m_isNaNFiltered = true;
        else
            m_isNaNFiltered = false;
    }
    else
    {
        if (m_parent->GetIsNaNValueFilterForced())
            m_isNaNFiltered = true;
        else
            m_isNaNFiltered = false;
    }    
}

bool SubscriberConnection::GetIsSubscribed() const
{
    return m_subscribed;
}

void SubscriberConnection::SetIsSubscribed(const bool value)
{
    m_subscribed = value;
}

const string& SubscriberConnection::GetSubscriptionInfo() const
{
    return m_subscriptionInfo;
}

void SubscriberConnection::SetSubscriptionInfo(const string& value)
{
    if (value.empty())
    {
        m_subscriptionInfo.clear();
        return;
    }

    const StringMap<string> settings = ParseKeyValuePairs(value);
    string source, version, updatedOn;

    TryGetValue(settings, "source", source);
    TryGetValue(settings, "version", version);
    TryGetValue(settings, "updatedOn", updatedOn);

    if (source.empty())
        source = "unknown source";

    if (version.empty())
        version = "?.?.?.?";

    if (updatedOn.empty())
        updatedOn = "undefined date";

    m_subscriptionInfo = source + " version " + version + " updated on " + updatedOn;
}

const SignalIndexCachePtr& SubscriberConnection::GetSignalIndexCache()
{
    ReaderLock readLock(m_signalIndexCacheLock);
    return m_signalIndexCache;
}

uint64_t SubscriberConnection::GetTotalCommandChannelBytesSent() const
{
    return m_totalCommandChannelBytesSent;
}

uint64_t SubscriberConnection::GetTotalDataChannelBytesSent() const
{
    return m_totalDataChannelBytesSent;
}

uint64_t SubscriberConnection::GetTotalMeasurementsSent() const
{
    return m_totalMeasurementsSent;
}

bool SubscriberConnection::CipherKeysDefined() const
{
    return !m_keys[0].empty();
}

vector<uint8_t> SubscriberConnection::Keys(const int32_t cipherIndex)
{
    if (cipherIndex < 0 || cipherIndex > 1)
        throw out_of_range("Cipher index must be 0 or 1");

    return m_keys[cipherIndex];
}

vector<uint8_t> SubscriberConnection::IVs(const int32_t cipherIndex)
{
    if (cipherIndex < 0 || cipherIndex > 1)
        throw out_of_range("Cipher index must be 0 or 1");

    return m_ivs[cipherIndex];
}

void SubscriberConnection::StartConnection(const bool connectionAccepted)
{
    m_connectionAccepted = connectionAccepted;

    // Attempt to lookup remote connection identification for logging purposes
    const auto remoteEndPoint = m_commandChannelSocket.remote_endpoint();
    m_ipAddress = remoteEndPoint.address();
    m_connectionID = ResolveDNSName(m_commandChannelService, remoteEndPoint, m_hostName);

    m_stopped = false;

    if (m_connectionAccepted)
        m_pingTimer.Start();

    ReadCommandChannel();
}

// Dispatcher for connection terminated. This is called from its own separate thread
// in order to cleanly shut down the subscriber in case the connection was terminated
// by the peer. Additionally, this allows the user to automatically reconnect in their
// callback function without having to spawn their own separate thread.
void SubscriberConnection::ConnectionTerminatedDispatcher()
{
    Disconnect(false, true);
}

void SubscriberConnection::WaitOnDisconnectThread()
{
    ScopeLock lock(m_disconnectThreadMutex);
    m_disconnectThread.join();
}

void SubscriberConnection::Connect(const string& hostname, const uint16_t port, const bool autoReconnecting)
{
    if (m_connectionAccepted)
        throw PublisherException("Publisher is already connected; disconnect first");

    // Make sure any pending disconnect has completed to make sure socket is closed
    WaitOnDisconnectThread();

    // Let any pending connect or disconnect operation complete before new connect
    // this prevents destruction disconnect before connection is completed
    ScopeLock lock(m_connectActionMutex);
    DnsResolver resolver(m_commandChannelService);
    ErrorCode error;

    m_stopped = false;
    
    if (!autoReconnecting)
        m_connector.ResetConnection();

    m_connector.SetConnectionRefused(false);

    connect(m_commandChannelSocket, resolver.resolve(hostname, to_string(port)), error);

    if (error)
        throw SystemError(error);

    if (!m_commandChannelSocket.is_open())
        throw PublisherException("Failed to connect to host");

#if BOOST_LEGACY
    m_commandChannelService.reset();
#else
    m_commandChannelService.restart();
#endif

    StartConnection(true);
}

void SubscriberConnection::Disconnect(const bool joinThread, const bool autoReconnecting)
{
    m_connectionAccepted = false;

    // Check if disconnect thread is running or subscriber has already disconnected
    if (IsDisconnecting())
    {
        if (!autoReconnecting && m_disconnecting && !m_stopped)
            m_connector.Cancel();
        
        if (joinThread && !m_stopped)
            WaitOnDisconnectThread();

        return;
    }
    
    // Notify running threads that the subscriber is disconnecting, i.e., disconnect thread is active
    m_disconnecting = true;

    {
        ScopeLock lock(m_disconnectThreadMutex);

        m_disconnectThread = Thread([this, autoReconnecting]
        {
            try
            {
                // Let any pending connect operation complete before disconnect - prevents destruction disconnect before connection is completed
                if (!autoReconnecting)
                {
                    m_connector.Cancel();
                    m_connectionTerminationThread.join();
                    m_connectActionMutex.lock();
                }
            }
            catch (SystemError& ex)
            {
                m_parent->DispatchErrorMessage("Exception while disconnecting data publisher reverse connection: " + string(ex.what()));
            }
            catch (...)
            {
                m_parent->DispatchErrorMessage("Exception while disconnecting data publisher reverse connection: " + boost::current_exception_diagnostic_information(true));
            }

            StopConnection();

            // Disconnect complete
            m_disconnecting = false;

            if (autoReconnecting)
            {
                // Handling auto-connect callback separately from connection terminated callback
                // since they serve two different use cases and current implementation does not
                // support multiple callback registrations
                if (m_parent->m_autoReconnectCallback != nullptr && !m_parent->m_disposing)
                    m_parent->m_autoReconnectCallback(m_parent.get());
            }
            else
            {
                m_connectActionMutex.unlock();
            }
        });
    }

    if (joinThread)
        WaitOnDisconnectThread();
}

void SubscriberConnection::StopConnection()
{
    if (m_stopped)
        return;

    if (m_subscribed)
        HandleUnsubscribe();

    try
    {
        m_validated = false;
        m_stopped = true;
        m_pingTimer.Stop();

        if (m_baseTimeRotationTimer != nullptr)
            m_baseTimeRotationTimer->Stop();

        if (m_throttledPublicationTimer != nullptr)
            m_throttledPublicationTimer->Stop();

        m_commandChannelSocket.shutdown(socket_base::shutdown_both);
        m_commandChannelSocket.cancel();

        if (m_dataChannelActive)
        {
            m_dataChannelActive = false;
            m_dataChannelWaitHandle.notify_all();
            m_dataChannelService.stop();
            m_dataChannelSocket.shutdown(socket_base::shutdown_type::shutdown_both);
            m_dataChannelSocket.close();
        }
    }
    catch (SystemError& ex)
    {
        m_parent->DispatchErrorMessage("Exception during data subscriber connection termination: " + string(ex.what()));
    }
    catch (...)
    {
        m_parent->DispatchErrorMessage("Exception during data subscriber connection termination: " + boost::current_exception_diagnostic_information(true));
    }

    m_parent->ConnectionTerminated(shared_from_this());
}

void SubscriberConnection::Stop()
{
    if (m_parent->IsReverseConnection())
        Disconnect(false, false);
    else
        StopConnection();
}

void SubscriberConnection::HandleConnectionError()
{
    // For reverse connection, this handles connection closed by peer; terminate connection
    if (m_parent->IsReverseConnection())
        m_connectionTerminationThread = Thread([this]{ ConnectionTerminatedDispatcher(); });
    else
        StopConnection();
}

void SubscriberConnection::PublishMeasurements(const vector<MeasurementPtr>& measurements)
{
    if (measurements.empty() || !m_subscribed)
        return;

    if (!m_startTimeSent)
        m_startTimeSent = SendDataStartTime(measurements[0]->Timestamp);

    if (m_trackLatestMeasurements)
    {
        ScopeLock lock(m_latestMeasurementsLock);

        // Track latest measurements
        for (const auto& measurement : measurements)
        {
            const Guid signalID = measurement->SignalID;

            if (!m_enableTimeReasonabilityCheck || TimestampIsReasonable(measurement->Timestamp, m_lagTime, m_leadTime) || GetIsTemporalSubscription())
            {
                m_latestMeasurements.insert_or_assign(signalID, measurement);
            }
            else
            {
                MeasurementPtr trackedMeasurement = ToPtr(*measurement);
                trackedMeasurement->Value = NaN;
                m_latestMeasurements.insert_or_assign(signalID, trackedMeasurement);
            }
        }
    }
    else
    {
        if (m_usingPayloadCompression)
            PublishTSSCMeasurements(measurements);
        else
            PublishCompactMeasurements(measurements);
    }
}

void SubscriberConnection::CancelTemporalSubscription()
{
    if (GetIsTemporalSubscription() && !m_temporalSubscriptionCanceled)
    {
        m_temporalSubscriptionCanceled = true;
        SendResponse(ServerResponse::ProcessingComplete, ServerCommand::Subscribe, ToString(m_parent->GetNodeID()));
        m_parent->DispatchTemporalSubscriptionCanceled(m_parent->AddDispatchReference(GetReference()));
    }
}

void SubscriberConnection::HandleSubscribe(uint8_t* data, uint32_t length)
{
    try
    {
        if (length < 6)
        {
            HandleSubscribeFailure("Not enough buffer was provided to parse client data subscription.");
            return;
        }

        const uint8_t flags = data[0];

        if (flags & DataPacketFlags::Synchronized)
        {
            // Remotely synchronized subscriptions are currently disallowed by data publisher
            HandleSubscribeFailure("Client request for remotely synchronized data subscription was denied. Data publisher currently does not allow for synchronized subscriptions.");
            return;
        }

        int32_t index = 1;

        // Cancel any existing subscription timers
        if (m_baseTimeRotationTimer != nullptr)
            m_baseTimeRotationTimer->Stop();

        if (m_throttledPublicationTimer != nullptr)
            m_throttledPublicationTimer->Stop();

        // Clear out existing latest measurement cache, if any
        m_latestMeasurementsLock.lock();
        m_latestMeasurements.clear();
        m_latestMeasurementsLock.unlock();

        // Cancel any existing temporal subscription
        if (m_subscribed)
            CancelTemporalSubscription();

        // Next 4 bytes are an integer representing the length of the connection string that follows
        const uint32_t byteLength = EndianConverter::ToBigEndian<uint32_t>(data, index);
        index += 4;

        if (byteLength <= 0 || length < byteLength + 6U)
        {
            HandleSubscribeFailure(byteLength > 0 ?
               "Not enough buffer was provided to parse client data subscription." :
               "Cannot initialize client data subscription without a connection string.");
            return;
        }

        uint32_t operationalModes = GetOperationalModes();

        // TODO: Update for IEEE 2664 supporting optional compression modes with DefineOperationalModes connection string
        m_usingPayloadCompression = (operationalModes & OperationalModes::CompressPayloadData) > 0 && (operationalModes & CompressionModes::TSSC) > 0;

        const string connectionString = DecodeString(data, index, byteLength);

        if (!m_usingPayloadCompression && ((flags & DataPacketFlags::Compact) == 0 || (operationalModes & OperationalModes::CompressPayloadData) > 0))
            m_parent->DispatchErrorMessage("WARNING: Data packets will be published in compact measurement format only when not compressing payload using TSSC.");

        m_parent->DispatchStatusMessage("Successfully decoded " + ToString(connectionString.size()) + " character connection string from " + ToString(byteLength) + " bytes...");

        StringMap<string> settings = ParseKeyValuePairs(connectionString);
        string setting;

        if (TryGetValue(settings, "includeTime", setting))
            TryParseBoolean(setting, m_includeTime);
        else
            m_includeTime = true;

        if (TryGetValue(settings, "useLocalClockAsRealTime", setting))
            TryParseBoolean(setting, m_useLocalClockAsRealTime);
        else
            m_useLocalClockAsRealTime = false;

        if (TryGetValue(settings, "enableTimeReasonabilityCheck", setting))
            TryParseBoolean(setting, m_enableTimeReasonabilityCheck);
        else
            m_enableTimeReasonabilityCheck = true;

        if (TryGetValue(settings, "lagTime", setting) && !setting.empty())
            TryParseDouble(setting, m_lagTime, DefaultLagTime);
        else
            m_lagTime = DefaultLagTime;

        if (TryGetValue(settings, "leadTime", setting) && !setting.empty())
            TryParseDouble(setting, m_leadTime, DefaultLeadTime);
        else
            m_leadTime = DefaultLeadTime;

        if (TryGetValue(settings, "publishInterval", setting) && !setting.empty())
            TryParseDouble(setting, m_publishInterval, DefaultPublishInterval);
        else
            m_publishInterval = DefaultPublishInterval;

        if (TryGetValue(settings, "useMillisecondResolution", setting))
            TryParseBoolean(setting, m_useMillisecondResolution);
        else
            m_useMillisecondResolution = false;

        if (TryGetValue(settings, "throttled", setting))
            TryParseBoolean(setting, m_trackLatestMeasurements);
        else
            m_trackLatestMeasurements = false;

        if (TryGetValue(settings, "requestNaNValueFilter", setting))
        {
            const bool nanFilterRequested = ParseBoolean(setting);

            if (nanFilterRequested && !m_parent->GetIsNaNValueFilterAllowed() && !m_parent->GetIsNaNValueFilterForced())
            {
                m_parent->DispatchErrorMessage("WARNING: NaN filter is disallowed by publisher, requestNaNValueFilter setting was set to false");
                m_isNaNFiltered = false;
            }
            else if (!nanFilterRequested && m_parent->GetIsNaNValueFilterForced())
            {
                m_parent->DispatchErrorMessage("WARNING: NaN filter is required by publisher, requestNaNValueFilter setting was set to true");
                m_isNaNFiltered = true;
            }
            else
            {
                m_isNaNFiltered = nanFilterRequested;
            }
        }

        if (TryGetValue(settings, "startTimeConstraint", setting))
            m_startTimeConstraint = ParseRelativeTimestamp(setting.c_str());
        else
            m_startTimeConstraint = DateTime::MaxValue;

        if (TryGetValue(settings, "stopTimeConstraint", setting))
            m_stopTimeConstraint = ParseRelativeTimestamp(setting.c_str());
        else
            m_stopTimeConstraint = DateTime::MaxValue;

        if (TryGetValue(settings, "processingInterval", setting) && !setting.empty())
            TryParseInt32(setting, m_processingInterval, -1);

        if (GetIsTemporalSubscription())
        {
            if (!m_parent->GetSupportsTemporalSubscriptions())
                throw PublisherException("Publisher does not temporal subscriptions");

            if (m_startTimeConstraint > m_stopTimeConstraint)
                throw PublisherException("Specified stop time of requested temporal subscription precedes start time");

            m_temporalSubscriptionCanceled = false;
        }

        TryGetValue(settings, "filterExpression", setting);
                        
        if (IsEmptyOrWhiteSpace(setting))
            setting = ToString(Empty::Guid);

        bool success;
                    
        // Apply subscriber filter expression and build signal index cache
        SignalIndexCachePtr signalIndexCache = ParseSubscriptionRequest(setting, success);

        if (!success)
            return;

        // Pass subscriber assembly information to connection, if defined
        if (TryGetValue(settings, "assemblyInfo", setting))
        {
            SetSubscriptionInfo(setting);
            m_parent->DispatchStatusMessage("Reported STTP v" + ToString(static_cast<int32_t>(m_version)) + " client subscription info: " + GetSubscriptionInfo());
        }

        if (TryGetValue(settings, "dataChannel", setting))
        {
            auto remoteEndPoint = m_commandChannelSocket.remote_endpoint(); //-V821
            auto localEndPoint = m_commandChannelSocket.local_endpoint();
            string networkInterface = localEndPoint.address().to_string();
            settings = ParseKeyValuePairs(setting);

            // Remove any dual-stack prefix
            if (StartsWith(networkInterface, "::ffff:"))
                networkInterface = networkInterface.substr(7);

            if (TryGetValue(settings, "port", setting) || TryGetValue(settings, "localport", setting))
            {
                // TODO: Implement IEEE 2664 support for UDP specific compression mode, GZip by default
                if (m_usingPayloadCompression)
                {
                    // TSSC is a stateful compression algorithm which will not reliably support UDP
                    m_parent->DispatchErrorMessage("WARNING: Cannot use TSSC compression mode with UDP - special compression mode disabled");

                    // Disable TSSC compression processing
                    m_usingPayloadCompression = false;

                    // TODO: Future version may reject subscription - IEEE 2664 will not support dynamic changes to operational modes, even a benign one like this
                    operationalModes &= ~CompressionModes::TSSC;
                    operationalModes &= ~OperationalModes::CompressPayloadData;
                    SetOperationalModes(operationalModes);
                }

                if (TryParseUInt16(setting, m_udpPort))
                {
                    const udp protocol = remoteEndPoint.protocol() == tcp::v6() ? udp::v6() : udp::v4();

                    // Reset UDP socket on resubscribe
                    if (m_dataChannelActive)
                    {
                        m_dataChannelActive = false;
                        m_dataChannelWaitHandle.notify_all();
                        m_dataChannelSocket.shutdown(socket_base::shutdown_type::shutdown_both);
                        m_dataChannelSocket.close();
                        m_dataChannelService.stop();
                    }

                    m_dataChannelSocket.open(protocol);
                    m_dataChannelSocket.bind(udp::endpoint(ip::make_address(networkInterface), 0));
                    m_dataChannelSocket.connect(udp::endpoint(remoteEndPoint.address(), m_udpPort));
                    m_dataChannelActive = true;
                                
                    m_parent->m_threadPool.Queue([this]
                    {
                        UniqueLock lock(m_dataChannelMutex);

                        while (m_dataChannelActive)
                        {
                        #if BOOST_LEGACY
                            m_dataChannelService.reset();
                        #else
                            m_dataChannelService.restart();
                        #endif
                            m_dataChannelService.run();                                        
                            m_dataChannelWaitHandle.wait(lock);
                        }
                    });
                }
            }
        }

        if (signalIndexCache == nullptr)
            throw PublisherException("Signal index cache is undefined.");

        UpdateSignalIndexCache(signalIndexCache);

        uint32_t signalCount = signalIndexCache->Count();

        // If using compact measurement format with base time offsets, setup base time rotation timer
        if (!m_usingPayloadCompression && m_parent->GetUseBaseTimeOffsets() && m_includeTime)
        {
            // In compact format, clients will attempt to use base time offset
            // to reduce timestamp serialization size
            m_baseTimeOffsets[0] = 0LL;
            m_baseTimeOffsets[1] = 0LL;
            m_latestTimestamp = 0LL;

            m_baseTimeRotationTimer = NewSharedPtr<Timer>(m_useMillisecondResolution ? 60000 : 420000, [this](const TimerPtr& timer, void*)
            {
                const int64_t realTime = m_useLocalClockAsRealTime ? ToTicks(UtcNow()) : m_latestTimestamp;

                if (realTime == 0LL)
                    return;

                if (m_baseTimeOffsets[0] == 0LL)
                {
                    // Initialize base time offsets
                    m_baseTimeOffsets[0] = realTime;
                    m_baseTimeOffsets[1] = realTime + timer->GetInterval() * Ticks::PerMillisecond;

                    m_timeIndex = 0;
                }
                else
                {
                    const uint32_t oldIndex = m_timeIndex;

                    // Switch to next time base (client will already have access to this)
                    m_timeIndex ^= 1;

                    // Setup next time base
                    m_baseTimeOffsets[oldIndex] = realTime + timer->GetInterval() * Ticks::PerMillisecond;
                }

                // Send new base time offsets to client
                vector<uint8_t> buffer;
                buffer.reserve(20);

                EndianConverter::WriteBigEndianBytes(buffer, m_timeIndex);
                EndianConverter::WriteBigEndianBytes(buffer, m_baseTimeOffsets[0]);
                EndianConverter::WriteBigEndianBytes(buffer, m_baseTimeOffsets[1]);

                SendResponse(ServerResponse::UpdateBaseTimes, ServerCommand::Subscribe, buffer);

                m_parent->DispatchStatusMessage("Sent new base time offset to subscriber: " + ToString(FromTicks(m_baseTimeOffsets[m_timeIndex ^ 1])));
            },
            true);

            m_baseTimeRotationTimer->Start();
        }

        // Setup publication timer for throttled subscriptions
        if (m_trackLatestMeasurements)
        {
            int32_t publishInterval = static_cast<int32_t>(m_publishInterval * 1000);

            // Fall back on lag-time if publish interval is defined as zero
            if (publishInterval <= 0)
                publishInterval = static_cast<int32_t>((m_lagTime == DefaultLagTime || m_lagTime <= 0.0 ? DefaultPublishInterval : m_lagTime) * 1000); // NOLINT(clang-diagnostic-float-equal) //-V550

            m_throttledPublicationTimer = NewSharedPtr<Timer>(publishInterval, [this](const TimerPtr&, void*)
            {
                if (m_latestMeasurements.empty())
                    return;

                vector<MeasurementPtr> measurements;

                m_latestMeasurementsLock.lock();

                for (const auto& element : m_latestMeasurements)
                {
                    MeasurementPtr measurement = element.second;

                    if (m_enableTimeReasonabilityCheck && !TimestampIsReasonable(measurement->Timestamp, m_lagTime, m_leadTime) && !GetIsTemporalSubscription())
                    {
                        measurement->Value = NaN;
                        measurement->Flags |= MeasurementStateFlags::BadTime;
                    }

                    measurements.push_back(measurement);
                }

                m_latestMeasurementsLock.unlock();

                if (m_usingPayloadCompression)
                    PublishTSSCMeasurements(measurements);
                else
                    PublishCompactMeasurements(measurements);
            },
            true);

            m_throttledPublicationTimer->Start();
        }

        const string message = string("Client subscribed using ") + 
            (m_usingPayloadCompression ? "TSSC compression over " : "compact format over ") +
            (m_dataChannelActive ? "UDP" : "TCP") + " with " + ToString(signalCount) + " signals.";

        SetIsSubscribed(true);

        SendResponse(ServerResponse::Succeeded, ServerCommand::Subscribe, message);
        m_parent->DispatchStatusMessage(message);

        if (GetIsTemporalSubscription())
            m_parent->DispatchTemporalSubscriptionRequested(m_parent->AddDispatchReference(GetReference()));
    }
    catch (const PublisherException& ex)
    {
        HandleSubscribeFailure("Failed to process client data subscription due to exception: " + string(ex.what()));
    }
    catch (...)
    {
        HandleSubscribeFailure("Failed to process client data subscription due to exception: " + boost::current_exception_diagnostic_information(true));
    }
}

void SubscriberConnection::HandleSubscribeFailure(const std::string& message)
{
    SendResponse(ServerResponse::Failed, ServerCommand::Subscribe, message);
    m_parent->DispatchErrorMessage(message);

    if (GetIsTemporalSubscription())
        CancelTemporalSubscription();
}

void SubscriberConnection::HandleUnsubscribe()
{
    SetIsSubscribed(false);

    if (GetIsTemporalSubscription())
        CancelTemporalSubscription();
}

void SubscriberConnection::HandleMetadataRefresh(uint8_t* data, uint32_t length)
{
    // Ensure that the subscriber is allowed to request meta-data
    if (!m_parent->GetIsMetadataRefreshAllowed())
        throw PublisherException("Meta-data refresh has been disallowed by the DataPublisher.");

    m_parent->DispatchStatusMessage("Received meta-data refresh request from " + GetConnectionID() + ", preparing response...");

    StringMap<ExpressionTreePtr> filterExpressions;
    const datetime_t startTime = UtcNow(); //-V821

    try
    {
        // Note that these client provided meta-data filter expressions are applied only to the
        // in-memory DataSet and therefore are not subject to SQL injection attacks
        if (length > 4 && m_parent->m_metadata != nullptr)
        {
            uint32_t index = 0;
            const uint32_t responseLength = EndianConverter::ToBigEndian<uint32_t>(data, index);
            index += 4;

            if (length >= responseLength + 4)
            {
                const string metadataFilters = DecodeString(data, index, responseLength);
                const vector<ExpressionTreePtr> expressions = filterexpressions::FilterExpressionParser::GenerateExpressionTrees(m_parent->m_metadata, "MeasurementDetail", metadataFilters);

                // Go through each subscriber specified filter expressions and add it to dictionary
                for (const auto& expression : expressions)
                    filterExpressions[expression->Table()->Name()] = expression;
            }
        }
    }
    catch (const FilterExpressionParserException& ex)
    {
        m_parent->DispatchErrorMessage("Failed to parse subscriber provided meta-data filter expressions: FilterExpressionParser exception: " + string(ex.what()));
    }
    catch (const ExpressionTreeException& ex)
    {
        m_parent->DispatchErrorMessage("Failed to parse subscriber provided meta-data filter expressions: ExpressionTree exception: " + string(ex.what()));
    }
    catch (...)
    {
        m_parent->DispatchErrorMessage("Failed to parse subscriber provided meta-data filter expressions: " + boost::current_exception_diagnostic_information(true));
    }

    try
    {
        const DataSetPtr metadata = FilterClientMetadata(filterExpressions);

        if (metadata == nullptr)
        {
            m_parent->DispatchStatusMessage("No meta-data has been defined by the publisher, sending an empty response to " + GetConnectionID() + "...");
            SendResponse(ServerResponse::Succeeded, ServerCommand::MetadataRefresh, vector<uint8_t>(0));
        }
        else
        {
            const vector<uint8_t> serializedMetadata = SerializeMetadata(metadata);
            vector<DataTablePtr> tables = metadata->Tables();
            uint64_t rowCount = 0;

            for (size_t i = 0; i < tables.size(); i++)
                rowCount += tables[i]->RowCount();

            if (rowCount > 0)
            {
                const TimeSpan elapsedTime = UtcNow() - startTime;
                m_parent->DispatchStatusMessage(ToString(rowCount) + " records spanning " + ToString(tables.size()) + " tables of meta-data prepared in " + ToString(elapsedTime) + ", sending response to " + GetConnectionID() + "...");
            }
            else
            {
                m_parent->DispatchStatusMessage("No meta-data is available" + string(filterExpressions.empty() ? "" : " due to user applied meta-data filters") + ", sending an empty response to " + GetConnectionID() + "...");
            }

            SendResponse(ServerResponse::Succeeded, ServerCommand::MetadataRefresh, serializedMetadata);
        }
    }
    catch (const FilterExpressionParserException& ex)
    {
        const string message = "Failed to transfer meta-data: FilterExpressionParser exception: " + string(ex.what());
        SendResponse(ServerResponse::Failed, ServerCommand::MetadataRefresh, message);
        m_parent->DispatchErrorMessage(message);
    }
    catch (const ExpressionTreeException& ex)
    {
        const string message = "Failed to transfer meta-data: ExpressionTree exception: " + string(ex.what());
        SendResponse(ServerResponse::Failed, ServerCommand::MetadataRefresh, message);
        m_parent->DispatchErrorMessage(message);
    }
    catch (...)
    {
        const string message = "Failed to transfer meta-data: " + boost::current_exception_diagnostic_information(true);
        SendResponse(ServerResponse::Failed, ServerCommand::MetadataRefresh, message);
        m_parent->DispatchErrorMessage(message);
    }
}

void SubscriberConnection::HandleRotateCipherKeys()
{
    // TODO: Implement cipher key rotation for UDP encryption
}

void SubscriberConnection::HandleUpdateProcessingInterval(const uint8_t* data, const uint32_t length)
{
    // Make sure there is enough buffer for new processing interval value
    if (length >= 4)
    {
        // Next 4 bytes are an integer representing the new processing interval
        const int32_t processingInterval = EndianConverter::Default.ConvertBigEndian(*reinterpret_cast<const int32_t*>(data));
        SetProcessingInterval(processingInterval);
        SendResponse(ServerResponse::Succeeded, ServerCommand::UpdateProcessingInterval, "New processing interval of " + ToString(processingInterval) + " assigned.");
    }
    else
    {
        const string message = "Not enough buffer was provided to update client processing interval.";
        SendResponse(ServerResponse::Failed, ServerCommand::UpdateProcessingInterval, message);
        m_parent->DispatchErrorMessage(message);
    }
}

void SubscriberConnection::HandleDefineOperationalModes(const uint8_t* data, const uint32_t length)
{
    if (length < 4)
        return;

    const uint32_t operationalModes = EndianConverter::ToBigEndian<uint32_t>(data, 0);
    const uint8_t version = operationalModes & OperationalModes::PreStandardVersionMask;

    if (version < 1 || version > 3)
    {
        const string message = "Client connection rejected: requested protocol version " + ToString(static_cast<int32_t>(version)) + " not supported. This STTP data publisher implementation only supports version 1 to 3 of the protocol.";
        m_parent->DispatchErrorMessage(message + " Operational modes may not be set correctly for client \"" + GetConnectionID() + "\" -- disconnecting client");

        m_validated = false;
        SendResponse(ServerResponse::Failed, ServerCommand::DefineOperationalModes, message);

        m_parent->m_threadPool.Queue([&]
        {
            // Allow a moment for failed response to be sent before disconnecting client
            ThreadSleep(1000);
            Stop();
        });

        return;
    }

    // TODO: Validate other operational modes
    m_version = version;

    if (m_version > 1)
        m_currentCacheIndex = 1;

    SetOperationalModes(operationalModes);

    const string message = "STTP v" + ToString(static_cast<int32_t>(m_version)) + " client connection accepted: requested operational modes applied.";
    m_parent->DispatchStatusMessage(message);

    m_validated = true;
    SendResponse(ServerResponse::Succeeded, ServerCommand::DefineOperationalModes, message);
}

void SubscriberConnection::HandleConfirmUpdateSignalIndexCache(const uint8_t* data, const uint32_t length)
{
    WriterLock writeLock(m_signalIndexCacheLock);

    // Swap over to next signal index cache
    if (m_nextSignalIndexCache != nullptr)
    {
        if (m_signalIndexCache == nullptr)
            m_parent->DispatchStatusMessage("Received confirmation of signal index cache update for subscriber \"" + GetConnectionID() + "\". Transitioning to cache index " + ToString(m_nextCacheIndex) + " with " + ToString(m_nextSignalIndexCache->Count()) + " records...");
        else
            m_parent->DispatchStatusMessage("Received confirmation of signal index cache update for subscriber \"" + GetConnectionID() + "\". Transitioning from cache index " + ToString(m_currentCacheIndex) + " with " + ToString(m_signalIndexCache->Count()) + " records to cache index " + ToString(m_nextCacheIndex) + " with " + ToString(m_nextSignalIndexCache->Count()) + " records...");

        m_signalIndexCache = std::move(m_nextSignalIndexCache);
        m_currentCacheIndex = m_nextCacheIndex;
        m_nextSignalIndexCache = SignalIndexCache::NullPtr;

        // Update measurement routes for newly subscribed measurement signal IDs
        m_parent->m_routingTables.UpdateRoutes(shared_from_this(), m_signalIndexCache->GetSignalIDs());

        // Reset TSSC encoder on successful (re)subscription
        ScopeLock lock(m_tsscResetRequestedLock);
        m_tsscResetRequested = true;
    }

    // Check for any pending signal index cache update
    m_parent->m_threadPool.Queue([&]
    {
        string exceptionMessage;

        try
        {
            SignalIndexCachePtr nextSignalIndexCache;

            // Constrain lock to this block
            {
                ScopeLock lock(m_pendingSignalIndexCacheLock);

                if (m_pendingSignalIndexCache == nullptr)
                    return;

                nextSignalIndexCache = m_pendingSignalIndexCache;
                m_pendingSignalIndexCache = SignalIndexCache::NullPtr;
            }

            m_parent->DispatchStatusMessage("Applying pending signal cache update for subscriber \"" + GetConnectionID() + "\" with " + ToString(nextSignalIndexCache->Count()) + " records...");
            UpdateSignalIndexCache(nextSignalIndexCache);
        }
        catch (const std::exception& ex)
        {
            exceptionMessage = "Pending signal index cache processing exception: " + string(ex.what());
        }
        catch (...)
        {
            exceptionMessage = "Pending signal index cache processing exception: " + boost::current_exception_diagnostic_information(true);
        }

        if (!exceptionMessage.empty())
            m_parent->DispatchErrorMessage(exceptionMessage);
    });
}

void SubscriberConnection::HandleConfirmNotification(const uint8_t* data, const uint32_t length)
{
    // TODO: Handle confirm notify
}

void SubscriberConnection::HandleConfirmBufferBlock(const uint8_t* data, const uint32_t length)
{
    // TODO: Handle confirm buffer block
}

void SubscriberConnection::HandleConfirmUpdateBaseTimes(const uint8_t* data, const uint32_t length)
{
    // TODO: Handle confirm update bases time (per IEEE 2664)
}

void SubscriberConnection::HandleUserCommand(const uint8_t command, const uint8_t* data, const uint32_t length)
{
    m_parent->DispatchUserCommand(m_parent->AddDispatchReference(GetReference()), command, data, length);
}

SignalIndexCachePtr SubscriberConnection::ParseSubscriptionRequest(const string& filterExpression, bool& success)
{
    string exceptionMessage, parsingException;
    const FilterExpressionParserPtr parser = NewSharedPtr<FilterExpressionParser>(filterExpression);

    // Define an empty schema if none has been defined
    if (m_parent->m_filteringMetadata == nullptr)
        m_parent->m_filteringMetadata = DataSet::FromXml(ActiveMeasurementsSchema, ActiveMeasurementsSchemaLength);

    // Set filtering dataset, this schema contains a more flattened, denormalized view of available metadata for easier filtering
    parser->SetDataSet(m_parent->m_filteringMetadata);

    // Manually specified signal ID and measurement key fields are expected to be searched against ActiveMeasurements table
    parser->SetTableIDFields("ActiveMeasurements", FilterExpressionParser::DefaultTableIDFields);
    parser->SetPrimaryTableName("ActiveMeasurements");

    // Register call-back for ANLTR parsing exceptions -- these will be appended to any primary exception message
    parser->RegisterParsingExceptionCallback([&parsingException](const FilterExpressionParserPtr&, const string& exception) { parsingException = exception; });

    try
    {
        parser->Evaluate();
    }
    catch (const FilterExpressionParserException& ex)
    {
        exceptionMessage = "FilterExpressionParser exception: " + string(ex.what());
    }
    catch (const ExpressionTreeException& ex)
    {
        exceptionMessage = "ExpressionTree exception: " + string(ex.what());
    }
    catch (...)
    {
        exceptionMessage = boost::current_exception_diagnostic_information(true);
    }

    if (!exceptionMessage.empty())
    {
        if (!parsingException.empty())
            exceptionMessage += "\n" + parsingException;

        SendResponse(ServerResponse::Failed, ServerCommand::Subscribe, exceptionMessage);
        m_parent->DispatchErrorMessage(exceptionMessage);

        success = false;
        return nullptr;
    }

    uint32_t charSizeEstimate;

    switch (GetEncoding())
    {
        case OperationalEncoding::UTF16LE:
        case OperationalEncoding::UTF16BE:
            charSizeEstimate = 2U;
            break;
        default:
            charSizeEstimate = 1U;
            break;
    }

    const DataTablePtr& activeMeasurements = m_parent->m_filteringMetadata->Table("ActiveMeasurements");
    const vector<DataRowPtr>& rows = parser->FilteredRows();
    const int32_t idColumn = DataPublisher::GetColumnIndex(activeMeasurements, "ID");
    const int32_t signalIDColumn = DataPublisher::GetColumnIndex(activeMeasurements, "SignalID");

    // Create a new signal index cache for filtered measurements
    SignalIndexCachePtr signalIndexCache = NewSharedPtr<SignalIndexCache>();

    for (size_t i = 0; i < rows.size(); i++)
    {
        const DataRowPtr& row = rows[i];
        const Guid& signalID = row->ValueAsGuid(signalIDColumn).GetValueOrDefault();        
        string source;
        uint64_t id;

        ParseMeasurementKey(row->ValueAsString(idColumn).GetValueOrDefault(), source, id);
        signalIndexCache->AddMeasurementKey(static_cast<int32_t>(i), signalID, source, id, charSizeEstimate);
    }

    success = true;
    return signalIndexCache;
}

void SubscriberConnection::UpdateSignalIndexCache(SignalIndexCachePtr signalIndexCache)
{
    WriterLock writeLock(m_signalIndexCacheLock);

    if (m_version > 1)
    {
        if (m_nextSignalIndexCache == nullptr)
        {
            m_nextCacheIndex = m_currentCacheIndex ^ 1;
            m_nextSignalIndexCache =  std::move(signalIndexCache);

            // Update serialized cache with proper index
            vector<uint8_t> serializedSignalIndexCache = SerializeSignalIndexCache(*m_nextSignalIndexCache);
            serializedSignalIndexCache[0] = static_cast<uint8_t>(m_nextCacheIndex);

            // Send updated signal index cache to data subscriber
            SendResponse(ServerResponse::UpdateSignalIndexCache, ServerCommand::Subscribe, serializedSignalIndexCache);

            ScopeLock lock(m_pendingSignalIndexCacheLock);
            m_pendingSignalIndexCache = SignalIndexCache::NullPtr;
        }
        else
        {
            // Queue any pending update to be processed after current item - this handles
            // updated subscription that may be occurring in quick succession
            ScopeLock lock(m_pendingSignalIndexCacheLock);
            m_pendingSignalIndexCache = std::move(signalIndexCache);
        }
    }
    else
    {
        m_signalIndexCache = std::move(signalIndexCache);

        // Send updated signal index cache to data subscriber
        SendResponse(ServerResponse::UpdateSignalIndexCache, ServerCommand::Subscribe, SerializeSignalIndexCache(*m_signalIndexCache));

        // Update measurement routes for newly subscribed measurement signal IDs
        m_parent->m_routingTables.UpdateRoutes(shared_from_this(), m_signalIndexCache->GetSignalIDs());

        // Reset TSSC encoder on successful (re)subscription
        ScopeLock lock(m_tsscResetRequestedLock);
        m_tsscResetRequested = true;
    }
}

void SubscriberConnection::PublishCompactMeasurements(const std::vector<MeasurementPtr>& measurements)
{
    const SignalIndexCachePtr signalIndexCache = GetSignalIndexCache();

    // Cache not available while initializing
    if (signalIndexCache == nullptr || signalIndexCache->Count() == 0)
        return;

    const CompactMeasurement serializer(signalIndexCache, m_baseTimeOffsets, m_includeTime, m_useMillisecondResolution, m_timeIndex);
    vector<uint8_t> packet, buffer;
    int32_t count = 0;

    packet.reserve(MaxPacketSize);
    buffer.reserve(16);

    for (size_t i = 0; i < measurements.size(); i++)
    {
        const Measurement& measurement = *measurements[i];
        const int64_t timestamp = measurement.Timestamp;
        const int32_t runtimeID = signalIndexCache->GetSignalIndex(measurement.SignalID);

        if (runtimeID == -1)
            continue;

        if (m_isNaNFiltered && isnan(measurement.Value))
            continue;

        const uint32_t length = serializer.SerializeMeasurement(measurement, buffer, runtimeID);

        if (packet.size() + length > MaxPacketSize)
        {
            PublishCompactDataPacket(packet, count);
            packet.clear();
            count = 0;
        }

        WriteBytes(packet, buffer);  // NOLINT
        buffer.clear();
        count++; //-V127

        // Track latest timestamp
        if (!m_useLocalClockAsRealTime && timestamp > m_latestTimestamp && (!m_enableTimeReasonabilityCheck || TimestampIsReasonable(timestamp, m_lagTime, m_leadTime) || GetIsTemporalSubscription()))
            m_latestTimestamp = timestamp;
    }

    if (count > 0)
        PublishCompactDataPacket(packet, count);
}

void SubscriberConnection::PublishCompactDataPacket(const vector<uint8_t>& packet, const int32_t count)
{
    vector<uint8_t> buffer;
    buffer.reserve(packet.size() + 5);

    // Serialize data packet flags into response
    buffer.push_back(DataPacketFlags::Compact);

    // Serialize total number of measurement values to follow
    EndianConverter::WriteBigEndianBytes(buffer, count);

    // Serialize measurements to data buffer
    WriteBytes(buffer, packet);

    // Publish data packet to client
    SendResponse(ServerResponse::DataPacket, ServerCommand::Subscribe, buffer);

    // Track last publication time
    m_lastPublishTime = UtcNow();

    // Track total number of published measurements
    m_totalMeasurementsSent += count;
}

void SubscriberConnection::PublishTSSCMeasurements(const std::vector<MeasurementPtr>& measurements)
{
    const SignalIndexCachePtr signalIndexCache = GetSignalIndexCache();

    // Cache not available while initializing
    if (signalIndexCache == nullptr || signalIndexCache->Count() == 0)
        return;

    ScopeLock lock(m_tsscResetRequestedLock);

    if (m_tsscResetRequested)
    {
        m_tsscResetRequested = false;
        m_tsscEncoder.Reset();

        for (size_t i = 0; i < TSSCBufferSize; i++)
            m_tsscWorkingBuffer[i] = static_cast<uint8_t>(0);

        if (m_tsscSequenceNumber != 0)
            m_parent->DispatchStatusMessage("TSSC algorithm reset before sequence number: " + ToString(m_tsscSequenceNumber));

        m_tsscSequenceNumber = 0;
    }

    m_tsscEncoder.SetBuffer(m_tsscWorkingBuffer, 0, TSSCBufferSize);
    int32_t count = 0;

    for (const auto& measurement : measurements)
    {
        const int32_t runtimeID = signalIndexCache->GetSignalIndex(measurement->SignalID);

        // Ignore unmapped signal
    	if (runtimeID == -1)
            continue;
    	
        if (!m_tsscEncoder.TryAddMeasurement(runtimeID, measurement->Timestamp, static_cast<uint32_t>(measurement->Flags), static_cast<float32_t>(measurement->AdjustedValue())))
        {
            PublishTSSCDataPacket(count);
            count = 0;

            m_tsscEncoder.SetBuffer(m_tsscWorkingBuffer, 0, TSSCBufferSize);
            m_tsscEncoder.TryAddMeasurement(runtimeID, measurement->Timestamp, static_cast<uint32_t>(measurement->Flags), static_cast<float32_t>(measurement->AdjustedValue()));
        }

        count++; //-V127
    }

    if (count > 0)
        PublishTSSCDataPacket(count);
}

void SubscriberConnection::PublishTSSCDataPacket(const int32_t count)
{
    const uint32_t length = m_tsscEncoder.FinishBlock();
    vector<uint8_t> buffer;
    buffer.reserve(static_cast<size_t>(length) + 8);

    // Serialize data packet flags into response
    buffer.push_back(DataPacketFlags::Compressed);

    // Serialize total number of measurement values to follow
    EndianConverter::WriteBigEndianBytes(buffer, count);

    // Add a version number
    buffer.push_back(85);

    EndianConverter::WriteBigEndianBytes(buffer, m_tsscSequenceNumber);
    m_tsscSequenceNumber++;

    // Do not increment sequence number to 0
    if (m_tsscSequenceNumber == 0)
        m_tsscSequenceNumber = 1;

    for (size_t i = 0; i < length; i++)
        buffer.push_back(m_tsscWorkingBuffer[i]);

    // Publish data packet to client
    SendResponse(ServerResponse::DataPacket, ServerCommand::Subscribe, buffer);

    // Track last publication time
    m_lastPublishTime = UtcNow();

    // Track total number of published measurements
    m_totalMeasurementsSent += count;
}

bool SubscriberConnection::SendDataStartTime(const uint64_t timestamp)
{
    vector<uint8_t> buffer;
    EndianConverter::WriteBigEndianBytes(buffer, timestamp);
    const bool result = SendResponse(ServerResponse::DataStartTime, ServerCommand::Subscribe, buffer);

    if (result)
        m_parent->DispatchStatusMessage("Start time sent to " + m_connectionID + ".");

    return result;
}

// All commands received from the client are handled by this thread.
void SubscriberConnection::ReadCommandChannel()
{
    if (IsDisconnecting())
        return;

    async_read(m_commandChannelSocket, buffer(m_readBuffer, Common::PayloadHeaderSize), [this]<typename T0, typename T1>(T0&& error, T1&& bytesTransferred)
    {
        ReadPayloadHeader(error, bytesTransferred);
    });
}

void SubscriberConnection::ReadPayloadHeader(const ErrorCode& error, const size_t bytesTransferred)
{
    if (IsDisconnecting())
        return;

    // Stop cleanly, i.e., don't report, on these errors
    if (error == error::connection_aborted || error == error::connection_reset || error == error::eof)
    {
        HandleConnectionError();
        return;
    }

    if (error)
    {
        stringstream messageStream;

        messageStream << "Error reading data from client \"";
        messageStream << m_connectionID;
        messageStream << "\" command channel: ";
        messageStream << SystemError(error).what();

        m_parent->DispatchErrorMessage(messageStream.str());

        HandleConnectionError();
        return;
    }

    const uint32_t packetSize = EndianConverter::ToBigEndian<uint32_t>(m_readBuffer.data(), 0);

    if (!m_validated)
    {
		// We need to check for a valid initial payload header size before attempting to resize
		// the read buffer. The very first command received from the subscriber should be the
		// DefineOperationalModes command. The packet payload size for this command, if any,
		// should be a parameter string. Longer message sizes would be considered suspect data,
		// likely from a non-STTP based client connection. In context of this initial command
		// message, anything larger than 8KB of payload is considered suspect and will be
		// evaluated as a non-STTP type request.
		constexpr uint32_t MaxInitialPacketSize = Common::PayloadHeaderSize + 1 + 8192;

        if (packetSize > MaxInitialPacketSize)
        {
            stringstream messageStream;

            messageStream << "Possible invalid protocol detected from client \"";
            messageStream << m_connectionID;
            messageStream << "\": encountered request for ";
            messageStream << packetSize;
            messageStream << " byte initial packet size -- connection likely from non-STTP client -- disconnecting client.";

            m_parent->DispatchErrorMessage(messageStream.str());

            Stop();
            return;
        }
    }

    if (packetSize > ConvertUInt32(m_readBuffer.size()))
        m_readBuffer.resize(packetSize);

    // Read packet (payload body)
    // This read method is guaranteed not to return until the
    // requested size has been read or an error has occurred.
    async_read(m_commandChannelSocket, buffer(m_readBuffer, packetSize), [this]<typename T0, typename T1>(T0&& error, T1&& bytesTransferred) // NOLINT
    {
        ParseCommand(error, bytesTransferred);
    });
}

void SubscriberConnection::ParseCommand(const ErrorCode& error, const size_t bytesTransferred)
{
    if (m_stopped || !m_connectionAccepted)
        return;

    // Stop cleanly, i.e., don't report, on these errors
    if (error == error::connection_aborted || error == error::connection_reset || error == error::eof)
    {
        HandleConnectionError();
        return;
    }

    if (error)
    {
        stringstream messageStream;

        messageStream << "Error reading data from client \"";
        messageStream << m_connectionID;
        messageStream << "\" command channel: ";
        messageStream << SystemError(error).what();

        m_parent->DispatchErrorMessage(messageStream.str());

        HandleConnectionError();
        return;
    }

    try
    {
        uint8_t* data = m_readBuffer.data();
        const uint8_t command = data[0];
        const uint32_t length = ConvertInt32(bytesTransferred);

        // Forward data position beyond command byte
        data++;

        if (!m_validated && command != ServerCommand::DefineOperationalModes)
        {
            stringstream messageStream;

            messageStream << "Requested server command ";
            messageStream << ServerCommand::ToString(command);
            messageStream << " from client \"";
            messageStream << m_connectionID;
            messageStream << "\" rejected before operational modes validation -- possible non-STTP client -- disconnecting client";

            m_parent->DispatchErrorMessage(messageStream.str());

            Stop();
            return;
        }

        switch (command)
        {
            case ServerCommand::Subscribe:
                HandleSubscribe(data, length);
                break;
            case ServerCommand::Unsubscribe:
                HandleUnsubscribe();
                break;
            case ServerCommand::MetadataRefresh:
                HandleMetadataRefresh(data, length);
                break;
            case ServerCommand::RotateCipherKeys:
                HandleRotateCipherKeys();
                break;
            case ServerCommand::UpdateProcessingInterval:
                HandleUpdateProcessingInterval(data, length);
                break;
            case ServerCommand::DefineOperationalModes:
                HandleDefineOperationalModes(data, length);
                break;
            case ServerCommand::ConfirmUpdateSignalIndexCache:
                HandleConfirmUpdateSignalIndexCache(data, length);
                break;
            case ServerCommand::ConfirmNotification:
                HandleConfirmNotification(data, length);
                break;
            case ServerCommand::ConfirmBufferBlock:
                HandleConfirmBufferBlock(data, length);
                break;
            case ServerCommand::ConfirmUpdateBaseTimes:
                HandleConfirmUpdateBaseTimes(data, length);
                break;
            case ServerCommand::UserCommand00:
            case ServerCommand::UserCommand01:
            case ServerCommand::UserCommand02:
            case ServerCommand::UserCommand03:
            case ServerCommand::UserCommand04:
            case ServerCommand::UserCommand05:
            case ServerCommand::UserCommand06:
            case ServerCommand::UserCommand07:
            case ServerCommand::UserCommand08:
            case ServerCommand::UserCommand09:
            case ServerCommand::UserCommand10:
            case ServerCommand::UserCommand11:
            case ServerCommand::UserCommand12:
            case ServerCommand::UserCommand13:
            case ServerCommand::UserCommand14:
            case ServerCommand::UserCommand15:
                HandleUserCommand(command, data, length);
                break;
            default:
            {
                stringstream messageStream;

                messageStream << "\"" << m_connectionID << "\"";
                messageStream << " sent an unrecognized server command: ";
                messageStream << ServerCommand::ToString(command);

                const string message = messageStream.str();
                SendResponse(ServerResponse::Failed, command, message);
                m_parent->DispatchErrorMessage(message);
                break;
            }
        }
    }
    catch (const std::exception& ex)
    {
        m_parent->DispatchErrorMessage("Encountered an exception while processing received client data: " + string(ex.what()));
    }

    ReadCommandChannel();
}

std::vector<uint8_t> SubscriberConnection::SerializeSignalIndexCache(const SignalIndexCache& signalIndexCache) const
{
    vector<uint8_t> serializationBuffer;

    const uint32_t operationalModes = GetOperationalModes();
    const bool compressSignalIndexCache = (operationalModes & OperationalModes::CompressSignalIndexCache) > 0;
    const bool useGZipCompression = compressSignalIndexCache && (operationalModes & CompressionModes::GZip) > 0;

    uint32_t binaryLength = signalIndexCache.GetBinaryLength();

    if (m_version > 1)
        binaryLength++;

    serializationBuffer.reserve(static_cast<size_t>(binaryLength * 0.02));

    if (m_version > 1 && !useGZipCompression)
        serializationBuffer.push_back(UInt8::MaxValue); // Place-holder - actual value updated inside lock

    signalIndexCache.Encode(*this, serializationBuffer);

    if (useGZipCompression)
    {
        const MemoryStream memoryStream(serializationBuffer);
        StreamBuffer streamBuffer;

        streamBuffer.push(GZipCompressor());
        streamBuffer.push(memoryStream);

        vector<uint8_t> compressedBuffer;

        if (m_version > 1)
            compressedBuffer.push_back(UInt8::MaxValue);

        CopyStream(&streamBuffer, compressedBuffer);
        return compressedBuffer;
    }

    return serializationBuffer;
}

std::vector<uint8_t> SubscriberConnection::SerializeMetadata(const sttp::data::DataSetPtr& metadata) const
{
    vector<uint8_t> serializationBuffer;

    const uint32_t operationalModes = GetOperationalModes();
    const bool compressMetadata = (operationalModes & OperationalModes::CompressMetadata) > 0;
    const bool useGZipCompression = compressMetadata && (operationalModes & CompressionModes::GZip) > 0;

    metadata->WriteXml(serializationBuffer);

    if (useGZipCompression)
    {
        const MemoryStream memoryStream(serializationBuffer);
        StreamBuffer streamBuffer;

        streamBuffer.push(GZipCompressor());
        streamBuffer.push(memoryStream);

        vector<uint8_t> compressionBuffer;
        CopyStream(&streamBuffer, compressionBuffer);

        return compressionBuffer;
    }

    return serializationBuffer;
}

DataSetPtr SubscriberConnection::FilterClientMetadata(const StringMap<ExpressionTreePtr>& filterExpressions) const
{
    if (filterExpressions.empty())
        return m_parent->m_metadata;

    DataSetPtr dataSet = NewSharedPtr<DataSet>();
    const vector<DataTablePtr> tables = m_parent->m_metadata->Tables();

    for (size_t i = 0; i < tables.size(); i++)
    {
        const DataTablePtr& table = tables[i];
        const DataTablePtr filteredTable = dataSet->CreateTable(table->Name());
        ExpressionTreePtr expression;

        for (int32_t j = 0; j < table->ColumnCount(); j++)
            filteredTable->AddColumn(filteredTable->CloneColumn(table->Column(j)));

        if (TryGetValue<ExpressionTreePtr>(filterExpressions, table->Name(), expression, nullptr))
        {
            vector<DataRowPtr> matchedRows = FilterExpressionParser::Select(expression);

            for (size_t j = 0; j < matchedRows.size(); j++)
                filteredTable->AddRow(filteredTable->CloneRow(matchedRows[j]));
        }
        else
        {
            for (int32_t j = 0; j < table->RowCount(); j++)
                filteredTable->AddRow(filteredTable->CloneRow(table->Row(j)));
        }

        dataSet->AddOrUpdateTable(filteredTable);
    }

    return dataSet;
}

void SubscriberConnection::CommandChannelSendAsync()
{
    if (IsDisconnecting())
        return;

    vector<uint8_t>& data = *m_tcpWriteBuffers[0];

    async_write(m_commandChannelSocket, buffer(data.data(), data.size()), bind_executor(m_tcpWriteStrand, [this]<typename T0, typename T1>(T0&& error, T1&& bytesTransferred)
    {
        CommandChannelWriteHandler(error, bytesTransferred);
    }));
}

void SubscriberConnection::CommandChannelWriteHandler(const ErrorCode& error, const size_t bytesTransferred)
{
    if (IsDisconnecting())
        return;

    m_tcpWriteBuffers.pop_front();

    // Stop cleanly, i.e., don't report, on these errors
    if (error == error::connection_aborted || error == error::connection_reset || error == error::eof)
    {
        HandleConnectionError();
        return;
    }

    if (error)
    {
        stringstream messageStream;

        messageStream << "Error writing data to client \"";
        messageStream << m_connectionID;
        messageStream << "\" command channel: ";
        messageStream << SystemError(error).what();

        m_parent->DispatchErrorMessage(messageStream.str());

        HandleConnectionError();
        return;
    }

    if (!m_tcpWriteBuffers.empty())
        CommandChannelSendAsync();
}

void SubscriberConnection::DataChannelSendAsync()
{
    if (IsDisconnecting())
        return;

    vector<uint8_t>& data = *m_udpWriteBuffers[0];

    m_dataChannelSocket.async_send(buffer(data.data(), data.size()), bind_executor(m_udpWriteStrand, [this]<typename T0, typename T1>(T0&& error, T1&& bytesTransferred)
    {
        DataChannelWriteHandler(error, bytesTransferred);
    }));
}

void SubscriberConnection::DataChannelWriteHandler(const ErrorCode& error, const size_t bytesTransferred)
{
    if (IsDisconnecting())
        return;

    m_udpWriteBuffers.pop_front();

    // Stop cleanly, i.e., don't report, on these errors
    if (error == error::connection_aborted || error == error::connection_reset || error == error::eof)
    {
        HandleConnectionError();
        return;
    }

    if (error)
    {
        stringstream messageStream;

        messageStream << "Error writing data to client \"";
        messageStream << m_connectionID;
        messageStream << "\" command channel: ";
        messageStream << SystemError(error).what();

        m_parent->DispatchErrorMessage(messageStream.str());

        HandleConnectionError();
        return;
    }

    if (!m_udpWriteBuffers.empty())
        DataChannelSendAsync();
}

bool SubscriberConnection::SendResponse(const uint8_t responseCode, const uint8_t commandCode)
{
    return SendResponse(responseCode, commandCode, vector<uint8_t>(0));
}

bool SubscriberConnection::SendResponse(const uint8_t responseCode, const uint8_t commandCode, const string& message)
{
    const vector<uint8_t> data = EncodeString(message);
    return SendResponse(responseCode, commandCode, data);
}

bool SubscriberConnection::SendResponse(const uint8_t responseCode, const uint8_t commandCode, const vector<uint8_t>& data)
{
    bool success = false;

    try
    {
        const bool useDataChannel = m_dataChannelActive && (responseCode == ServerResponse::DataPacket || responseCode == ServerResponse::BufferBlock);
        const uint32_t packetSize = ConvertUInt32(data.size() + Common::ResponseHeaderSize);
        SharedPtr<vector<uint8_t>> bufferPtr = NewSharedPtr<vector<uint8_t>>();
        vector<uint8_t>& buffer = *bufferPtr;
        
        if (useDataChannel)
        {
            buffer.reserve(packetSize);
        }
        else
        {
            // Add response payload size for TCP channels to handle interleaved data reception
            buffer.reserve(static_cast<size_t>(packetSize) + Common::PayloadHeaderSize);
            EndianConverter::WriteBigEndianBytes(buffer, packetSize);
        }

        // Add response code
        buffer.push_back(responseCode);

        // Add original in response to command code
        buffer.push_back(commandCode);

        if (data.empty())
        {
            // Add zero sized data buffer to response packet
            WriteBytes(buffer, static_cast<uint32_t>(0));
        }
        else
        {
            // Future use case should implement UDP AES data packet encryption:
            //if (useDataChannel && CipherKeysDefined())

            // Add size of data buffer to response packet
            EndianConverter::WriteBigEndianBytes(buffer, ConvertInt32(data.size()));

            // Write data buffer
            WriteBytes(buffer, data);
        }

        // Data packets and buffer blocks can be published on a UDP data channel, so check for this...
        if (useDataChannel)
        {
            m_totalDataChannelBytesSent += buffer.size();

            post(m_udpWriteStrand, [this, bufferPtr] {
                m_udpWriteBuffers.push_back(bufferPtr);

                if (m_udpWriteBuffers.size() == 1)
                    DataChannelSendAsync();
            });

            m_dataChannelWaitHandle.notify_all();
        }
        else
        {
            m_totalCommandChannelBytesSent += buffer.size();

            post(m_tcpWriteStrand, [this, bufferPtr] {
                m_tcpWriteBuffers.push_back(bufferPtr);

                if (m_tcpWriteBuffers.size() == 1)
                    CommandChannelSendAsync();
            });
        }

        success = true;
    }
    catch (...)
    {
        m_parent->DispatchErrorMessage("Failed to send subscriber response: " + boost::current_exception_diagnostic_information(true));
    }

    return success;
}

string SubscriberConnection::DecodeString(const uint8_t* data, const uint32_t offset, const uint32_t length) const
{
    bool swapBytes = EndianConverter::IsBigEndian();

    switch (m_encoding)
    {
        case OperationalEncoding::UTF8:
            return string(reinterpret_cast<char*>(const_cast<uint8_t*>(&data[offset])), length / sizeof(char));
        case OperationalEncoding::UTF16BE:
            // UTF16 in C++ is encoded as big-endian
            swapBytes = !swapBytes;
            [[fallthrough]];
        case OperationalEncoding::UTF16LE:
        {
            // On Windows sizeof(wchar_t) == 2 and on Linux and OS X sizeof(wchar_t) == 4, so we do not use
            // sizeof(wchar_t) to infer number of encoded bytes per wchar_t, which is always 2:
            static constexpr uint32_t enc_sizeof_wchar = 2U;
            wstring value(length / enc_sizeof_wchar, L'\0');

            for (uint32_t i = 0, j = 0; i < length; i += enc_sizeof_wchar, j++)
            {
                uint16_t utf16char;

                if (swapBytes)
                    utf16char = EndianConverter::ToBigEndian<uint16_t>(data, offset + i);
                else
                    utf16char = *reinterpret_cast<const uint16_t*>(&data[offset + i]);

                value[j] = static_cast<wchar_t>(utf16char);
            }

            return ToUTF8(value);
        }
        default:
            throw PublisherException("Encountered unexpected operational encoding " + ToHex(m_encoding));
    }
}

vector<uint8_t> SubscriberConnection::EncodeString(const string& value) const
{
    bool swapBytes = EndianConverter::IsBigEndian();
    vector<uint8_t> result {};

    switch (m_encoding)
    {
        case OperationalEncoding::UTF8:
            result.reserve(value.size() * sizeof(char));
            result.assign(value.begin(), value.end());
            break;
        case OperationalEncoding::UTF16BE:
            // UTF16 in C++ is encoded as big-endian
            swapBytes = !swapBytes;
            [[fallthrough]];
        case OperationalEncoding::UTF16LE:
        {
            // On Windows sizeof(wchar_t) == 2 and on Linux and OS X sizeof(wchar_t) == 4, so we do not use
            // sizeof(wchar_t) to infer number of encoded bytes per wchar_t, which is always 2:
            static constexpr uint32_t enc_sizeof_wchar = 2;
            const wstring utf16 = ToUTF16(value);            
            result.reserve(utf16.size() * enc_sizeof_wchar);

            for (size_t i = 0; i < utf16.size(); i++)
            {
                // Convert wchar_t, which can be 4 bytes, to a 2 byte uint16_t
                const uint16_t utf16char = static_cast<uint16_t>(utf16[i]);
                const uint8_t* data = reinterpret_cast<const uint8_t*>(&utf16char);

                if (swapBytes)
                {
                    result.push_back(data[1]);
                    result.push_back(data[0]);
                }
                else
                {
                    result.push_back(data[0]);
                    result.push_back(data[1]);
                }
            }

            break;
        }
        default:
            throw PublisherException("Encountered unexpected operational encoding " + ToHex(m_encoding));
    }

    return result;
}

void SubscriberConnection::PingTimerElapsed(const TimerPtr&, void* userData)
{
    SubscriberConnection* connection = static_cast<SubscriberConnection*>(userData);

    if (connection == nullptr)
        return;

    if (!connection->m_stopped)
        connection->SendResponse(ServerResponse::NoOP, ServerCommand::Subscribe);
}
