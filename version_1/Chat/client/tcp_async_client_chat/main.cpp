#include <iostream>
#include <memory>
#include <thread>
#include <list>
#include <mutex>
#include <boost/asio.hpp>

using std::cout;
using std::cin;
using std::endl;
using std::cerr;

std::mutex all_threads_guard;

struct session
{
    boost::asio::ip::tcp::endpoint m_end;
    boost::asio::ip::tcp::socket m_sock;
    boost::asio::streambuf m_buf;
    std::string message;
    std::string response;

    session(boost::asio::io_service &ios, std::string raw_ip, unsigned short port_num)
        : m_sock(ios), m_end(boost::asio::ip::address::from_string(raw_ip), port_num)
    {
    }
};
class client
{
private:
    boost::asio::io_service m_ios;
    std::unique_ptr<boost::asio::io_service::work> worker;
    std::list<std::unique_ptr<std::thread>> threads;
    std::unique_ptr<session> s;
    bool ready_exit;
private:
    void checkExitRequest()
    {
        if(s->message == "EXIT")
        {
            ready_exit = true;
        }
    }
    void onWriteMsg(const boost::system::error_code &er)
    {
        if(er != 0)
        {
            cerr << "onWriteMsg error! with value: " << er.value() << " | with code: " << er.message() << endl;
            return;
        }
        writeToChat();
    }
    void onReadMsg(const boost::system::error_code &er, std::size_t bytes)
    {
        if(er != 0)
        {
            cerr << "onReadMsg error! with value: " << er.value() << " | with code: " << er.message() << endl;
            return;
        }
        std::istream in(&s->m_buf);
        std::getline(in, s->response);
        if(s->response != "")
            cout << s->response << endl;
        readFromChat();
    }
    void onFinish()
    {
        boost::system::error_code ignored_er;
        s->m_sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_er);
        if(ignored_er != 0)
            cerr << "onFinish error! with value: " << ignored_er.value() << " | with message: " << ignored_er.message() << endl;
    }
    void onConnect(const boost::system::error_code &er)
    {
        if(er != 0)
            cerr << "onConnect error! With value: " << er.value() << " | with message: " << er.message() << endl;
    }
    void readFromChat()
    {
        boost::asio::async_read_until(s->m_sock, s->m_buf, '\n', [this](const boost::system::error_code &er, std::size_t bytes)
        {
            onReadMsg(er, bytes);
        });
    }
    void writeToChat()
    {
        std::getline(cin, s->message);
        s->message += "\n";
        boost::asio::async_write(s->m_sock, boost::asio::buffer(s->message), [this](const boost::system::error_code &er, std::size_t bytes)
        {
            onWriteMsg(er);
        });
    }
public:
    client(): ready_exit(false)
    {
        worker.reset(new boost::asio::io_service::work(m_ios));
    }
    void start(std::string ip, unsigned short port_num, unsigned int amount_threads)
    {
        s.reset(new session(m_ios, ip, port_num));
        for(unsigned int i(0); i < amount_threads; i++)
        {
            std::unique_ptr<std::thread> list_thread(new std::thread([this]()
            {
                m_ios.run();
            }));
            threads.push_back(std::move(list_thread));
        }
        s->m_sock.open(s->m_end.protocol());
        s->m_sock.async_connect(s->m_end, [this](const boost::system::error_code &er)
        {
            if(er != 0)
            {
                cerr << "async_accept error! with value: " << er.value() << " | with message: " << er.message() << endl;
                return;
            }
        });
        readFromChat();
        writeToChat();
    }
    void stop()
    {
        onFinish();
        s.reset(nullptr);
        m_ios.stop();
        for(std::list<std::unique_ptr<std::thread> >::iterator it = threads.begin(); it != threads.end(); it++)
            it->get()->join();
    }
};

int main()
{
    unsigned short port = 3333;
    std::string addr = "127.0.0.1";
    try
    {
        client obj;
        obj.start(addr, port, 5);
        std::this_thread::sleep_for(std::chrono::seconds(1200));
        obj.stop();
    }
    catch(boost::system::system_error &er)
    {
        cout << "" << er.code() << "" << er.what() << endl;
        return er.code().value();
    }
    return 0;
}
