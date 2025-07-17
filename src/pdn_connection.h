#pragma once

#include <bearer.h>

#include <memory>
#include <unordered_map>

class control_plane;

class pdn_connection : public std::enable_shared_from_this<pdn_connection> {
public:
    static std::shared_ptr<pdn_connection> create(uint32_t cp_teid, boost::asio::ip::address_v4 apn_gw,
                                                  boost::asio::ip::address_v4 ue_ip_addr);

    [[nodiscard]] uint32_t get_sgw_cp_teid() const;
    void set_sgw_cp_teid(uint32_t sgw_cp_teid);

    [[nodiscard]] std::shared_ptr<bearer> get_default_bearer() const;
    void set_default_bearer(std::shared_ptr<bearer> bearer);

    [[nodiscard]] boost::asio::ip::address_v4 get_sgw_address() const;
    void set_sgw_addr(boost::asio::ip::address_v4 sgw_addr);

    [[nodiscard]] uint32_t get_cp_teid() const;
    [[nodiscard]] boost::asio::ip::address_v4 get_apn_gw() const;
    [[nodiscard]] boost::asio::ip::address_v4 get_ue_ip_addr() const;
    
    void add_bearer(std::shared_ptr<bearer> bearer); //wow-wow, do it
    void remove_bearer(uint32_t dp_teid); //op-op, do it

private:
    friend control_plane;

    pdn_connection(uint32_t cp_teid, boost::asio::ip::address_v4 apn_gw, boost::asio::ip::address_v4 ue_ip_addr);

    boost::asio::ip::address_v4 _apn_gateway;
    boost::asio::ip::address_v4 _ue_ip_addr;
    uint32_t _cp_teid{};
    uint32_t _sgw_cp_teid{};
    boost::asio::ip::address_v4 _sgw_address;
    std::unordered_map<uint32_t, std::shared_ptr<bearer>> _bearers;
    std::shared_ptr<bearer> _default_bearer;
};
