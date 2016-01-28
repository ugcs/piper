
#include "pipe.h"
#include <ugcs/vsm/vsm.h>
#include <fstream>

int
main(int argc, char *argv[])
{
    ugcs::vsm::Request_worker::Ptr worker = ugcs::vsm::Request_worker::Create("piper worker");
    worker->Enable();
    std::string config_file = "piper.conf";
    if (argc > 1) {
        config_file = std::string(argv[1]);
    }
    ugcs::vsm::Log::Set_level("debug");
    auto properties = ugcs::vsm::Properties::Get_instance();
    auto prop_stream = std::unique_ptr<std::fstream>
        (new std::fstream(config_file, std::ios_base::in));
    if (!prop_stream->is_open()) {
        LOG("Cannot open configuration file: %s",
                config_file.c_str());
        return 1;
    }
    properties->Load(*prop_stream);

    auto tp = ugcs::vsm::Timer_processor::Get_instance();
    auto sp = ugcs::vsm::Socket_processor::Get_instance();
    auto serp = ugcs::vsm::Serial_processor::Get_instance();
    auto fp = ugcs::vsm::File_processor::Get_instance();
    tp->Enable();
    sp->Enable();
    serp->Enable();
    fp->Enable();

    for (auto base_it = properties->begin("pipe"); base_it != properties->end(); base_it++) {
        try {
            std::string service_base("pipe.");
            if (base_it.Get_count() == 4) {
                service_base += base_it[1] + ".";
            }
            if (    *base_it == service_base + "first.type"
                &&  properties->Exists(service_base + "second.type"))
            {
                auto peer1 = std::make_shared<Pipe>(worker, service_base + "first");
                auto peer2 = std::make_shared<Pipe>(worker, service_base + "second");
                peer1->Set_peer(peer2);
                peer2->Set_peer(peer1);
            }
        } catch (ugcs::vsm::Exception&) {
        }
    }

    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
