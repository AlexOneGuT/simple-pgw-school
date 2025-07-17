#include <data_plane.h>

#include <gtest/gtest.h>

class mock_data_plane_forwarding : public data_plane {
public:
    explicit mock_data_plane_forwarding(control_plane &control_plane) : data_plane(control_plane) {}

    std::unordered_map<boost::asio::ip::address_v4, std::unordered_map<uint32_t, std::vector<Packet>>>
            _forwarded_to_sgw;
    std::unordered_map<boost::asio::ip::address_v4, std::vector<Packet>> _forwarded_to_apn;

protected:
    void forward_packet_to_sgw(boost::asio::ip::address_v4 sgw_addr, uint32_t sgw_dp_teid, Packet &&packet) override {
        _forwarded_to_sgw[sgw_addr][sgw_dp_teid].emplace_back(std::move(packet));
    }

    void forward_packet_to_apn(boost::asio::ip::address_v4 apn_gateway, Packet &&packet) override {
        _forwarded_to_apn[apn_gateway].emplace_back(std::move(packet));
    }
};

class data_plane_test : public ::testing::Test {
public:
    static const inline std::string apn{"test.apn"};
    static const inline auto apn_gw{boost::asio::ip::make_address_v4("127.0.0.1")};
    static const inline auto sgw_addr{boost::asio::ip::make_address_v4("127.1.0.1")};
    static constexpr auto sgw_default_bearer_teid{1};
    static constexpr auto sgw_ded_bearer_teid{2};

    data_plane_test() {
        _control_plane.add_apn(apn, apn_gw);
        _pdn = _control_plane.create_pdn_connection(apn, sgw_addr, sgw_default_bearer_teid);

        _default_bearer = _control_plane.create_bearer(_pdn, sgw_default_bearer_teid);
        _pdn->set_default_bearer(_default_bearer);

        _dedicated_bearer = _control_plane.create_bearer(_pdn, sgw_ded_bearer_teid);
    }

    std::shared_ptr<pdn_connection> _pdn;
    std::shared_ptr<bearer> _default_bearer;
    std::shared_ptr<bearer> _dedicated_bearer;
    control_plane _control_plane;
    mock_data_plane_forwarding _data_plane{_control_plane};
};

TEST_F(data_plane_test, handle_downlink_for_pdn) {
    data_plane::Packet packet1{1, 2, 3};
    _data_plane.handle_uplink(_pdn->get_default_bearer()->get_dp_teid(), {packet1.begin(), packet1.end()});

    data_plane::Packet packet2{4, 5, 6};
    _data_plane.handle_uplink(_dedicated_bearer->get_dp_teid(), {packet2.begin(), packet2.end()});

    data_plane::Packet packet3{7};
    _data_plane.handle_downlink(_pdn->get_ue_ip_addr(), {packet3.begin(), packet3.end()});

    ASSERT_EQ(1, _data_plane._forwarded_to_sgw.size());
    ASSERT_EQ(packet3, _data_plane._forwarded_to_sgw[sgw_addr][sgw_default_bearer_teid][0]);

    ASSERT_EQ(1, _data_plane._forwarded_to_apn.size());
    ASSERT_EQ(2, _data_plane._forwarded_to_apn[apn_gw].size());
    ASSERT_EQ(packet1, _data_plane._forwarded_to_apn[apn_gw][0]);
    ASSERT_EQ(packet2, _data_plane._forwarded_to_apn[apn_gw][1]);
}

TEST_F(data_plane_test, handle_uplink_for_default_bearer) {
    data_plane::Packet packet1{1, 2, 3};
    _data_plane.handle_uplink(_pdn->get_default_bearer()->get_dp_teid(), {packet1.begin(), packet1.end()});

    ASSERT_EQ(1, _data_plane._forwarded_to_apn.size());
    ASSERT_EQ(1, _data_plane._forwarded_to_apn[apn_gw].size());
    ASSERT_EQ(packet1, _data_plane._forwarded_to_apn[apn_gw][0]);
}

TEST_F(data_plane_test, handle_uplink_for_dedicated_bearer) {
    data_plane::Packet packet1{1, 2, 3};
    _data_plane.handle_uplink(_dedicated_bearer->get_dp_teid(), {packet1.begin(), packet1.end()});

    ASSERT_EQ(1, _data_plane._forwarded_to_apn.size());
    ASSERT_EQ(1, _data_plane._forwarded_to_apn[apn_gw].size());
    ASSERT_EQ(packet1, _data_plane._forwarded_to_apn[apn_gw][0]);
}

TEST_F(data_plane_test, didnt_handle_uplink_for_unknown_bearer) {
    data_plane::Packet packet1{1, 2, 3};
    _data_plane.handle_uplink(UINT32_MAX, {packet1.begin(), packet1.end()});

    ASSERT_TRUE(_data_plane._forwarded_to_apn.empty());
}

TEST_F(data_plane_test, didnt_handle_downlink_for_unknown_ue_ip) {
    data_plane::Packet packet1{1, 2, 3};
    _data_plane.handle_downlink(boost::asio::ip::address_v4::any(), {packet1.begin(), packet1.end()});

    ASSERT_TRUE(_data_plane._forwarded_to_apn.empty());
}
