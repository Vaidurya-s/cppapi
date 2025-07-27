//******************************************************************************************************
//  AdvancedPublish.cpp - Gbtc
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
//  02/14/2019 - J. Ritchie Carroll
//       Generated original version of source code.
//
//******************************************************************************************************

// ReSharper disable CppAssignedValueIsNeverUsed
// ReSharper disable CppClangTidyConcurrencyMtUnsafe
#include "../../lib/transport/DataPublisher.h"
#include "GenHistory.h"
#include "TemporalSubscriber.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>

using namespace std;
using namespace sttp;
using namespace sttp::data;
using namespace sttp::transport;
using namespace sttp::filterexpressions;

DataPublisherPtr Publisher;
GenHistoryPtr HistoryGenerator;
TimerPtr PublishTimer;
vector<DeviceMetadataPtr> DevicesToPublish;
vector<MeasurementMetadataPtr> MeasurementsToPublish;
vector<PhasorMetadataPtr> PhasorsToPublish;
unordered_map<sttp::Guid, TemporalSubscriberPtr> TemporalSubscriptions;
Mutex TemporalSubscriptionsLock;
const TemporalSubscriberPtr NullTemporalSubscription = nullptr;

// Dynamic Rate Synchronization - Publisher rate adjustment
struct SubscriberFeedback
{
    size_t queueSize;
    double processingLag;
    string rateRecommendation;
    uint64_t totalProcessed;
    chrono::steady_clock::time_point lastUpdateTime;
    
    SubscriberFeedback() : queueSize(0), processingLag(0.0), rateRecommendation("NORMAL"), 
                          totalProcessed(0), lastUpdateTime(chrono::steady_clock::now()) {}
};

unordered_map<string, SubscriberFeedback> subscriberFeedbackMap; // Use connection ID as key
Mutex feedbackMapMutex;
int32_t currentPublishInterval = 33; // Default 33ms interval
int32_t basePublishInterval = 33;    // Base interval for scaling

// Function declarations for feedback handling
void HandleUserCommand(DataPublisher* source, const SubscriberConnectionPtr& connection, uint8_t commandCode, const vector<uint8_t>& data);
void ProcessFeedbackMessage(const SubscriberConnectionPtr& connection, const string& feedbackData);
void UpdatePublishingRate();
SubscriberFeedback ParseFeedbackMessage(const string& message);

bool RunPublisher(uint16_t port, bool genHistory);

void DisplayClientConnected(DataPublisher* source, const SubscriberConnectionPtr& connection);
void DisplayClientDisconnected(DataPublisher* source, const SubscriberConnectionPtr& connection);
void DisplayStatusMessage(DataPublisher* source, const string& message);
void DisplayErrorMessage(DataPublisher* source, const string& message);
void HandleProcessingIntervalChangeRequested(DataPublisher* source, const SubscriberConnectionPtr& connection);
void HandleTemporalSubscriptionRequested(DataPublisher* source, const SubscriberConnectionPtr& connection);
void HandleTemporalSubscriptionCanceled(DataPublisher* source, const SubscriberConnectionPtr& connection);

bool UpdateTemporalSubscriptionProcessingInterval(const SubscriberConnectionPtr& connection);
TemporalSubscriberPtr CreateNewTemporalSubscription(const SubscriberConnectionPtr& connection);
bool RemoveTemporalSubscription(const SubscriberConnectionPtr& connection, bool& completed);

