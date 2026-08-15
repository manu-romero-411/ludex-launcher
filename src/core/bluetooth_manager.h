#pragma once
#include <chrono>
#include <string>
#include <vector>

enum class BtDeviceKind { Unknown, Gamepad, Audio };

struct BluetoothDevice {
    std::string mac;
    std::string name;
    bool paired = false;
    bool connected = false;
    BtDeviceKind kind = BtDeviceKind::Unknown;
};

class BluetoothManager {
public:
    bool available() const;
    void refresh();
    void startScan(int timeout_sec = 12);
    void update();
    bool isScanning() const { return scanning_; }
    int  scanRemainingSec() const;

    bool pair(const std::string &mac);
    bool connect(const std::string &mac);
    bool disconnect(const std::string &mac);
    bool removeDevice(const std::string &mac);
    bool trust(const std::string &mac);

    const std::vector<BluetoothDevice> &devices() const { return devices_; }
    const std::vector<BluetoothDevice> &discoveredDevices() const { return discovered_devices_; }

    void setAutoRefresh(bool on);

private:
    bool auto_refresh_ = false;
    std::chrono::steady_clock::time_point last_refresh_;    static std::string runCmd(const std::string &cmd);
    void parseDevicesOutput(const std::string &output, std::vector<BluetoothDevice> &target);
    void refreshConnectedState();

    std::vector<BluetoothDevice> devices_;
    std::vector<BluetoothDevice> discovered_devices_;
    bool scanning_ = false;
    std::chrono::steady_clock::time_point scan_start_;
    int scan_timeout_sec_ = 12;
    std::chrono::steady_clock::time_point last_poll_;
};