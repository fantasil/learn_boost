#pragma once
#include<string>


const std::string localhost = "localhost";
const unsigned short def_port = 19001;
const size_t def_buf = 1024;
const char end_flag = 0x10;
const char end_flag_2 = 0x0a;


//server log msg
const std::string log_header{ "system log:" };
const std::string log_server_init_msg{ "init server..." };
const std::string log_server_wait_msg{ "wait for client..." };
const std::string log_server_client_coming{ "client coming..." };
const std::string log_server_client_leaving{ "client leaving..." };
const std::string log_server_close_msg{ "close server..." };
const std::string log_server_read_msg_header{ "read " };
const std::string log_server_read_msg_foot{ " bytes from client..." };
const std::string log_server_write_msg_header{ "write " };
const std::string log_server_write_msg_foot{ " bytes from client..." };

//server prompt msg
const std::string prompt_header{ "system prompt:" };
const std::string prompt_server_client_shutdown_send{ "client closed send channel..." };
const std::string prompt_server_client_shutdown_recv{ "client closed recv channel..." };
const std::string prompt_server_client_shutdown_both{ "client closed channels" };
const std::string prompt_server_server_shutdown_send{ "server closed send channel..." };
const std::string prompt_server_server_shutdown_recv{ "server closed recv channel..." };
const std::string prompt_server_server_shutdown_both{ "server closed channels.." };

//server err msg
const std::string err_header{ "\nsystem err " };