void LoadMetadataToPublish(vector<DeviceMetadataPtr>& deviceMetadata, vector<MeasurementMetadataPtr>& measurementMetadata, vector<PhasorMetadataPtr>& phasorMetadata)
{
    DeviceMetadataPtr device1Metadata = NewSharedPtr<DeviceMetadata>();
    const datetime_t timestamp = UtcNow();

    // Add a device
    device1Metadata->Name = "Test PMU";
    device1Metadata->Acronym = ToUpper(Replace(device1Metadata->Name, " ", "", false));
    device1Metadata->UniqueID = ParseGuid("933690ab-71e1-4c56-ab54-097f5ed8db34");
    device1Metadata->Longitude = 300;
    device1Metadata->Latitude = 200;
    device1Metadata->FramesPerSecond = 30;
    device1Metadata->ProtocolName = "STTP";
    device1Metadata->UpdatedOn = timestamp;

    deviceMetadata.emplace_back(device1Metadata);

    const string& pointTagPrefix = device1Metadata->Acronym + ".";
    const string& measurementSource = "PPA:";
    int runtimeIndex = 1;

    // Add a frequency measurement
    MeasurementMetadataPtr measurement1Metadata = NewSharedPtr<MeasurementMetadata>();
    measurement1Metadata->ID = measurementSource + ToString(runtimeIndex++);
    measurement1Metadata->PointTag = pointTagPrefix + "FREQ";
    measurement1Metadata->SignalID = ParseGuid("6586f230-8e7f-4f0f-9e18-1eefee4b9edd");
    measurement1Metadata->DeviceAcronym = device1Metadata->Acronym;
    measurement1Metadata->Reference.Acronym = device1Metadata->Acronym;
    measurement1Metadata->Reference.Kind = SignalKind::Frequency;
    measurement1Metadata->Reference.Index = 0;
    measurement1Metadata->PhasorSourceIndex = 0;
    measurement1Metadata->UpdatedOn = timestamp;

    // Add a dF/dt measurement
    MeasurementMetadataPtr measurement2Metadata = NewSharedPtr<MeasurementMetadata>();
    measurement2Metadata->ID = measurementSource + ToString(runtimeIndex++);
    measurement2Metadata->PointTag = pointTagPrefix + "DFDT";
    measurement2Metadata->SignalID = ParseGuid("60c97530-2ed2-4abb-a7a2-99e2170479a4");
    measurement2Metadata->DeviceAcronym = device1Metadata->Acronym;
    measurement2Metadata->Reference.Acronym = device1Metadata->Acronym;
    measurement2Metadata->Reference.Kind = SignalKind::DfDt;
    measurement2Metadata->Reference.Index = 0;
    measurement2Metadata->PhasorSourceIndex = 0;
    measurement2Metadata->UpdatedOn = timestamp;

    // Add a phase angle measurement
    MeasurementMetadataPtr measurement3Metadata = NewSharedPtr<MeasurementMetadata>();
    measurement3Metadata->ID = measurementSource + ToString(runtimeIndex++);
    measurement3Metadata->PointTag = pointTagPrefix + "VPHA";
    measurement3Metadata->SignalID = ParseGuid("aa47a61c-8596-46af-8c28-f9ee774bcf26");
    measurement3Metadata->DeviceAcronym = device1Metadata->Acronym;
    measurement3Metadata->Reference.Acronym = device1Metadata->Acronym;
    measurement3Metadata->Reference.Kind = SignalKind::Angle;
    measurement3Metadata->Reference.Index = 1;   // First phase angle
    measurement3Metadata->PhasorSourceIndex = 1; // Match to Phasor.SourceIndex = 1
    measurement3Metadata->UpdatedOn = timestamp;

    // Add a phase magnitude measurement
    MeasurementMetadataPtr measurement4Metadata = NewSharedPtr<MeasurementMetadata>();
    measurement4Metadata->ID = measurementSource + ToString(runtimeIndex++);
    measurement4Metadata->PointTag = pointTagPrefix + "VPHM";
    measurement4Metadata->SignalID = ParseGuid("4ab24720-3763-407c-afa0-15f0d69ac897");
    measurement4Metadata->DeviceAcronym = device1Metadata->Acronym;
    measurement4Metadata->Reference.Acronym = device1Metadata->Acronym;
    measurement4Metadata->Reference.Kind = SignalKind::Magnitude;
    measurement4Metadata->Reference.Index = 1;   // First phase magnitude
    measurement4Metadata->PhasorSourceIndex = 1; // Match to Phasor.SourceIndex = 1
    measurement4Metadata->UpdatedOn = timestamp;

    measurementMetadata.emplace_back(measurement1Metadata);
    measurementMetadata.emplace_back(measurement2Metadata);
    measurementMetadata.emplace_back(measurement3Metadata);
    measurementMetadata.emplace_back(measurement4Metadata);

    // Add a phasor
    PhasorMetadataPtr phasor1Metadata = NewSharedPtr<PhasorMetadata>();
    phasor1Metadata->DeviceAcronym = device1Metadata->Acronym;
    phasor1Metadata->Label = device1Metadata->Name + " Voltage Phasor";
    phasor1Metadata->Type = "V";      // Voltage phasor
    phasor1Metadata->Phase = "+";     // Positive sequence
    phasor1Metadata->SourceIndex = 1; // Phasor number 1
    phasor1Metadata->UpdatedOn = timestamp;

    phasorMetadata.emplace_back(phasor1Metadata);
}

