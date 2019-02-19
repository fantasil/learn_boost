#pragma once

#include<boost/asio.hpp>
#include<string>
#include<iostream>
#include<fstream>

/*
	这是一个基本的使用tcp协议连接服务器读写数据的过程;
	基本流程:
		0:初始化相关资源
		1:解析域名
		2:连接远程服务器
		3:发送数据
		4:接收数据
		5:清理工作

	tips:
		 :一般函数都会支持一个boost::system::error的参数,可以检查该参数查询函数是否执行成功,也可以通过抛出异常方式获取.  //所有的boost异常都继承自std::exception
		 :在同步过程读取数据的时候,若之前不主动shutdown(send),那么最后一个eof会等待相当一段时间,而不是马上返回,所以这里为了测试,会主动shutdown
		 :同步过程还是比较顺利的,异步过程就踩了一些坑:
			0:模仿代码时,调用异步过程传入参数时,r.async_resolve(q,boost::bind(resolve_handler,placeholders::error,placeholders::results)),而编译却一直报错.
			  所以后来取消了bind方式,直接调用   r.async_resolve(q,resolve_handler);
			  因为没有编译boost::bind库,使用的是std::bind,是因为这个引发的编译错误.
			1:执行时会由于"由于线程退出或应用程序请求，已中止 I/O 操作"而终止,发现是由于resolver对象过早销毁而引发.
			2:read_handler仍然需要多次调用,直到数据读取完毕
*/

namespace tcp_demo
{
	using namespace std;
	using namespace boost::asio;
	using namespace boost::asio::ip;
	
	namespace sync {
		//0:初始化相关资源
		io_service g_is;
		tcp::socket tcp_socket{ g_is };
		const string host{ "www.baidu.com" };	//测试用域名
		const string service{ "http" };
		const string send_data{ "GET / HTTP/1.1\r\nHost:www.baidu.com\r\n\r\n" }; //mini HTTP request
		fstream fs{ "storage.txt",ios::app };

		array<char, 1024> async_read_buf{};
		/*同步的tcp*/
		//1:解析域名
		tcp::resolver::iterator getaddrinfo(const string& host, const string& service)
		{
			tcp::resolver r{ g_is };
			tcp::resolver::query q{ host,service };
			tcp::resolver::iterator iter = r.resolve(q);
			return iter;
		}

		//2:连接远程服务器
		void connect_server(tcp::resolver::iterator iter)
		{
			tcp::resolver::iterator end;
			if (iter == end)
				return;
			connect(tcp_socket, iter);
		}

		//3:发送数据
		void write_data(const string& data)
		{
			tcp_socket.write_some(boost::asio::buffer(data.c_str(), data.size()));
			//tcp_socket.shutdown(tcp::socket::shutdown_send);
		}

		//4:接收数据
		void read_data()
		{
			boost::system::error_code error;
			size_t read_bytes{};
			const int buf_sz{ 1024 };
			array<char, buf_sz> read_buf{};
			while (true)
			{
				read_bytes = tcp_socket.read_some(buffer(read_buf.data(), read_buf.size()), error);
				//错误处理
				if (error == boost::asio::error::eof)
				{
					cout << "read finished\n" << endl;
					break;
				}
				else if (error)
				{
					throw boost::system::system_error{ error };
				}
				//处理数据
				cout << "received " << read_bytes << " bytes\n";
				fs << read_buf.data();

			}
		}


		//5:清理资源
		void clear_resource()
		{
			tcp_socket.close();
			fs.close();
		}

		void _test()
		{
			auto iter = getaddrinfo(host, service);
			try {
				connect_server(iter);
			}
			catch (std::exception& e)
			{
				cout << e.what() << endl;
				return;
			}
			write_data(send_data);
			read_data();
			clear_resource();
		}
	}

	/*异步的tcp*/
	namespace async {

		//0:初始化相关资源
		io_service g_is;
		tcp::socket tcp_socket{ g_is };
		const string host{ "www.baidu.com" };
		const string service{ "http" };
		const string send_data{ "GET / HTTP/1.1\r\nHost:www.baidu.com\r\n\r\n" };
		tcp::resolver resolver{ g_is };		//我们需要保持resolver的生存期,否则会因为需要访问resolver而其已被销毁时发生异常错误
		array<char, 1024> read_buf{};
		fstream fs{ "storage.txt",ios::app };
		//异步处理函数,先声明,仅是个人习惯
		void resolve_handler(const boost::system::error_code& error, tcp::resolver::results_type results);
		void connect_handler(const boost::system::error_code& error, const tcp::endpoint& ep);
		void write_handler(const boost::system::error_code& error, size_t byte_transferred);
		void read_handler(const boost::system::error_code& error, size_t byte_transferred);
		void clear_resource();

		
		void async_getaddr(const string& host, const string& service)
		{
			cout << "async_getaddr {host:" << host << "  ,service:" << service << "}\n";
			resolver.async_resolve(host, service, resolve_handler);
		}

		//1:解析域名
		void resolve_handler(const boost::system::error_code& error, tcp::resolver::results_type results)
		{
			cout << "resolver_handler\n";
			if (!error)
			{
				boost::asio::async_connect(tcp_socket, results, connect_handler);	//解析成功就进行异步连接

				/*
					若resolver在此之前已被销毁,则会生成"由于线程退出或应用程序请求，已中止 I/O 操作"异常.
				*/
			}
			else
				throw boost::system::system_error{ error };
		}

		//
		void connect_handler(const boost::system::error_code& error, const tcp::endpoint& ep)
		{
			cout << "connect_handler\n";

			if (!error)
			{
				tcp_socket.async_write_some(boost::asio::buffer(send_data.c_str(), send_data.size()), write_handler); //连接成功写入请求
			}
			else
				throw boost::system::system_error{ error };
		}

		void write_handler(const boost::system::error_code& error, size_t byte_transferred)
		{
			cout << "write_handler\n";
			if (!error)
			{
				cout << "bytes_write:" << byte_transferred << "\n";
				tcp_socket.shutdown(tcp::socket::shutdown_send);
				tcp_socket.async_read_some(boost::asio::buffer(read_buf, read_buf.size()), read_handler); //写入数据之后关闭写通道,并等待读取数据
			}
			else
				throw boost::system::system_error{ error };

		}

		void read_handler(const boost::system::error_code& error, size_t byte_transferred)
		{
			cout << "read_handler\n";
			if (error == boost::asio::error::eof)
			{
				cout << "read finished\n";			//数据读取完毕,写入文件,清理资源
				fs << "\r\n";
				clear_resource();
				return;
			}
			else if (error)
			{
				throw boost::system::system_error{ error };
			}

			//保存数据
			fs << read_buf.data();	

			cout << "byte_read:" << byte_transferred << " bytes\n";

			tcp_socket.async_read_some(boost::asio::buffer(read_buf, read_buf.size()), read_handler);


		}

		void clear_resource()
		{
			tcp_socket.close();
		}

		void _test()
		{
			try {
				async_getaddr(host, service);
				g_is.run();
			}
			catch (std::exception& e)
			{
				cout << e.what() << endl;
			}
		}
	}

};

