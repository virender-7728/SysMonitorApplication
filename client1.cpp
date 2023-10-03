#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <sstream>
#include <fstream>

#include </home/virender/Downloads/json.hpp>

using namespace std;
using json = nlohmann::json;
//using json = nlohmann::json;

namespace asio = boost::asio;
namespace ip = asio::ip;
namespace websocket = boost::beast::websocket;

class SystemInfoProvider {
public:
    string getSystemName() const {
        return exec("hostname");
    }

    double getCpuUtilization() const {
        istringstream cpuUtilizationStream(exec("top -bn1 | grep 'Cpu(s)' | sed 's/.*, *\\([0-9.]*\\)%* id.*/\\1/'"));
        double cpuUtilization;
        cpuUtilizationStream >> cpuUtilization;
        return cpuUtilization;
    }

    double getRamUsage() const {
        istringstream ramUsageStream(exec("free -m | awk 'NR==2{print $3}'"));
        double ramUsage;
        ramUsageStream >> ramUsage;
        return ramUsage;
    }
    double getSystemIdleTime() const {
        // Command to get system idle time (in seconds)
        string command = "top -bn1 | grep 'Cpu(s)' | sed 's/.*, *\\([0-9.]*\\)%* id.*/\\1/'";
        double cpuIdleTime;
        istringstream cpuIdleTimeStream(exec(command.c_str()));
        cpuIdleTimeStream >> cpuIdleTime;

        // Convert CPU idle time to system idle time in seconds
        return (cpuIdleTime / 100.0) * getUptime();
    }

    double getHDDUtilization() const {
        // Command to get HDD utilization
        string command = "df -h | awk '$NF==\"/\"{printf \"%s\", $5}'";
        double hddUtilization;
        istringstream hddUtilizationStream(exec(command.c_str()));
        hddUtilizationStream >> hddUtilization;
        return hddUtilization;
    }

private:
    string exec(const char* cmd) const {
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
        return uptime;
    }
};

class WebSocketClient {
public:
    WebSocketClient(const string& serverIp, int port) 
        : endpoint(asio::ip::address::from_string(serverIp), port) {
    }

    void connect() {
        ws.next_layer().connect(endpoint);
        ws.handshake("10.11.245.174", "/");
        cout << "Connected to server at port " << endpoint.port() << endl;
        cout << "Enter ctrl + c to exit.." << endl;
    }

    void sendMessage(const string& message) {
        ws.write(asio::buffer(message.data(), message.size()));
    }

    void close() {
        ws.close(websocket::close_code::normal);
    }

private:
    asio::io_context io_context;
    websocket::stream<asio::ip::tcp::socket> ws{ io_context };
    asio::ip::tcp::endpoint endpoint;
};

int main() {
    try {
        SystemInfoProvider systemInfoProvider;
        WebSocketClient webSocketClient("10.11.245.174", 8080);
        webSocketClient.connect();

        while (true) {
            ostringstream sysInfoStream;
    sysInfoStream << endl <<"SystemName: " << systemInfoProvider.getSystemName()
                  << "CPUutilization: " << systemInfoProvider.getCpuUtilization() << " %"<< endl
                  << "RamUsage: " << systemInfoProvider.getRamUsage() << " MB" << endl
                  << "SystemIdleTime: " << systemInfoProvider.getSystemIdleTime() << " sec" <<endl
                  << "HDDutilization: " <<systemInfoProvider.getHDDUtilization() << " %"<< endl;

    string sysInfoString = sysInfoStream.str();

    // Send the system information string
    webSocketClient.sendMessage(sysInfoString);

            this_thread::sleep_for(chrono::seconds(5));
        }
        cout<<endl;
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}

