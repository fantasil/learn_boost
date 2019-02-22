#pragma once
#include"def.h"
#include<boost/asio.hpp>
#include<boost/date_time.hpp>
#include<iostream>
#include<atomic>
#include<chrono>
#include<ctime>
namespace tcp_server_demo
{
	/********************tips: not thread safe*****************************/
	//获取当前时间的字符串
	std::string get_time_now()
	{
		using namespace std::chrono;
		auto t1 = system_clock::now();
		auto t2 = system_clock::to_time_t(t1);
		char* buf = new char[100];
		ctime_s(buf, 100, &t2);
		std::string time{ buf };
		return time;
	}

	//打印asio error
	void asio_err_msg(boost::system::system_error& se)
	{
		std::cout << err_header << se.code() << ":" << se.what() << " " << get_time_now();
	}

	//打印asio prompt
	void asio_prompt_msg(std::string prompt_context)
	{
		std::cout << prompt_header << prompt_context << " " << get_time_now();
	}
	//打印asio log
	void asio_log_msg(std::string log_context)
	{
		std::cout << log_header << log_context << " " << get_time_now();
	}

	/**************************************************************************/

	namespace sync {

		/*
			一个同步服务器:
				1:初始化相关资源,acceptor绑定监听端口,accept开始等待客户端连接
				2:内部功能只是简单的把用户发送的数据发送回去
			
			learn:
				1:当c/s同时使用单线程阻塞模式的时候,容易都阻塞在read之上:
					planA:给服务器设置超时时间,时间一到就closesocket
					planB:使用子线程执行客户端的读取操作.

					planA:
						:使用b_timeout作为检测标志,在计时器绑定程序之后检查该值,若仍是ture,就关闭连接
						{
							0:自己设置计时器
							1:b_timeout必须防止竞争
							2:read函数依然会阻塞,直到接收到数据后返回,然后跳出循环
						}
						planA isn't good;
					planB:
						:客户端使用子线程读取数据,主线程写入数据.客户端主动断开连接.
						{
							0:客户端使用子线程读取数据
							1:客户端主线程写入数据
							2:服务端逻辑依然保持简单.
						}
				 依然存在的问题:
					1:cout<<array.data的时候,遇见空格会换行.  ...待解决

				2019-2-20

				1:添加了自定义读取返回的标志read_compl_handler,即read函数读取到特定的值(这里设置的是0x10)或者满缓存的时候会返回
				2:上述cout<<array.data产生换行的问题是由于,客户端使用cin>>string,会自动跳过空白符,所以hello world会分两次传输,服务器端也就会分开换行打印
				3:write不会发送这个缓冲区的内容,而仅仅将收到的信息回送.
				4:打印消息时添加了一个时间戳
				::_test_plan_a未做改动.

				2019-2-22
		*/


		using namespace std;
		using namespace boost::asio;
		using namespace boost::asio::ip;

		
		array<char, def_buf> read_buf{};	//读取缓冲区
		
		boost::system::error_code error;
		size_t offset;
	

		void _test_plan_a()
		{
			io_service service;
			cout << "start service...\n";
			tcp::acceptor acceptor{ service ,tcp::endpoint(tcp::v4(),def_port) };
			auto ptimeout = std::make_shared<atomic<bool>>(new atomic<bool>{ true });
			while (true)
			{
				tcp::socket sock{ service };
				cout << "wait for client...\n";
				acceptor.accept(sock);

				cout << "client coming...\n";
				while (true)
				{

					auto timeout_handler = [&ptimeout, &sock](const boost::system::error_code& error) {
						if (ptimeout->load())
						{
							cout << "timeout:close socket...\n";
							sock.close();
							ptimeout->store(false);
						}
						 
					};
					deadline_timer dt{ service ,boost::posix_time::seconds(5) };
					std::thread t{ 
						[&]() {dt.async_wait(timeout_handler); service.run(); }
					};
					t.detach();
					sock.read_some(buffer(read_buf.data(), read_buf.size()), error);
					if (!ptimeout->load())
					{
						break;	//当这里ptimeout==false时,说明已经超时了.
					}
					ptimeout->store(false);
					if (error)
					{
						if (error == boost::asio::error::eof)
						{
							sock.shutdown(tcp::socket::shutdown_both);
							break;
						}
						else
							//throw boost::system::system_error{ error };
							break;	//just break and clear resource
					}
					cout << read_buf.data() << "\n";
					sock.write_some(buffer(read_buf.data(), read_buf.size()), error);
					memset(&read_buf, 0, read_buf.size());
					ptimeout->store(true);
				}

				try {
					sock.close();
				}
				catch (std::exception& e)
				{
					cout << e.what() << endl;
				}
				cout << "client finished...\n";
			}
		}

