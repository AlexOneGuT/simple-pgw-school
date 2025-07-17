#include <control_plane.h>
#include <bearer.h>
#include <pdn_connection.h>

using namespace boost::asio::ip;


std::shared_ptr<pdn_connection> control_plane::find_pdn_by_cp_teid(uint32_t cp_teid) const{
    auto pdn_find_result = _pdns.find(cp_teid);
    return pdn_find_result != _pdns.end() ? pdn_find_result->second : nullptr;
}


std::shared_ptr<pdn_connection> control_plane::find_pdn_by_ip_address(const boost::asio::ip::address_v4 &ip) const{
    auto pdn_find_result = _pdns_by_ue_ip_addr.find(ip);
    return pdn_find_result != _pdns_by_ue_ip_addr.end() ? pdn_find_result->second : nullptr;
}

std::shared_ptr<bearer> control_plane::find_bearer_by_dp_teid(uint32_t dp_teid) const{
    auto bearer_find_result = _bearers.find(dp_teid);
    return bearer_find_result != _bearers.end() ? bearer_find_result->second : nullptr;
}

 std::shared_ptr<bearer> control_plane::create_bearer(const std::shared_ptr<pdn_connection> &pdn, uint32_t sgw_teid){
    if (!pdn) return nullptr;

    static uint32_t new_dp_teid = 1;
    uint32_t dp_teid = new_dp_teid++;

    auto bearer_ptr = std::make_shared<bearer>(dp_teid, *pdn);
    bearer_ptr->set_sgw_dp_teid(sgw_teid);

    _bearers[dp_teid] = bearer_ptr;
    pdn->add_bearer(bearer_ptr); //new, but not new method from pdn_connection.h

    return bearer_ptr;
 }

void control_plane::delete_bearer(uint32_t dp_teid){
    auto bearer_find_result = _bearers.find(dp_teid);
    if (bearer_find_result == _bearers.end()) return;

    auto check = bearer_find_result->second;
    if (auto pdn = check->get_pdn_connection()){
        pdn->remove_bearer(dp_teid); //new, but not new method
    }

    _bearers.erase(bearer_find_result);
}

std::shared_ptr<pdn_connection> control_plane::create_pdn_connection(
    const std::string &apn, address_v4 sgw_addr, uint32_t sgw_cp_teid){

        //Go find APN gateway
        auto apn_find_result = _apns.find(apn);
        if (apn_find_result == _apns.end()) return nullptr;

        //Go create some unique cp_teid
        static uint32_t new_cp_teid = 1;
        int32_t cp_teid = new_cp_teid++;

        //Go do ip-address
        static uint32_t next_ue_ip = 0x0A000001;
        address_v4 ue_ip(make_address_v4(next_ue_ip++));

        //Go create PDN-connection
        auto pdn = pdn_connection::create(cp_teid, apn_find_result->second, ue_ip);
        pdn->set_sgw_addr(sgw_addr);
        pdn->set_sgw_cp_teid(sgw_cp_teid);

        //Go store PDN-connections, its final, йоу
        _pdns[cp_teid] = pdn;
        _pdns_by_ue_ip_addr[ue_ip] = pdn;

        return pdn;
    }

void control_plane::delete_pdn_connection(uint32_t cp_teid){
    auto pdns_find_result = _pdns.find(cp_teid);
    if (pdns_find_result == _pdns.end()) return;

    auto pdn = pdns_find_result->second;
    _pdns_by_ue_ip_addr.erase(pdn->get_ue_ip_addr());
    _pdns.erase(pdns_find_result);
}

void control_plane::add_apn(std::string apn_name, address_v4 apn_gateway) {
    _apns.emplace(std::move(apn_name), std::move(apn_gateway));
}

