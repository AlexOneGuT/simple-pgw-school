#pragma once

#include <boost/asio/ip/address_v4.hpp>

class pdn_connection;

class bearer {
public:
    bearer(uint32_t dp_teid, pdn_connection &pdn);

    [[nodiscard]] uint32_t get_sgw_dp_teid() const;
    void set_sgw_dp_teid(uint32_t sgw_cp_teid);

    [[nodiscard]] uint32_t get_dp_teid() const;

    [[nodiscard]] std::shared_ptr<pdn_connection> get_pdn_connection() const;

private:
    uint32_t _sgw_dp_teid{};
    uint32_t _dp_teid{};
    pdn_connection &_pdn;
};
