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
	//��ȡ��ǰʱ����ַ���
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

	//��ӡasio error
	void asio_err_msg(boost::system::system_error& se)
	{
		std::cout << err_header << se.code() << ":" << se.what() << " " << get_time_now();
	}

	//��ӡasio prompt
	void asio_prompt_msg(std::string prompt_context)
	{
		std::cout << prompt_header << prompt_context << " " << get_time_now();
	}
	//��ӡasio log
	void asio_log_msg(std::string log_context)
	{
		std::cout << log_header << log_context << " " << get_time_now();
	}

	/**************************************************************************/

	namespace sync {

		/*
			һ��ͬ��������:
				1:��ʼ�������Դ,acceptor�󶨼����˿�,accept��ʼ�ȴ��ͻ�������
				2:�ڲ�����ֻ�Ǽ򵥵İ��û����͵����ݷ��ͻ�ȥ
			
			learn:
				1:��c/sͬʱʹ�õ��߳�����ģʽ��ʱ��,���׶�������read֮��:
					planA:�����������ó�ʱʱ��,ʱ��һ����closesocket
					planB:ʹ�����߳�ִ�пͻ��˵Ķ�ȡ����.

					planA:
						:ʹ��b_timeout��Ϊ����־,�ڼ�ʱ���󶨳���֮�����ֵ,������ture,�͹ر�����
						{
							0:�Լ����ü�ʱ��
							1:b_timeout�����ֹ����
							2:read������Ȼ������,ֱ�����յ����ݺ󷵻�,Ȼ������ѭ��
						}
						planA isn't good;
					planB:
						:�ͻ���ʹ�����̶߳�ȡ����,���߳�д������.�ͻ��������Ͽ�����.
						{
							0:�ͻ���ʹ�����̶߳�ȡ����
							1:�ͻ������߳�д������
							2:������߼���Ȼ���ּ�.
						}
				 ��Ȼ���ڵ�����:
					1:cout<<array.data��ʱ��,�����ո�ỻ��.  ...�����

				2019-2-20

				1:������Զ����ȡ���صı�־read_compl_handler,��read������ȡ���ض���ֵ(�������õ���0x10)�����������ʱ��᷵��
				2:����cout<<array.data�������е�����������,�ͻ���ʹ��cin>>string,���Զ������հ׷�,����hello world������δ���,��������Ҳ�ͻ�ֿ����д�ӡ
				3:write���ᷢ�����������������,���������յ�����Ϣ����.
				4:��ӡ��Ϣʱ�����һ��ʱ���
				::_test_plan_aδ���Ķ�.

				2019-2-22
		*/


		using namespace std;
		using namespace boost::asio;
		using namespace boost::asio::ip;

		
		array<char, def_buf> read_buf{};	//��ȡ������
		
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
						break;	//������ptimeout==falseʱ,˵���Ѿ���ʱ��.
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

			//���ö�ȡ��������
			auto read_compl_handler = [](const boost::system::error_code& ec,size_t bytes) 
			{
				if (ec)
					throw boost::system::system_error{ ec };	//���쳣�׳�
				for (offset; offset < bytes; ++offset)
				{
					if (read_buf[offset] == end_flag)
					{
						offset = 0;							//�����ȡ���������Ľ�����,������ƫ����
						return static_cast<size_t>(0);		//����0��ʶ�����˴ζ�ȡ
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
						break;	//����ѭ�� ������Դ
					}
					std::string read_msg = log_server_read_msg_header + std::to_string(bytes_read) + log_server_read_msg_foot;
					asio_log_msg(read_msg);
					try {
						bytes_write = boost::asio::write(sock, buffer(read_buf.data(), bytes_read));
					}
					catch (boost::system::system_error& se)
					{
						asio_err_msg(se);
						break;	//����ѭ�� ������Դ
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
			һ���첽������:
				1:��ʼ�������Դ
				2:�ȴ��ͻ�����
				3:��ȡ/д������
				4:�յ�eof��־,�Ͽ�����,�ظ�����2

			learn:
				1:��Ϊ�첽��־�Ǽ�ʱ���ص�,������Ҫ����һЩ������������������,������main_loop��,socket��Ҫʹ�ö��ڴ�,������ջ�ռ�
				2:��ʹ��std::bind�󶨲�����ʱ��,�������ò���,��Ҫʹ��std::ref(refrence)����,������벻ͨ��
				3:��read_compl_handler��,Ҳ���ȡ��eof��־,������û��ֱ�Ӵ���,ֻ���ú���������,��read_handler�д���.
				4:��ͬ��ͬ������ʹ��whileѭ��,������Ҫ�Լ��ֶ�����,���յ�eof�Ͽ����Ӻ�,ʹ��is.post(main_loop),ִ����һ�ֺ���

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
			//�������ﴦ��eof��־,�ö�ȡ����
			if (ec == boost::asio::error::eof)
				return 0;
			else if (ec)
				throw boost::system::system_error{ ec };
			for (offset; offset < bytes; ++offset)
			{
				if (buf[offset] == end_flag)	//ֱ����ȡend_flag��־����
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
			//��ѭ��
			asio_log_msg(log_server_wait_msg);
			//tcp::socket sock{ is };
			tcp::socket* sock = new tcp::socket{ is };	//��Ϊ��Ҫ���뿪main_loop����ʱ����sock����������,����ʹ�ö��ڴ�
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
				//��socket�ж�ȡ����
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
			//�����ﴦ��eof��־,�����˿ͻ���"����"�Ͽ�����,������Ͽ����ӵȴ��¿ͻ�����
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

			//��������
			std::string read_msg = log_server_read_msg_header + to_string(bytes) + log_server_read_msg_foot;
			asio_log_msg(read_msg);
			//д������
			boost::asio::async_write(sock, buffer(buf.data(), bytes), write_handler);
			//������ȡ
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

		//д�봦����,����¼д����ֽ���Ŀ,���쳣���׳�
		void write_handler(const boost::system::error_code& ec, size_t bytes)
		{
			if (ec)
				throw boost::system::system_error{ ec };
			std::string write_msg = log_server_write_msg_header + to_string(bytes) + log_server_write_msg_foot;
			asio_log_msg(write_msg);
		}
	}


}