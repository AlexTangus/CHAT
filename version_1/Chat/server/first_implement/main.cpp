#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <boost/asio.hpp>

using std::cout;
using std::cin;
using std::endl;
using std::cerr;

std::map<std::string, std::shared_ptr<boost::asio::ip::tcp::socket>> name_sock;
std::mutex all_thread_guard;

class Service
{
private:
    std::shared_ptr<boost::asio::ip::tcp::socket> pointer_to_sock;
    boost::asio::streambuf m_request;
    std::string m_response;
    std::string name;
    bool first_connection;
private:
    void onFinish()
    {
        eraseFromTable();
        boost::system::error_code ignored_er;
        pointer_to_sock->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_er);
        pointer_to_sock->close();
        delete this;
    }
    void addToTable()
    {
        all_thread_guard.lock();
        name_sock[this->name] = pointer_to_sock;
        all_thread_guard.unlock();
    }
    void eraseFromTable()
    {
        all_thread_guard.lock();
        auto it = name_sock.find(this->name);
        if(it != name_sock.end())
            name_sock.erase(it);
        all_thread_guard.unlock();
    }
    void onResponseSent(const boost::system::error_code &er, std::size_t bytes)
    {
        if(er != 0)
            cerr << "onResponseSent error! Value: " << er.value() << " | Message: " << er.message() << endl;
    }
    void onRequestRecieved(const boost::system::error_code &er, std::size_t bytes)
    {
        if(er != 0)
        {
            cerr << "onRequestRecieved error! Value: " << er.value() << " | Message: " << er.message() << endl;
            onFinish();
            return;
        }
        std::istream in(&m_request);
        std::getline(in, m_response);

        if(m_response != "")
        {
            //m_response.pop_back(); // pop symbol '\r'
            if(m_response[m_response.length() - 1] == '\r')
                m_response.pop_back();
            if(m_response == "EXIT")
            {
                onFinish();
                return;
            }

            if(first_connection)
            {
                name = m_response;
                addToTable();
                first_connection = false;
                readMsg();
            }
            else if(!first_connection)
            {
                m_response.push_back('\n'); // push symbol '\n' for write to sockets
                writeMsg();
            }
        }
    }
    void writeMsg()
    {
        std::string temp = name + ": " + m_response;
        for(std::map<std::string, std::shared_ptr<boost::asio::ip::tcp::socket> >::iterator it = name_sock.begin(); it != name_sock.end(); it++)
        {
            if(this->name == it->first)
                continue;
            boost::asio::async_write(*(it->second).get(), boost::asio::buffer(temp), [this](const boost::system::error_code &er, std::size_t bytes)
            {
                onResponseSent(er, bytes);
            });
        }
        readMsg();
    }
    void readMsg()
    {
        boost::asio::async_read_until(*pointer_to_sock.get(), m_request, '\n', [this](const boost::system::error_code &er, std::size_t bytes)
        {
            onRequestRecieved(er, bytes);
        });
    }
public:
    Service(std::shared_ptr<boost::asio::ip::tcp::socket> &sock): pointer_to_sock(sock), first_connection(true) {}
    void startHandling()
    {
        readMsg();
    }
};
class Acceptor
{
private:
    boost::asio::io_service &m_ios;
    boost::asio::ip::tcp::acceptor m_acc;
    std::atomic<bool> m_isStop;
private:
    void onAccept(const boost::system::error_code &er, std::shared_ptr<boost::asio::ip::tcp::socket> p_sock)
    {
        if(er == 0)
            (new Service(p_sock))->startHandling();
        else
            cout << "Accept error. Value: " << er.value() << " | Message: " << er.message() << endl;

        if(!m_isStop.load())
            initAccept();
        else
            m_acc.close();
    }
    void initAccept()
    {
        std::shared_ptr<boost::asio::ip::tcp::socket> p_sock(new boost::asio::ip::tcp::socket(m_ios));
        m_acc.async_accept(*p_sock.get(), [this, p_sock](const boost::system::error_code &er)
        {
            onAccept(er, p_sock);
        });
    }
public:
    Acceptor(boost::asio::io_service &ios, unsigned short port)
        : m_ios(ios), m_acc(m_ios, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), port)) {}
    void start()
    {
        m_acc.listen();
        initAccept();
    }
    void stop()
    {
        m_isStop.store(true);
    }
};

class Server
{
private:
    boost::asio::io_service ios;
    std::unique_ptr<boost::asio::io_service::work> work;
    std::unique_ptr<Acceptor> acc;
    std::vector<std::unique_ptr<std::thread>> m_thread;
public:
    Server()
    {
        work.reset(new boost::asio::io_service::work(ios));
    }
    void start(unsigned short port, unsigned int amount_threads)
    {
        acc.reset(new Acceptor(ios, port));
        acc->start();

        for(unsigned int i(0); i < amount_threads; i++)
        {
            std::unique_ptr<std::thread> th(new std::thread([this]()
            {
                ios.run();
            }));
            m_thread.push_back(std::move(th));
        }
    }
    void stop()
    {
        acc->stop();
        ios.stop();
        for(auto &th : m_thread)
            th->join();
    }
};
int main()
{
    unsigned short port = 3333;

    try
    {
        Server obj;
        obj.start(port, 3);
        std::this_thread::sleep_for(std::chrono::seconds(1200));
        obj.stop();
    }
    catch(boost::system::system_error &er)
    {
        cerr << "Error occured. With code: " << er.code() << " | With message: " << er.what() << endl;
        return er.code().value();
    }
    return 0;
}
