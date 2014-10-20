#include <tools/log.hpp>
#include <hardware/device/FWiegandReader.hpp>
#include "RplethModule.hpp"
#include "rplethprotocol.hpp"

RplethModule::RplethModule(zmqpp::context &ctx,
        zmqpp::socket *pipe,
        const boost::property_tree::ptree &cfg) :
        BaseModule(ctx, pipe, cfg),
        ctx_(ctx),
        server_(ctx, zmqpp::socket_type::stream),
        bus_sub_(ctx, zmqpp::socket_type::sub),
        reader_(nullptr),
        stream_mode_(false)
{
    process_config();
    bus_sub_.connect("inproc://zmq-bus-pub");
    bus_sub_.subscribe("S_" + reader_->name());
    reactor_.add(server_, std::bind(&RplethModule::handle_socket, this));
    reactor_.add(bus_sub_, std::bind(&RplethModule::handle_wiegand_event, this));
}

RplethModule::~RplethModule()
{
    delete reader_;
}

void RplethModule::process_config()
{
    boost::property_tree::ptree module_config = config_.get_child("module_config");

    uint16_t port = module_config.get<uint16_t>("port", 4242);
    std::string reader_name = module_config.get_child("reader").data();
    stream_mode_ = module_config.get<bool>("stream_mode", false);

    LOG() << "Rpleth module will bind to " << port << " and will control the device nammed " << reader_name;
    reader_ = new FWiegandReader(ctx_, reader_name);
    assert(reader_);
    server_.bind("tcp://*:" + std::to_string(port));
    LOG() << "bind ok";
}

void RplethModule::handle_socket()
{
    zmqpp::message msg;
    std::string identity;
    std::string content;

    server_.receive(msg);
    msg >> identity >> content;

    if (content.size() == 0)
    {
        // handle special 0 length message that indicates connection / disconnection.
        if (client_connected(identity)) // client exists so this is a disconnection message.
        {
            LOG() << "client disconnected";
            clients_.erase(identity);
            if (client_failed(identity))
                failed_clients_.erase(std::remove(failed_clients_.begin(), failed_clients_.end(), identity), failed_clients_.end());
        }
        else
        {
            LOG() << "Client connected";
            clients_[identity];
        }
        return;
    }
    if (client_failed(identity))
        return;
    assert(clients_.count(identity) && !client_failed(identity));
    clients_[identity].write(reinterpret_cast<const uint8_t *> (content.c_str()), content.size());
    if (handle_client_msg(identity, clients_[identity]) == false)
        failed_clients_.push_back(identity);
}

bool RplethModule::handle_client_msg(const std::string &client_identity, CircularBuffer &buf)
{
    RplethPacket packet(RplethPacket::Sender::Client);

    do
    {
        std::array<uint8_t, buffer_size> buffer;
        packet = RplethProtocol::decodeCommand(buf);
        if (!packet.isGood)
            break;
        RplethPacket response = handle_client_packet(packet);
        std::size_t size = RplethProtocol::encodeCommand(response, &buffer[0], buffer_size);

        zmqpp::message msg;
        msg << client_identity;
        msg.add_raw(&buffer[0], size);
        if (!server_.send(msg, true))
        {
            // would block: peer is probably disconnected already.
            return false;
        }
    }
    while (packet.isGood && buf.toRead());
    return true;
}

RplethPacket RplethModule::handle_client_packet(RplethPacket packet)
{
    RplethPacket response = packet;

    LOG() << "received client packet";
    response.sender = RplethPacket::Sender::Server;
    if (response.type == RplethProtocol::Rpleth && response.command == RplethProtocol::Ping)
    {
        response.status = RplethProtocol::Success;
    }
    else if (response.type == RplethProtocol::TypeCode::HID && response.command == RplethProtocol::HIDCommands::Greenled)
    {
        assert(packet.data.size() > 0);
        if (packet.data[0] == 0x01)
            reader_->greenLedOn();
        else if (packet.data[0] == 0x00)
            reader_->greenLedOff();
        else
            LOG() << "Invalid packet";
    }
    else if (response.type == RplethProtocol::TypeCode::HID && response.command == RplethProtocol::HIDCommands::Beep)
    {
        assert(packet.data.size() > 0);
        if (packet.data[0] == 0x01)
            reader_->buzzerOn();
        else if (packet.data[0] == 0x00)
            reader_->buzzerOff();
        else
            LOG() << "Invalid packet";
    }
    else if (response.type ==RplethProtocol::TypeCode::HID && response.command == RplethProtocol::HIDCommands::SendCards)
    {
        handle_send_cards(packet);
    }
    else if (response.type ==RplethProtocol::TypeCode::HID && response.command == RplethProtocol::HIDCommands::ReceiveCardsWaited)
    {
        response = handle_receive_cards(response);
    }
    else
    {
        response.status = RplethProtocol::Success; // Default response
    }
    return (response);
}