// Sample application to demonstrate the more advanced use of the publisher API.
//
// This application accepts the port of the publisher via command line argument,
// starts listening for subscriber connections, the displays summary information
// about the measurements it publishes. It provides four manually defined
// measurements, i.e., PPA:1 through PPA:4
//
// Measurements are transmitted via the TCP command channel.
//
// DYNAMIC RATE SYNCHRONIZATION FEATURE:
// =====================================
// The publisher now implements dynamic rate synchronization with subscribers:
// 
// 1. Listens for feedback messages from subscribers via UserCommand01
// 2. Aggregates feedback from all active subscribers (queue size, lag, recommendations)
// 3. Adjusts publishing timer interval based on majority subscriber recommendations:
//    - SLOW: Increases interval by 20% (max 500ms) when subscribers are overloaded
//    - READY: Decreases interval by 15% (min 10ms) when subscribers can handle more
//    - NORMAL: Gradually returns to base 33ms interval
// 4. Logs all interval changes with subscriber statistics
// 5. Removes stale feedback after 10 seconds of inactivity
//
// Usage Instructions:
// - Run AdvancedPublish PORT (e.g., AdvancedPublish 7165)
// - Connect multiple AdvancedSubscribe clients
// - Observe automatic rate adjustments based on subscriber load
// - Monitor console output for rate change notifications
int main(int argc, char* argv[])
{
    uint16_t port;

    // Ensure that the necessary
    // command line arguments are given.
    if (argc < 2)
    {
        cout << "Usage:" << endl;
        cout << "    AdvancedPublish PORT" << endl;
        return 0;
    }

    // Get hostname and port.
    stringstream(argv[1]) >> port;
    const bool genHistory = argc > 2 && IsEqual(argv[2], "GenHistory");

    // Run the publisher.
    const bool publisherRunning = RunPublisher(port, genHistory);

    // Wait until the user presses enter before quitting.
    string line;
    getline(cin, line);

    // Stop data publication
    if (publisherRunning)
        PublishTimer->Stop();

    if (genHistory)
        HistoryGenerator->StopArchive();

    cout << "Publisher stopped." << endl;

    return 0;
}

bool RunPublisher(const uint16_t port, const bool genHistory)
{
    constexpr float64_t randMax = RAND_MAX;
    string errorMessage;
    bool running = false;

    try
    {
        Publisher = NewSharedPtr<DataPublisher>();
        Publisher->Start(port);
        running = true;
    }
    catch (PublisherException& ex)
    {
        errorMessage = ex.what();
    }
    catch (SystemError& ex)
    {
        errorMessage = ex.what();
    }
    catch (...)
    {
        errorMessage = boost::current_exception_diagnostic_information(true);
    }

    if (running)
    {
        cout << endl << "Listening on port: " << port << "..." << endl << endl;

        // Register callbacks
        Publisher->RegisterClientConnectedCallback(&DisplayClientConnected);
        Publisher->RegisterClientDisconnectedCallback(&DisplayClientDisconnected);
        Publisher->RegisterStatusMessageCallback(&DisplayStatusMessage);
        Publisher->RegisterErrorMessageCallback(&DisplayErrorMessage);
        Publisher->RegisterProcessingIntervalChangeRequestedCallback(&HandleProcessingIntervalChangeRequested);
        Publisher->RegisterTemporalSubscriptionRequestedCallback(&HandleTemporalSubscriptionRequested);
        Publisher->RegisterTemporalSubscriptionCanceledCallback(&HandleTemporalSubscriptionCanceled);
        
        // Dynamic Rate Synchronization: Register user command callback for feedback
        Publisher->RegisterUserCommandCallback(&HandleUserCommand);

        // Enable temporal subscription support - this allows historical data requests as well as real-time
        Publisher->SetSupportsTemporalSubscriptions(true);

        // Load metadata to be used for publication
        LoadMetadataToPublish(DevicesToPublish, MeasurementsToPublish, PhasorsToPublish);
        Publisher->DefineMetadata(DevicesToPublish, MeasurementsToPublish, PhasorsToPublish);

        cout << "Loaded " << MeasurementsToPublish.size() << " measurement metadata records for publication:" << endl;

        for (size_t i = 0; i < MeasurementsToPublish.size(); i++)
            cout << "    " << MeasurementsToPublish[i]->PointTag << std::endl;

        cout << endl;

        // Setup data publication timer - for this publishing sample we send
        // data type reasonable random values every 33 milliseconds
        PublishTimer = NewSharedPtr<Timer>(33, [](const TimerPtr&, void*)
        {
            // If metadata can change, the following integer should not be static:
            static uint32_t count = ConvertUInt32(MeasurementsToPublish.size());
            const int64_t timestamp = RoundToSubsecondDistribution(ToTicks(UtcNow()), 30);
            vector<MeasurementPtr> measurements;

            measurements.reserve(count);

            // Create new measurement values for publication
            for (size_t i = 0; i < count; i++)
            {
                const MeasurementMetadataPtr metadata = MeasurementsToPublish[i];
                MeasurementPtr measurement = NewSharedPtr<Measurement>();

                measurement->SignalID = metadata->SignalID;
                measurement->Timestamp = timestamp;

                const float64_t randFraction = rand() / randMax;
                const float64_t sign = randFraction > 0.5 ? 1.0 : -1.0;
                float64_t value;

                switch (metadata->Reference.Kind)  // NOLINT
                {
                    case SignalKind::Frequency:
                        value = 60.0 + sign * randFraction * 0.1;
                        break;
                    case SignalKind::DfDt:
                        value = sign * randFraction * 2;
                        break;
                    case SignalKind::Magnitude:
                        value = 500 + sign * randFraction * 50;
                        break;
                    case SignalKind::Angle:
                        value = sign * randFraction * 180;
                        break;
                    default:
                        value = sign * randFraction * UInt32::MaxValue;
                        break;
                }

                measurement->Value = value;

                measurements.push_back(measurement);
            }

            // Publish measurements
            Publisher->PublishMeasurements(measurements);
        },
        true);

        // Start data publication
        PublishTimer->Start();

        if (genHistory)
        {
            HistoryGenerator = NewSharedPtr<GenHistory>(port);
            HistoryGenerator->StartArchive();
        }            
    }
    else
    {
        cerr << "Failed to listen on port: " << port << ": " << errorMessage;
    }

    return running;
}

