#include <data_plane.h>
using namespace boost::asio::ip;

data_plane::data_plane(control_plane &control_plane) : _control_plane(control_plane) {}

void data_plane::handle_uplink(uint32_t dp_teid, Packet &&packet) {
    if (auto bearer = _control_plane.find_bearer_by_dp_teid(dp_teid)) {
        if (bearer->check_uplink_rate(packet.size())) {
            if (auto pdn = bearer->get_pdn_connection()) {
                forward_packet_to_apn(pdn->get_apn_gw(), std::move(packet));
            }
        }
    }
}

void data_plane::handle_downlink(const address_v4 &ue_ip, Packet &&packet) {
    if (auto pdn = _control_plane.find_pdn_by_ip_address(ue_ip)) {
        if (auto bearer = pdn->get_default_bearer()) {
            if (bearer->check_downlink_rate(packet.size())) {
                forward_packet_to_sgw(pdn->get_sgw_address(), bearer->get_sgw_dp_teid(), std::move(packet));
            }
        }
    }
}