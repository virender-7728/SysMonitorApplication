#include <iostream>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "json-1.hpp"
#include <sstream>
#include <stdexcept>
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#define EXAMPLE_HOST "localhost"
#define EXAMPLE_USER "ishjain"
#define EXAMPLE_PASS "Vmboxlinux@2023"
#define EXAMPLE_DB "SystemMonitor"
using namespace std;
using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

class DatabaseManager
{
public:
    void insertData(  double cpuUtilization, double hddUtilization, double ramUsage, int rxPackets, double systemIdleTime,const string &systemName)
    {
        try
        {
            const string url = EXAMPLE_HOST;
            const string user = EXAMPLE_USER;
            const string pass = EXAMPLE_PASS;
            const string database = EXAMPLE_DB;
            sql::Driver *driver = get_driver_instance();
            std::auto_ptr<sql::Connection> con(driver->connect(url, user, pass));
            con->setSchema(database);
            sql::PreparedStatement *pstmt;
            pstmt = con->prepareStatement("INSERT INTO System_Information (CPU_UTILIZATION, HDD_UTILILZATION, RAM_USAGE, RX_PACKETS, SYSTEM_IDLE_TIME,SYSTEM_NAME) VALUES (?, ?, ?, ?, ?, ?)");

            pstmt->setString(6, systemName);
            pstmt->setDouble(1, cpuUtilization);
            pstmt->setDouble(2, hddUtilization);
            pstmt->setDouble(3, ramUsage);
            pstmt->setInt(4, rxPackets);
            pstmt->setDouble(5, systemIdleTime);
            
            
            //pstmt->setInt(7, txPackets);

            pstmt->execute();
            delete pstmt;
        }
        catch (sql::SQLException &e)
        {
            cerr << "MySQL Error: " << e.what() << endl;
        }
    }
};

class ClientHandler : public enable_shared_from_this<ClientHandler>
{
private:
    websocket::stream<tcp::socket> ws_;
    boost::beast::flat_buffer buffer_;
    tcp::endpoint remote_endpoint_;
    DatabaseManager db_manager;

public:
    explicit ClientHandler(tcp::socket socket)
        : ws_(move(socket))
    {
        remote_endpoint_ = ws_.next_layer().remote_endpoint();
        cout << "Client connected: " << remote_endpoint_ << endl;
        cout << endl;
    }

    ~ClientHandler()
    {
        cout << "Client disconnected: " << remote_endpoint_ << endl;
    }

    void start()
    {
        ws_.async_accept(
            boost::asio::bind_executor(
                ws_.get_executor(),
                bind(
                    &ClientHandler::onAccept,
                    shared_from_this(),
                    placeholders::_1)));
    }

private:
    void onAccept(boost::beast::error_code ec)
    {
        if (ec)
        {
            cerr << "WebSocket accept error: " << ec.message() << endl;
            return;
        }

        doRead();
    }

    void doRead()
    {
        ws_.async_read(
            buffer_,
            boost::asio::bind_executor(
                ws_.get_executor(),
                bind(
                    &ClientHandler::onRead,
                    shared_from_this(),
                    placeholders::_1,
                    placeholders::_2)));
    }

    void onRead(boost::beast::error_code ec, size_t)
    {
        if (ec == websocket::error::closed)
            return;

        if (ec)
        {
            cerr << endl
                 << "WebSocket read message: " << ec.message() << endl;
            return;
        }

        // Echo the received message back to the client
        string message = boost::beast::buffers_to_string(buffer_.data());
        cout << endl
             << "Received message from " << remote_endpoint_ << ": " << message << endl;

        try
        {
            json jsonData = json::parse(message);
            // Access the individual fields
            double cpuUtilization = jsonData["cpu_utilization"];
            double hddUtilization = jsonData["hdd_utilization"];
            double ramUsage = jsonData["ram_usage"];
            int rxPackets = jsonData["rx_packets"];
            double systemIdleTime = jsonData["system_idle_time"];
            string systemName = jsonData["system_name"];
            //int txPackets = jsonData["tx_packets"];
            cout << "System Name: " << systemName;
            cout << "CPU Utilization: " << cpuUtilization << "%" << endl;
            cout << "RAM Usage: " << ramUsage << " MB" << endl;
            cout << "System Idle Time: " << systemIdleTime << " seconds" << endl;
            cout << "HDD Utilization: " << hddUtilization << "%" << endl;
            cout << "Recieved Packets: " << rxPackets << endl;
            //  Insert data into the database
            db_manager.insertData( cpuUtilization, hddUtilization, ramUsage, rxPackets, systemIdleTime,systemName);
        }
        catch (const exception &e)
        {
            cerr << "Exception: " << e.what() << endl;
        }
        // Clear the buffer and start a new read operation
        buffer_.consume(buffer_.size());
        doRead();
    }
};

// The WebSocketServer and main function remain the same as in your original code
class WebSocketServer
{
private:
    boost::asio::io_context &io_context_;
    tcp::acceptor acceptor_;

public:
    WebSocketServer(boost::asio::io_context &io_context, short port)
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {}

    void startAccept()
    {
        acceptor_.async_accept(
            [this](boost::beast::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    make_shared<ClientHandler>(move(socket))->start();
                }
                startAccept();
            });
    }
};

int main()
{
    try
    {
        boost::asio::io_context io_context;

        const short port = 8080;
        WebSocketServer server(io_context, port);
        server.startAccept();

        cout << "Server listening on port " << port << "..." << endl;
        cout << endl;

        io_context.run();
    }
    catch (const exception &e)
    {
        cerr << "Exception: " << e.what() << endl;
    }
    return 0;
}