void DisplayClientConnected(DataPublisher* source, const SubscriberConnectionPtr& connection)
{
    stringstream message;

    message << ">> New Client Connected:" << endl;
    message << "   Subscriber ID: " << ToString(connection->GetSubscriberID()) << endl;
    message << "   Connection ID: " << ToString(connection->GetConnectionID());

    DisplayStatusMessage(source, message.str());
}

void DisplayClientDisconnected(DataPublisher* source, const SubscriberConnectionPtr& connection)
{
    stringstream message;

    message << ">> Client Disconnected:" << endl;
    message << "   Subscriber ID: " << ToString(connection->GetSubscriberID()) << endl;
    message << "   Connection ID: " << ToString(connection->GetConnectionID());

    // Dynamic Rate Synchronization: Clean up feedback for disconnected client
    {
        lock_guard<Mutex> lock(feedbackMapMutex);
        subscriberFeedbackMap.erase(connection->GetConnectionID());
    }

    DisplayStatusMessage(source, message.str());
}

// Callback which is called to display status messages from the subscriber.
void DisplayStatusMessage(DataPublisher* source, const string& message)
{
    cout << message << endl << endl;
}

// Callback which is called to display error messages from the connector and subscriber.
void DisplayErrorMessage(DataPublisher* source, const string& message)
{
    cerr << message << endl << endl;
}

void HandleProcessingIntervalChangeRequested(DataPublisher* source, const SubscriberConnectionPtr& connection)
{
    if (!UpdateTemporalSubscriptionProcessingInterval(connection))
        return;

    stringstream message;

    message << "Client \"" << connection->GetConnectionID() << "\" with subscriber ID " << ToString(connection->GetSubscriberID()) << " has requested to change its temporal processing interval to " << ToString(connection->GetProcessingInterval()) << "ms";

    DisplayStatusMessage(source, message.str());
}

void HandleTemporalSubscriptionRequested(DataPublisher* source, const SubscriberConnectionPtr& connection)
{
    stringstream message;
    bool completed;
    
    message << "Client \"" << connection->GetConnectionID() << "\" with subscriber ID " << ToString(connection->GetSubscriberID()) << " has requested a temporal subscription starting at " << ToString(connection->GetStartTimeConstraint()) << endl;

    RemoveTemporalSubscription(connection, completed);
    
    if (CreateNewTemporalSubscription(connection))
    {
        const size_t count = TemporalSubscriptions.size();
        message << "Created new temporal subscription - " <<  count << (count == 1 ? " is" : " are") << " now active...";
    }

    DisplayStatusMessage(source, message.str());
}

