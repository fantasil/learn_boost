#pragma once
#include"def.h"
#include<boost/asio.hpp>
#include<boost/date_time.hpp>
#include<iostream>
#include<atomic>

namespace tcp_server_demo
{

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
		*/


		using namespace std;
		using namespace boost::asio;
		using namespace boost::asio::ip;

		
		array<char, def_buf> read_buf{};
		boost::system::error_code error;
		
		
		


		void _test_plan_a()
		{
			io_service service;
			cout << "start service...\n";
			tcp::acceptor acceptor{ service ,tcp::endpoint(tcp::v4(),def_port) };
			//atomic<bool>* b_timeout = new atomic<bool>{ true };
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
			cout << "start service...\n";
			tcp::acceptor acceptor{ service ,tcp::endpoint(tcp::v4(),def_port) };
			
			while (true)
			{
				tcp::socket sock{ service };
				cout << "wait for client...\n";
				acceptor.accept(sock);

				cout << "client coming...\n";
				while (true)
				{
					sock.read_some(buffer(read_buf.data(), read_buf.size()), error);
					if (error)
					{
						if (error == boost::asio::error::eof)
						{
							sock.shutdown(tcp::socket::shutdown_both);
							break;
						}
						else
							break;	//just break and clear resource
					}
					cout << read_buf.data() << "\n";
					sock.write_some(buffer(read_buf.data(), read_buf.size()), error);
					memset(&read_buf, 0, read_buf.size());
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


	}


}