		void _test_plan_b()
		{
			io_service service;
			asio_log_msg(log_server_init_msg);
			tcp::acceptor acceptor{ service ,tcp::endpoint(tcp::v4(),def_port) };
			int bytes_read{},bytes_write;

			//设置读取返回条件
			auto read_compl_handler = [](const boost::system::error_code& ec,size_t bytes) 
			{
				if (ec)
					throw boost::system::system_error{ ec };	//有异常抛出
				for (offset; offset < bytes; ++offset)
				{
					if (read_buf[offset] == end_flag)
					{
						offset = 0;							//如果读取到了期望的结束符,就重置偏移量
						return static_cast<size_t>(0);		//返回0标识结束此次读取
					}
				}
				return static_cast<size_t>(1);
			};		

			while (true)
			{
				tcp::socket sock{ service };
				asio_log_msg(log_server_wait_msg);
				acceptor.accept(sock);
				asio_log_msg(log_server_client_coming);
				while (true)
				{
					try {
						bytes_read = boost::asio::read(sock, buffer(read_buf.data(), read_buf.max_size()), read_compl_handler);
					}
					catch (boost::system::system_error& se)
					{
						if (se.code() == boost::asio::error::eof)
							asio_prompt_msg(prompt_server_client_shutdown_send); 
						else
							asio_err_msg(se);
						break;	//跳出循环 清理资源
					}
					std::string read_msg = log_server_read_msg_header + std::to_string(bytes_read) + log_server_read_msg_foot;
					asio_log_msg(read_msg);
					try {
						bytes_write = boost::asio::write(sock, buffer(read_buf.data(), bytes_read));
					}
					catch (boost::system::system_error& se)
					{
						asio_err_msg(se);
						break;	//跳出循环 清理资源
					}
					std::string write_msg = log_server_write_msg_header + std::to_string(bytes_write) + log_server_write_msg_foot;
					asio_log_msg(write_msg);
					memset(&read_buf, 0, read_buf.size());
				}

				try {
					sock.close();
				}
				catch (std::exception& e)
				{
					cout << e.what() << endl;
				}
				asio_log_msg(log_server_client_leaving);
			}
		}


	}

	namespace async {

		/*
			一个异步服务器:
				1:初始化相关资源
				2:等待客户连接
				3:读取/写回数据
				4:收到eof标志,断开连接,重复步骤2

			learn:
				1:因为异步标志是及时返回的,所以需要考虑一些变量的声明周期问题,例如在main_loop中,socket需要使用堆内存,而不是栈空间
				2:当使用std::bind绑定参数的时候,对于引用参数,需要使用std::ref(refrence)函数,否则编译不通过
				3:在read_compl_handler中,也会读取到eof标志,但这里没有直接处理,只是让函数返回了,在read_handler中处理.
				4:不同于同步可以使用while循环,这里需要自己手动打造,在收到eof断开连接后,使用is.post(main_loop),执行下一轮函数

			2019-2-22
		*/


		using namespace std;
		using namespace boost::asio;
		using namespace boost::asio::ip;

		io_service is;
		tcp::endpoint ep{ tcp::v4(),def_port };
		tcp::acceptor acceptor{ is,ep };
		//tcp::socket sock{ is };
		array<char, 1024> buf{};
		size_t offset;

		void main_loop();
		void accept_handler(const boost::system::error_code& ec,tcp::socket& sock);
		void read_handler(const boost::system::error_code& ec, size_t bytes,tcp::socket& sock);
		void write_handler(const boost::system::error_code& ec, size_t bytes);

		size_t read_compl_handler(const boost::system::error_code& ec, size_t bytes)
		{
			//不在这里处理eof标志,让读取返回
			if (ec == boost::asio::error::eof)
				return 0;
			else if (ec)
				throw boost::system::system_error{ ec };
			for (offset; offset < bytes; ++offset)
			{
				if (buf[offset] == end_flag)	//直到读取end_flag标志返回
					return 0;
			}
			return 1;

		}

		void _test()
		{
			asio_log_msg(log_server_init_msg);
			main_loop();
			is.run();
			

		}

		void main_loop()
		{
			//主循环
			asio_log_msg(log_server_wait_msg);
			//tcp::socket sock{ is };
			tcp::socket* sock = new tcp::socket{ is };	//因为需要在离开main_loop函数时保持sock的生命周期,所以使用堆内存
			acceptor.async_accept(*sock, std::bind(accept_handler, std::placeholders::_1, std::ref(*sock)));
			
		}

		//
		void accept_handler(const boost::system::error_code& ec,tcp::socket& sock)
		{
			asio_log_msg(log_server_client_coming);
			if (ec)
				throw boost::system::system_error{ ec };

			/*auto compl_handler = [](const boost::system::error_code& ec,size_t bytes) {
				if (ec)
					throw boost::system::system_error{ ec };
				for (offset; offset < bytes; ++offset)
				{
					if (buf[offset] == end_flag)
						return static_cast<size_t>(0);
				}
				return static_cast<size_t>(1);
			};*/

			try {
				//从socket中读取数据
				boost::asio::async_read(sock, buffer(buf.data(), buf.size()), read_compl_handler, std::bind(read_handler,
					std::placeholders::_1,
					std::placeholders::_2,
					std::ref(sock)));
			}
			catch (boost::system::system_error& se)
			{
				asio_err_msg(se);
			}
			
		}

		
		void read_handler(const boost::system::error_code& ec, size_t bytes,tcp::socket& sock)
		{
			//在这里处理eof标志,代表了客户端"期望"断开连接,在这里断开连接等待新客户连接
			if (ec == boost::asio::error::eof)
			{
				asio_prompt_msg(prompt_server_client_shutdown_send);
				sock.close();
				delete &sock;
				is.post(main_loop);
				return;
			}
			else if (ec)
				throw boost::system::system_error{ ec };

			//处理数据
			std::string read_msg = log_server_read_msg_header + to_string(bytes) + log_server_read_msg_foot;
			asio_log_msg(read_msg);
			//写回数据
			boost::asio::async_write(sock, buffer(buf.data(), bytes), write_handler);
			//继续读取
			try {
				boost::asio::async_read(sock, buffer(buf.data(), buf.size()), read_compl_handler, std::bind(read_handler,
					std::placeholders::_1,
					std::placeholders::_2,
					std::ref(sock)));
			}
			catch (boost::system::system_error& se)
			{
				asio_err_msg(se);
			}

		}

		//写入处理函数,仅记录写入的字节数目,有异常则抛出
		void write_handler(const boost::system::error_code& ec, size_t bytes)
		{
			if (ec)
				throw boost::system::system_error{ ec };
			std::string write_msg = log_server_write_msg_header + to_string(bytes) + log_server_write_msg_foot;
			asio_log_msg(write_msg);
		}
	}


}