void HandleTemporalSubscriptionCanceled(DataPublisher* source, const SubscriberConnectionPtr& connection)
{
    bool completed;

    if (!RemoveTemporalSubscription(connection, completed))
        return;

    stringstream message;
    const size_t count = TemporalSubscriptions.size();

    message << "Client \"" << connection->GetConnectionID() << "\" with subscriber ID " << ToString(connection->GetSubscriberID()) << (completed ? " completed" : " canceled") << " temporal subscription starting at " << ToString(connection->GetStartTimeConstraint()) << endl;
    message << "Temporal subscription removed - " << count << (count == 1 ? " is" : " are") << " now active...";

    DisplayStatusMessage(source, message.str());
}

bool UpdateTemporalSubscriptionProcessingInterval(const SubscriberConnectionPtr& connection)
{
    const sttp::Guid& instanceID = connection->GetInstanceID();
    TemporalSubscriberPtr temporalSubscription;
    const int32_t processingInterval = connection->GetProcessingInterval();
    bool updated = false;

    TemporalSubscriptionsLock.lock();

    if (TryGetValue(TemporalSubscriptions, instanceID, temporalSubscription, NullTemporalSubscription))
    {
        temporalSubscription->SetProcessingInterval(processingInterval);
        updated = true;
    }

    TemporalSubscriptionsLock.unlock();

    return updated;
}

TemporalSubscriberPtr CreateNewTemporalSubscription(const SubscriberConnectionPtr& connection)
{
    const sttp::Guid& instanceID = connection->GetInstanceID();

    TemporalSubscriptionsLock.lock();

    TemporalSubscriberPtr temporalSubscription = NewSharedPtr<TemporalSubscriber>(connection);
    TemporalSubscriptions.insert(pair(instanceID, temporalSubscription));

    TemporalSubscriptionsLock.unlock();

    return temporalSubscription;
}

bool RemoveTemporalSubscription(const SubscriberConnectionPtr& connection, bool& completed)
{
    const sttp::Guid& instanceID = connection->GetInstanceID();
    TemporalSubscriberPtr temporalSubscription;
    bool removed = false;

    TemporalSubscriptionsLock.lock();

    if (TryGetValue(TemporalSubscriptions, instanceID, temporalSubscription, NullTemporalSubscription))
    {
        completed = temporalSubscription->GetIsStopped();
        temporalSubscription->CompleteTemporalSubscription();
        TemporalSubscriptions.erase(instanceID);
        temporalSubscription.reset();
        removed = true;
    }

    TemporalSubscriptionsLock.unlock();

    return removed;
}

// Dynamic Rate Synchronization Implementation
// ==========================================

/**
 * Handles user commands from subscribers, including rate synchronization feedback.
 * @param source - The DataPublisher instance
 * @param connection - The subscriber connection sending the command
 * @param commandCode - The command code (UserCommand01 for rate feedback)
 * @param data - The command data payload
 */
void HandleUserCommand(DataPublisher* source, const SubscriberConnectionPtr& connection, uint8_t commandCode, const vector<uint8_t>& data)
{
    string commandData(data.begin(), data.end());
    
    switch (commandCode)
    {
        case ServerCommand::UserCommand00:
            // Handle existing custom command (hello message)
            cout << "Received custom command from client \"" << connection->GetConnectionID() 
                 << "\": " << commandData << endl << endl;
            break;
            
        case ServerCommand::UserCommand01:
            // Handle rate synchronization feedback
            ProcessFeedbackMessage(connection, commandData);
            break;
            
        default:
            cout << "Received unknown user command " << static_cast<int>(commandCode) 
                 << " from client \"" << connection->GetConnectionID() << "\"" << endl;
            break;
    }
}

/**
 * Processes feedback message from subscriber and updates rate control.
 * @param connection - The subscriber connection providing feedback
 * @param feedbackData - The feedback message string
 */