void RplethModule::handle_send_cards(RplethPacket packet)
{
    auto itr_start = packet.data.begin();
    std::vector<Byte>::iterator my_start = itr_start;
    std::vector<Byte>::iterator it;

    LOG() << "Handle_send_cards";
    cards_pushed_.clear();
    cards_read_.clear();

    while (my_start != packet.data.end())
    {
        it = std::find(itr_start, packet.data.end(), '|');
        std::string card;
        while (my_start != packet.data.end() && my_start != it)
        {
            card += *my_start;
            my_start++;
        }
        LOG() << "FOUND CARD {" << card << "}";
        cards_pushed_.push_back(card);
        if (my_start == packet.data.end())
            break;
        else
            my_start++;
        itr_start = ++it;
    }
    cards_pushed_.unique();
}

void RplethModule::handle_wiegand_event()
{
    zmqpp::message msg;
    std::string card_id;

    bus_sub_.receive(msg);
    msg >> card_id >> card_id;

    LOG() << "Rpleth module register card {" << card_id << "}";
    card_id.erase(std::remove(card_id.begin(), card_id.end(), ':'), card_id.end());
    if (std::find(cards_pushed_.begin(), cards_pushed_.end(), card_id) != cards_pushed_.end())
    {
        cards_read_.push_back(card_id);
    }
    else
    {
        // card not found
        LOG() << "This card shouldnt register {" << card_id << "}";
        reader_->beep(3000);
    }
    if (stream_mode_)
        cards_read_stream_.push_back(card_id);

    cards_read_.unique();
}

RplethPacket RplethModule::handle_receive_cards(RplethPacket packet)
{
    std::list<std::string> to_send;
    RplethPacket response = packet;

    LOG() << "Packet size = " << packet.data.size();
    if (packet.data.size() != 1)
    {
        LOG() << "Invalid Packet";
        return response;
    }
    if (packet.data[0] == 0x01)
    {
        LOG() << "Present list";
        // send present list
        to_send = cards_read_;
    }
    else
    {
        LOG() << "Absent list";
        // send absent list
        to_send = cards_pushed_;
        auto lambda = [this] (const std::string &str) -> bool
        {
            // if entry is not in cards_read_ means user was absent, do not remove him
            bool found = std::find(cards_read_.begin(), cards_read_.end(), str) != cards_read_.end();
            return found;
        };
        to_send.erase(std::remove_if(to_send.begin(), to_send.end(), lambda), to_send.end());
    }
    // we reserve approximately what we need, just to avoid useless memory allocations.
    std::vector<Byte> data;
    data.reserve(to_send.size() * 8);
    for (auto &card : to_send)
    {
        LOG() << "Current size of data vector = " << data.size();
        LOG() << "Will store the card {" << card << "} into the rplet packet to send.";
        // we need to convert the card (ff:ae:32:00) to something like "ffae3200"
        card.erase(std::remove(card.begin(), card.end(), ':'), card.end());
        data.insert(data.end(), card.begin(), card.end());
        LOG() << "Card {" << card << "} has been stored into the rplet packet";
        data.push_back('|');
    }
    LOG() << "Built data vector (size = " << data.size() << ")";
    response.data = data;
    response.dataLen = data.size();
    return response;
}

bool RplethModule::client_connected(const std::string &identity) const
{
    return clients_.count(identity);
}

bool RplethModule::client_failed(const std::string &identity) const
{
    return (std::find(failed_clients_.begin(), failed_clients_.end(), identity) != failed_clients_.end());
}
