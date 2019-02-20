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