void ProcessFeedbackMessage(const SubscriberConnectionPtr& connection, const string& feedbackData)
{
    try
    {
        SubscriberFeedback feedback = ParseFeedbackMessage(feedbackData);
        
        {
            lock_guard<Mutex> lock(feedbackMapMutex);
            feedback.lastUpdateTime = chrono::steady_clock::now();
            subscriberFeedbackMap[connection->GetConnectionID()] = feedback;
        }
        
        // Update publishing rate based on aggregated feedback
        UpdatePublishingRate();
        
        cout << "Processed feedback from client \"" << connection->GetConnectionID() 
             << "\": queue=" << feedback.queueSize 
             << ", lag=" << feedback.processingLag << "ms"
             << ", recommendation=" << feedback.rateRecommendation << endl;
    }
    catch (const exception& ex)
    {
        cerr << "Error processing feedback from client \"" << connection->GetConnectionID() 
             << "\": " << ex.what() << endl;
    }
}

/**
 * Updates the publishing rate based on aggregated feedback from all subscribers.
 * Adjusts the timer interval to slow down or speed up based on recommendations.
 */
void UpdatePublishingRate()
{
    lock_guard<Mutex> lock(feedbackMapMutex);
    
    if (subscriberFeedbackMap.empty())
        return;
    
    // Count recommendations from active subscribers (updated within last 10 seconds)
    auto now = chrono::steady_clock::now();
    int slowCount = 0, normalCount = 0, readyCount = 0;
    int activeSubscribers = 0;
    
    for (auto it = subscriberFeedbackMap.begin(); it != subscriberFeedbackMap.end();)
    {
        auto timeSinceUpdate = chrono::duration_cast<chrono::seconds>(now - it->second.lastUpdateTime);
        
        if (timeSinceUpdate.count() > 10) // Remove stale feedback
        {
            it = subscriberFeedbackMap.erase(it);
            continue;
        }
        
        activeSubscribers++;
        const string& recommendation = it->second.rateRecommendation;
        
        if (recommendation == "SLOW") slowCount++;
        else if (recommendation == "READY") readyCount++;
        else normalCount++;
        
        ++it;
    }
    
    if (activeSubscribers == 0)
        return;
    
    // Determine new interval based on majority recommendation
    int32_t newInterval = currentPublishInterval;
    
    if (slowCount > activeSubscribers / 2)
    {
        // Majority wants slower rate - increase interval by 20% (max 500ms)
        newInterval = min(500, static_cast<int32_t>(currentPublishInterval * 1.2));
    }
    else if (readyCount > activeSubscribers / 2)
    {
        // Majority ready for faster rate - decrease interval by 15% (min 10ms)
        newInterval = max(10, static_cast<int32_t>(currentPublishInterval * 0.85));
    }
    else
    {
        // Normal or mixed feedback - gradually return to base interval
        if (currentPublishInterval > basePublishInterval)
            newInterval = max(basePublishInterval, static_cast<int32_t>(currentPublishInterval * 0.95));
        else if (currentPublishInterval < basePublishInterval)
            newInterval = min(basePublishInterval, static_cast<int32_t>(currentPublishInterval * 1.05));
    }
    
    // Update interval if it changed significantly (>5ms difference)
    if (abs(newInterval - currentPublishInterval) > 5)
    {
        currentPublishInterval = newInterval;
        PublishTimer->SetInterval(currentPublishInterval);
        
        cout << ">> RATE ADJUSTMENT: Publishing interval changed to " << currentPublishInterval 
             << "ms (Active subscribers: " << activeSubscribers 
             << ", SLOW: " << slowCount << ", NORMAL: " << normalCount 
             << ", READY: " << readyCount << ")" << endl << endl;
    }
}

/**
 * Parses feedback message in format: "RATE_FEEDBACK:queue_size=X,lag=Y,recommendation=Z,total_processed=W"
 * @param message - The feedback message string
 * @return SubscriberFeedback struct with parsed values
 */
SubscriberFeedback ParseFeedbackMessage(const string& message)
{
    SubscriberFeedback feedback;
    
    if (message.find("RATE_FEEDBACK:") != 0)
        throw runtime_error("Invalid feedback message format");
    
    string params = message.substr(14); // Remove "RATE_FEEDBACK:" prefix
    
    // Parse comma-separated key=value pairs
    stringstream ss(params);
    string param;
    
    while (getline(ss, param, ','))
    {
        size_t equalPos = param.find('=');
        if (equalPos == string::npos)
            continue;
            
        string key = param.substr(0, equalPos);
        string value = param.substr(equalPos + 1);
        
        if (key == "queue_size")
            feedback.queueSize = stoul(value);
        else if (key == "lag")
            feedback.processingLag = stod(value);
        else if (key == "recommendation")
            feedback.rateRecommendation = value;
        else if (key == "total_processed")
            feedback.totalProcessed = stoull(value);
    }
    
    return feedback;
}
