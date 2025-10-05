#include "../../lib/transport/DataPublisher.h"
#include <iostream>
#include <sstream>
#include <thread>

using namespace std;
using namespace sttp;
using namespace sttp::data;
using namespace sttp::transport;

DataPublisherPtr Publisher;
vector<MeasurementMetadataPtr> MeasurementsToPublish;
bool isRunning = false;

void LoadMetadata()
{
    // Create device metadata
    vector<DeviceMetadataPtr> deviceMetadata;
    DeviceMetadataPtr device = NewSharedPtr<DeviceMetadata>();
    device->Name = "Terminal PMU";
    device->Acronym = "TERM";
    device->UniqueID = ParseGuid("12345678-1234-1234-1234-123456789abc");
    device->FramesPerSecond = 30;
    device->ProtocolName = "STTP";
    device->UpdatedOn = UtcNow();
    deviceMetadata.emplace_back(device);

    // Create frequency measurement metadata
    MeasurementMetadataPtr freqMeasurement = NewSharedPtr<MeasurementMetadata>();
    freqMeasurement->ID = "PPA:1";
    freqMeasurement->PointTag = "TERM.FREQ";
    freqMeasurement->SignalID = ParseGuid("11111111-1111-1111-1111-111111111111");
    freqMeasurement->DeviceAcronym = "TERM";
    freqMeasurement->Reference.Acronym = "TERM";
    freqMeasurement->Reference.Kind = SignalKind::Frequency;
    freqMeasurement->Reference.Index = 0;
    freqMeasurement->UpdatedOn = UtcNow();
    
    MeasurementsToPublish.emplace_back(freqMeasurement);

    // Define metadata
    vector<PhasorMetadataPtr> phasorMetadata; // Empty
    Publisher->DefineMetadata(deviceMetadata, MeasurementsToPublish, phasorMetadata);
}

void PublishFrequency(double frequency)
{
    if (!isRunning) return;

    const int64_t timestamp = ToTicks(UtcNow());
    vector<MeasurementPtr> measurements;

    MeasurementPtr measurement = NewSharedPtr<Measurement>();
    measurement->SignalID = MeasurementsToPublish[0]->SignalID;
    measurement->Timestamp = timestamp;
    measurement->Value = frequency;
    
    measurements.push_back(measurement);
    Publisher->PublishMeasurements(measurements);
    
    cout << "Published frequency: " << frequency << " Hz" << endl;
}

void StatusMessage(DataPublisher* source, const string& message)
{
    cout << "Status: " << message << endl;
}

void ErrorMessage(DataPublisher* source, const string& message)
{
    cerr << "Error: " << message << endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cout << "Usage: BasicPublish PORT" << endl;
        return 0;
    }

    uint16_t port;
    stringstream(argv[1]) >> port;

    try
    {
        // Create and start publisher
        Publisher = NewSharedPtr<DataPublisher>();
        Publisher->RegisterStatusMessageCallback(&StatusMessage);
        Publisher->RegisterErrorMessageCallback(&ErrorMessage);
        Publisher->Start(port);
        isRunning = true;

        cout << "Publisher started on port " << port << endl;

        // Load metadata
        LoadMetadata();
        cout << "Metadata loaded. Ready to publish frequency measurements." << endl;
        cout << "Enter frequency values (or 'quit' to exit):" << endl;

        string input;
        while (isRunning && getline(cin, input))
        {
            if (input == "quit" || input == "exit")
            {
                break;
            }

            try
            {
                double frequency = stod(input);
                PublishFrequency(frequency);
            }
            catch (...)
            {
                cout << "Invalid input. Please enter a number or 'quit'." << endl;
            }
        }
    }
    catch (const exception& ex)
    {
        cerr << "Error: " << ex.what() << endl;
    }

    isRunning = false;
    cout << "Publisher stopped." << endl;
    return 0;
}