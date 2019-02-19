#pragma once

#include<boost/asio.hpp>
#include<string>
#include<iostream>
#include<fstream>

/*
	����һ��������ʹ��tcpЭ�����ӷ�������д���ݵĹ���;
	��������:
		0:��ʼ�������Դ
		1:��������
		2:����Զ�̷�����
		3:��������
		4:��������
		5:������

	tips:
		 :һ�㺯������֧��һ��boost::system::error�Ĳ���,���Լ��ò�����ѯ�����Ƿ�ִ�гɹ�,Ҳ����ͨ���׳��쳣��ʽ��ȡ.  //���е�boost�쳣���̳���std::exception
		 :��ͬ�����̶�ȡ���ݵ�ʱ��,��֮ǰ������shutdown(send),��ô���һ��eof��ȴ��൱һ��ʱ��,���������Ϸ���,��������Ϊ�˲���,������shutdown
		 :ͬ�����̻��ǱȽ�˳����,�첽���̾Ͳ���һЩ��:
			0:ģ�´���ʱ,�����첽���̴������ʱ,r.async_resolve(q,boost::bind(resolve_handler,placeholders::error,placeholders::results)),������ȴһֱ����.
			  ���Ժ���ȡ����bind��ʽ,ֱ�ӵ���   r.async_resolve(q,resolve_handler);
			  ��Ϊû�б���boost::bind��,ʹ�õ���std::bind,����Ϊ��������ı������.
			1:ִ��ʱ������"�����߳��˳���Ӧ�ó�����������ֹ I/O ����"����ֹ,����������resolver����������ٶ�����.
			2:read_handler��Ȼ��Ҫ��ε���,ֱ�����ݶ�ȡ���
*/

namespace tcp_demo
{
	using namespace std;
	using namespace boost::asio;
	using namespace boost::asio::ip;
	
	namespace sync {
		//0:��ʼ�������Դ
		io_service g_is;
		tcp::socket tcp_socket{ g_is };
		const string host{ "www.baidu.com" };	//����������
		const string service{ "http" };
		const string send_data{ "GET / HTTP/1.1\r\nHost:www.baidu.com\r\n\r\n" }; //mini HTTP request
		fstream fs{ "storage.txt",ios::app };

		array<char, 1024> async_read_buf{};
		/*ͬ����tcp*/
		//1:��������
		tcp::resolver::iterator getaddrinfo(const string& host, const string& service)
		{
			tcp::resolver r{ g_is };
			tcp::resolver::query q{ host,service };
			tcp::resolver::iterator iter = r.resolve(q);
			return iter;
		}

		//2:����Զ�̷�����
		void connect_server(tcp::resolver::iterator iter)
		{
			tcp::resolver::iterator end;
			if (iter == end)
				return;
			connect(tcp_socket, iter);
		}

		//3:��������
		void write_data(const string& data)
		{
			tcp_socket.write_some(boost::asio::buffer(data.c_str(), data.size()));
			//tcp_socket.shutdown(tcp::socket::shutdown_send);
		}

		//4:��������
		void read_data()
		{
			boost::system::error_code error;
			size_t read_bytes{};
			const int buf_sz{ 1024 };
			array<char, buf_sz> read_buf{};
			while (true)
			{
				read_bytes = tcp_socket.read_some(buffer(read_buf.data(), read_buf.size()), error);
				//������
				if (error == boost::asio::error::eof)
				{
					cout << "read finished\n" << endl;
					break;
				}
				else if (error)
				{
					throw boost::system::system_error{ error };
				}
				//��������
				cout << "received " << read_bytes << " bytes\n";
				fs << read_buf.data();

			}
		}


		//5:������Դ
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

	/*�첽��tcp*/
	namespace async {

		//0:��ʼ�������Դ
		io_service g_is;
		tcp::socket tcp_socket{ g_is };
		const string host{ "www.baidu.com" };
		const string service{ "http" };
		const string send_data{ "GET / HTTP/1.1\r\nHost:www.baidu.com\r\n\r\n" };
		tcp::resolver resolver{ g_is };		//������Ҫ����resolver��������,�������Ϊ��Ҫ����resolver�����ѱ�����ʱ�����쳣����
		array<char, 1024> read_buf{};
		fstream fs{ "storage.txt",ios::app };
		//�첽������,������,���Ǹ���ϰ��
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

		//1:��������
		void resolve_handler(const boost::system::error_code& error, tcp::resolver::results_type results)
		{
			cout << "resolver_handler\n";
			if (!error)
			{
				boost::asio::async_connect(tcp_socket, results, connect_handler);	//�����ɹ��ͽ����첽����

				/*
					��resolver�ڴ�֮ǰ�ѱ�����,�������"�����߳��˳���Ӧ�ó�����������ֹ I/O ����"�쳣.
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
				tcp_socket.async_write_some(boost::asio::buffer(send_data.c_str(), send_data.size()), write_handler); //���ӳɹ�д������
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
				tcp_socket.async_read_some(boost::asio::buffer(read_buf, read_buf.size()), read_handler); //д������֮��ر�дͨ��,���ȴ���ȡ����
			}
			else
				throw boost::system::system_error{ error };

		}

		void read_handler(const boost::system::error_code& error, size_t byte_transferred)
		{
			cout << "read_handler\n";
			if (error == boost::asio::error::eof)
			{
				cout << "read finished\n";			//���ݶ�ȡ���,д���ļ�,������Դ
				fs << "\r\n";
				clear_resource();
				return;
			}
			else if (error)
			{
				throw boost::system::system_error{ error };
			}

			//��������
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

