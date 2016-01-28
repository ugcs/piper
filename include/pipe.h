/*
 * pipe.h
 *
 *  Created on: Jun 8, 2015
 *      Author: j
 */

#include <ugcs/vsm/vsm.h>

#ifndef SRC_PIPE_H_
#define SRC_PIPE_H_

class Pipe: public std::enable_shared_from_this<Pipe>{
public:
    typedef enum {
        TCP_IN,
        TCP_OUT,
        UDP,
        FILE,
        PIPE,
        SERIAL,
        CAN
    } Pipe_type;

    Pipe(
        ugcs::vsm::Request_worker::Ptr w,
        std::string property_prefix);
    virtual ~Pipe();

    void
    Set_peer(std::shared_ptr<Pipe> peer);

    void
    Peer_connected();

    std::string
    Get_name();

    ugcs::vsm::Io_stream::Ref
    Get_stream();

private:

    void
    Connect();

    bool
    Is_connected();

    void
    Schedule_read();

    void
    On_read(ugcs::vsm::Io_buffer::Ptr buf, ugcs::vsm::Io_result res);

    void
    On_tcp_connected(ugcs::vsm::Socket_processor::Stream::Ref s, ugcs::vsm::Io_result res);

    void
    On_write_complete(ugcs::vsm::Io_result res);

    bool
    On_timer();

    Pipe_type type;

    bool connect_in_progress = false;

    std::shared_ptr<Pipe> peer = nullptr;
    ugcs::vsm::Request_worker::Ptr worker;
    ugcs::vsm::Io_stream::Ref stream = nullptr;
    ugcs::vsm::Socket_processor::Stream::Ref listener = nullptr;

    std::string name1;   // local ip adress, file name, serial port.
    std::string port1;   // local udp/tcp port number, baud.
    std::string name2;   // peer adress, file name, serial port.
    std::string port2;   // peer udp port number.
    int baud;
};

#endif /* SRC_PIPE_H_ */
