/*
 * pipe.cpp
 *
 *  Created on: Jun 8, 2015
 *      Author: j
 */

#include "pipe.h"

Pipe::Pipe(
        ugcs::vsm::Request_worker::Ptr w,
        std::string prefix):
        worker(w)
{
    auto pr = ugcs::vsm::Properties::Get_instance();
    std::string t = pr->Get(prefix + ".type");
    if (t == "tcp_out") {
        type = TCP_OUT;
        name2 = pr->Get(prefix + ".remote_address");
        port2 = pr->Get(prefix + ".remote_port");
    } else if (t == "tcp_in") {
        type = TCP_IN;
        if (pr->Exists(prefix + ".local_address")) {
            name1 = pr->Get(prefix + ".local_address");
        } else {
            name1 = "0.0.0.0";
        }
        port1 = pr->Get(prefix + ".local_port");
    } else if (t == "udp") {
        type = UDP;
        if (pr->Exists(prefix + ".local_address")) {
            name1 = pr->Get(prefix + ".local_address");
        } else {
            name1 = "0.0.0.0";
        }
        port1 = pr->Get(prefix + ".local_port");
        name2 = pr->Get(prefix + ".remote_address");
        port2 = pr->Get(prefix + ".remote_port");
    } else if (t == "serial") {
        type = SERIAL;
        name1 = pr->Get(prefix + ".name");
        baud = pr->Get_int(prefix + ".baud");
    } else if (t == "pipe") {
        type = PIPE;
        name1 = pr->Get(prefix + ".name");
    } else if (t == "can") {
        type = CAN;
        name1 = pr->Get(prefix + ".name");
    } else if (t == "file") {
        type = FILE;
        name1 = pr->Get(prefix + ".name");
    }
}

Pipe::~Pipe()
{
    LOG("Pipe destructor");
}

bool
Pipe::Is_connected()
{
    return (stream && !stream->Is_closed());
}

std::string
Pipe::Get_name()
{
    switch (type) {
    case TCP_IN:
        return "tcp_in " + name1 + ":" + port1;
    case TCP_OUT:
        return "tcp_out " + name2 + ":" + port2;
    case UDP:
        return "udp " + name1 + ":" + port1 + "-" + name2 + ":" + port2;
    case PIPE:
        return "pipe " + name1;
    case SERIAL:
        return "serial " + name1 + ":" + std::to_string(baud);
    default:
        return "";
    }
}

void
Pipe::Peer_connected()
{
    if (type == FILE) {
        try {
            stream = ugcs::vsm::File_processor::Get_instance()->Open(
                    name1,
                    "r+",
                    false);
            Schedule_read();
        } catch (ugcs::vsm::Exception&) {
            stream = nullptr;
        }
        connect_in_progress = false;
    }
}

bool
Pipe::On_timer()
{
    if (!Is_connected() && !connect_in_progress) {
        Connect();
    }
    return true;
}

ugcs::vsm::Io_stream::Ref
Pipe::Get_stream()
{
    return stream;
}

void
Pipe::Set_peer(std::shared_ptr<Pipe> p)
{
    if (peer == nullptr && p) {
        ugcs::vsm::Timer_processor::Get_instance()->Create_timer(
                std::chrono::seconds(1),
                ugcs::vsm::Make_callback(
                        &Pipe::On_timer,
                        shared_from_this()),
                worker);
        peer = p;
        Connect();
    }
}

void
Pipe::On_write_complete(ugcs::vsm::Io_result res)
{
    if (res == ugcs::vsm::Io_result::OK) {
        Schedule_read();
    } else {
        LOG("Write failed");
        if (peer && peer->Get_stream()) {
            peer->Get_stream()->Close();
        }
    }
}

void
Pipe::On_read(ugcs::vsm::Io_buffer::Ptr buf, ugcs::vsm::Io_result res)
{
    if (res == ugcs::vsm::Io_result::OK) {
//      LOG("Got data! %s", buf->Get_hex().c_str());
        if (peer && peer->Get_stream()) {
            peer->Get_stream()->Write(
                    buf,
                    ugcs::vsm::Make_write_callback(
                            &Pipe::On_write_complete,
                            shared_from_this()));
        } else {
            Schedule_read();
        }
    } else {
        LOG("Closed");
        stream->Close();
        stream = nullptr;
    }
}

void
Pipe::Schedule_read()
{
    int to_read;
    if (stream->Get_type() == ugcs::vsm::Io_stream::Type::CAN) {
        to_read = 16;
    } else {
        to_read = 1000;
    }
    stream->Read(to_read, 1,
                 ugcs::vsm::Make_read_callback(
                         &Pipe::On_read,
                         shared_from_this()),
                 worker);
}

