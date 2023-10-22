#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <sstream>
#include <fstream>
#include <regex>
#include<bits/stdc++.h>
#include "json-1.hpp"
#include<fstream>

using namespace std;
using json = nlohmann::json;
//using json = nlohmann::json;

namespace asio = boost::asio;
namespace ip = asio::ip;
namespace websocket = boost::beast::websocket;

// SystemInfoProvider class provides methods to collect system information.
class SystemInfoProvider {
public:
    string getSystemName() const {
        return exec("hostname");      //Returns the system's hostname.
    }



    // Updated method to get network statistics as a formatted string
    std::pair<long long, long long> getNetworkUsage(const std::string& interfaceName) const {
    std::string command = "ifconfig " + interfaceName + " | grep 'RX packets\\|TX packets'";
    std::istringstream networkUsageStream(exec(command.c_str()));

    std::string line;
    long long rxPackets = 0;
    long long txPackets = 0;

    while (std::getline(networkUsageStream, line)) {
        // Search for RX packets and TX packets lines
        if (line.find("RX packets") != std::string::npos) {
            // Extract RX packets count
            size_t pos = line.find("RX packets");
            rxPackets = std::stoll(line.substr(pos + 11));
        } else if (line.find("TX packets") != std::string::npos) {
            // Extract TX packets count
            size_t pos = line.find("TX packets");
            txPackets = std::stoll(line.substr(pos + 11));
        }
    } 
    return std::make_pair(rxPackets, txPackets); // Returns a pair of received (RX) and transmitted (TX) packet counts.
}



    double getCpuUtilization() const {
        istringstream cpuUtilizationStream(exec("top -bn1 | grep 'Cpu(s)' | sed 's/.*, *\\([0-9.]*\\)%* id.*/\\1/'"));
        double cpuUtilization;
        cpuUtilizationStream >> cpuUtilization;
        return cpuUtilization;  // Retrieves the current CPU utilization as a percentage.
    }

    double getRamUsage() const {
        istringstream ramUsageStream(exec("free -m | awk 'NR==2{print $3}'"));
        double ramUsage;
        ramUsageStream >> ramUsage;
        return ramUsage;   // Retrieves the RAM usage in megabytes.
    }
    double getSystemIdleTime() const {
        // Command to get system idle time (in seconds)
        string command = "top -bn1 | grep 'Cpu(s)' | sed 's/.*, *\\([0-9.]*\\)%* id.*/\\1/'";
        double cpuIdleTime;
        istringstream cpuIdleTimeStream(exec(command.c_str()));
        cpuIdleTimeStream >> cpuIdleTime;

        // Convert CPU idle time to system idle time in seconds
        return (cpuIdleTime / 100.0) * getUptime();     // Retrieves the system's idle time in seconds.
    }

    double getHDDUtilization() const {
        // Command to get HDD utilization
        string command = "df -h | awk '$NF==\"/\"{printf \"%s\", $5}'";
        double hddUtilization;
        istringstream hddUtilizationStream(exec(command.c_str()));
        hddUtilizationStream >> hddUtilization;
        return hddUtilization;       // Retrieves the HDD utilization as a percentage.
    }

private:
    string exec(const char* cmd) const {   // Executes a system command and returns its output
        array<char, 128> buffer;
        string result;
        unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

        if (!pipe) {
            throw runtime_error("popen() failed!");
        }

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        return result;
    }
        double getUptime() const {
        // Command to get system uptime in seconds
        string command = "cat /proc/uptime | cut -f1 -d\" \"";
        double uptime;
        istringstream uptimeStream(exec(command.c_str()));
        uptimeStream >> uptime;
        return uptime;   // Retrieves the system uptime in seconds.
    }
};
    // WebSocketClient class handles WebSocket communication and logging.
class WebSocketClient {
public:
   // Opens a log file for writing log messages.
    void openLogFile(const string& logFilePath) {
        logFile.open(logFilePath, ios::out);
        if (!logFile.is_open()) {
            cerr << "Failed to open the log file." << endl;
        }
    }

    void logMessage(const string& message) {
        if (logFile.is_open()) {
            logFile << message << endl;
        }
    }
     // Initializes the WebSocketClient with the server's IP and port.
    WebSocketClient(const string& serverIp, int port) 
        : endpoint(asio::ip::address::from_string(serverIp), port) {
    }
   // Connects to the WebSocket server.
    void connect() {
        ws.next_layer().connect(endpoint);
        ws.handshake("127.0.0.1", "/");
        cout << "Connected to server at port " << endpoint.port() << endl;
        cout << "Enter ctrl + c to exit.." << endl;
    }
   // Sends a message to the WebSocket server.
    void sendMessage(const string& message) {
        ws.write(asio::buffer(message.data(), message.size()));
    }
   // Closes the WebSocket connection. 
    void close() {
        ws.close(websocket::close_code::normal);
    }

private:
    asio::io_context io_context;
    websocket::stream<asio::ip::tcp::socket> ws{ io_context };
    asio::ip::tcp::endpoint endpoint;
    ofstream logFile;
};

// ... Other includes and namespaces
 // The main function where the system information is collected and sent to the server.
int main() {
    try {
        SystemInfoProvider systemInfoProvider;
        WebSocketClient webSocketClient("127.0.0.1", 8080);
        string logFilePath = "/path/to/acknowledgment.log"; 
        webSocketClient.openLogFile("acknowledgment.log");
        webSocketClient.connect();
        std::string interfaceName = "enp0s3";
        std::pair<long long, long long> networkUsage;
          // Collect system information, format it as JSON, and send it to the server at regular intervals.

        while (true) {
            networkUsage = systemInfoProvider.getNetworkUsage(interfaceName);

            json systemInfo;
            string name = systemInfoProvider.getSystemName();
            systemInfo["system_name"] = name;

            double cpuUtilization = systemInfoProvider.getCpuUtilization();
            if (!std::isnan(cpuUtilization))
                systemInfo["cpu_utilization"] = cpuUtilization;

            double ramUsage = systemInfoProvider.getRamUsage();
            if (!std::isnan(ramUsage))
                systemInfo["ram_usage"] = ramUsage;

            double systemIdleTime = systemInfoProvider.getSystemIdleTime();
            if (!std::isnan(systemIdleTime))
                systemInfo["system_idle_time"] = systemIdleTime;

            double hddUtilization = systemInfoProvider.getHDDUtilization();
            if (!std::isnan(hddUtilization))
                systemInfo["hdd_utilization"] = hddUtilization;



            // Ensure the network usage values are valid before adding to JSON
            if (networkUsage.first >= 0)
                systemInfo["rx_packets"] = networkUsage.first;

           // if (networkUsage.second >= 0)
             //   systemInfo["tx_packets"] = networkUsage.second;

            // Send the system information as JSON
            std::string sysInfoString = systemInfo.dump();
            webSocketClient.sendMessage(sysInfoString);

            cout<<"Server Response: Recieved!"<<endl;
            webSocketClient.logMessage("Server Response: Received!");  // Log a message indicating server response.

            this_thread::sleep_for(chrono::seconds(5));
        }
        cout << endl;
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}