void
Pipe::On_tcp_connected(ugcs::vsm::Socket_processor::Stream::Ref s, ugcs::vsm::Io_result res)
{
    if (res == ugcs::vsm::Io_result::OK) {
        LOG("Connected %s:%s - %s:%s",
            s->Get_local_address()->Get_name_as_c_str(),
            s->Get_local_address()->Get_service_as_c_str(),
            s->Get_peer_address()->Get_name_as_c_str(),
            s->Get_peer_address()->Get_service_as_c_str());
        stream = s;
        Schedule_read();
        if (peer) {
            peer->Peer_connected();
        }
    }
    if (listener) {
        listener->Close();
        listener = nullptr;
    }
    connect_in_progress = false;
}

void
Pipe::Connect()
{
    connect_in_progress = true;
    switch (type) {
    case TCP_IN:
        ugcs::vsm::Socket_processor::Get_instance()->Listen(
                name1,
                port1,
                ugcs::vsm::Make_socket_listen_callback(
                        [&](ugcs::vsm::Socket_processor::Stream::Ref s, ugcs::vsm::Io_result res)
                        {
                            if (res == ugcs::vsm::Io_result::OK) {
                                LOG("Listen Ok %s", Get_name().c_str());
                                listener = s;
                                ugcs::vsm::Socket_processor::Get_instance()->Accept(
                                        listener,
                                        ugcs::vsm::Make_socket_accept_callback(
                                                &Pipe::On_tcp_connected,
                                                shared_from_this()),
                                        worker);
                            } else {
                                LOG("Listen failed %s", Get_name().c_str());
                                connect_in_progress = false;
                            }
                        }),
                worker);
        break;
    case TCP_OUT:
        ugcs::vsm::Socket_processor::Get_instance()->Connect(
                name2,
                port2,
                ugcs::vsm::Make_socket_connect_callback(
                        &Pipe::On_tcp_connected,
                        shared_from_this()),
                worker);
        break;
    case UDP:
        ugcs::vsm::Socket_processor::Get_instance()->Bind_udp(
                ugcs::vsm::Socket_address::Create(name1, port1),
                ugcs::vsm::Make_socket_listen_callback(
                        [&](ugcs::vsm::Socket_processor::Stream::Ref s, ugcs::vsm::Io_result res)
                        {
                            if (res == ugcs::vsm::Io_result::OK) {
                                LOG("Bind_udp Ok");
                                s->Set_peer_address(ugcs::vsm::Socket_address::Create(name2, port2));
                                stream = s;
                                Schedule_read();
                                if (peer) {
                                    peer->Peer_connected();
                                }
                            } else {
                                LOG("Bind_udp failed");
                            }
                            connect_in_progress = false;
                        }),
                worker);
        break;
    case CAN:
        ugcs::vsm::Socket_processor::Get_instance()->Bind_can(
                name1,
                std::vector<int>(),
                ugcs::vsm::Make_socket_listen_callback(
                        [&](ugcs::vsm::Socket_processor::Stream::Ref s, ugcs::vsm::Io_result res)
                        {
                            if (res == ugcs::vsm::Io_result::OK) {
                                LOG("Bind_can Ok");
                                stream = s;
                                Schedule_read();
                                if (peer) {
                                    peer->Peer_connected();
                                }
                            } else {
                                LOG("Bind_can failed");
                            }
                            connect_in_progress = false;
                        }),
                worker);
        break;
    case SERIAL:
        if (ugcs::vsm::File_processor::Get_instance()->Access_utf8(name1, W_OK) == 0) {
            try {
                auto mode = ugcs::vsm::Serial_processor::Stream::Mode().Baud(baud);
                stream = ugcs::vsm::Serial_processor::Get_instance()->Open(
                        name1,
                        mode);
                Schedule_read();
                if (peer) {
                    peer->Peer_connected();
                }
            } catch (ugcs::vsm::Exception&) {
                stream = nullptr;
            }
        }
        connect_in_progress = false;
        break;
    case PIPE:
//        if (ugcs::vsm::File_processor::Get_instance()->Access_utf8(name1, W_OK) == 0) {
            try {
                stream = ugcs::vsm::File_processor::Get_instance()->Open(
                        name1,
                        "r+",
                        false);
                Schedule_read();
                if (peer) {
                    peer->Peer_connected();
                }
                LOG("Pipe opened");
            } catch (ugcs::vsm::Exception&) {
                stream = nullptr;
            }
  //      }
        connect_in_progress = false;
        break;
    case FILE:
        break;
    }
}
