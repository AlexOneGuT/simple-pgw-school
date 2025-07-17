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

TEST_F(data_plane_test, zero_rate_limit_allows_all_packets) {
    // Лимит 0 - без ограничений
    _default_bearer->set_uplink_rate(0);
    
    data_plane::Packet packet(1000, 0xBB);
    
    // Много пакетов подряд должны проходить
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(_default_bearer->check_uplink_rate(packet.size()));
    }
}

TEST_F(data_plane_test, rate_limiter_allows_packets_within_limit) {
    // 8000 бит/сек = 1000 байт/сек
    _default_bearer->set_uplink_rate(8000);
    
    // Пакет 4000 бит (500 байт)
    const data_plane::Packet packet(500, 0xAA);
    
    // Первые два пакета должны пройти (8000 - 4000 - 4000 = 0)
    EXPECT_TRUE(_default_bearer->check_uplink_rate(packet.size()));
    EXPECT_TRUE(_default_bearer->check_uplink_rate(packet.size()));
    
    // Третий пакет должен быть отклонён
    EXPECT_FALSE(_default_bearer->check_uplink_rate(packet.size()));
    
    // Ждём 0.5 сек (должно накопиться 4000 бит)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_TRUE(_default_bearer->check_uplink_rate(packet.size()));
}

TEST_F(data_plane_test, data_plane_respects_rate_limits) {
    // 800 бит/сек = 100 байт/сек
    _default_bearer->set_uplink_rate(800);
    
    // Пакет 800 бит (100 байт)
    data_plane::Packet packet(100, 0xBB);
    
    // Первый пакет должен пройти
    _data_plane.handle_uplink(_default_bearer->get_dp_teid(), std::move(packet));
    ASSERT_EQ(1, _data_plane._forwarded_to_apn[apn_gw].size());
    
    // Второй пакет должен быть отклонён
    data_plane::Packet packet2(100, 0xCC);
    _data_plane.handle_uplink(_default_bearer->get_dp_teid(), std::move(packet2));
    ASSERT_EQ(1, _data_plane._forwarded_to_apn[apn_gw].size());
    
    // Ждём 1 сек и проверяем, что новый пакет проходит
    std::this_thread::sleep_for(std::chrono::seconds(1));
    data_plane::Packet packet3(100, 0xDD);
    _data_plane.handle_uplink(_default_bearer->get_dp_teid(), std::move(packet3));
    ASSERT_EQ(2, _data_plane._forwarded_to_apn[apn_gw].size());
}

TEST_F(data_plane_test, dynamic_rate_change) {
    // Устанавливаем начальную скорость 1000 бит/сек
    _default_bearer->set_uplink_rate(1000); // 1 Кбит/сек
    
    // Пакет 1000 бит (125 байт)
    data_plane::Packet packet(125, 0xAA);
    
    // Первый пакет должен пройти (начальные токены = 1000)
    EXPECT_TRUE(_default_bearer->check_uplink_rate(packet.size()));
    
    // Второй пакет не должен пройти (токенов 0)
    EXPECT_FALSE(_default_bearer->check_uplink_rate(packet.size()));
    
    // Увеличиваем скорость до 2000 бит/сек (2 Кбит/сек)
    _default_bearer->set_uplink_rate(2000);
    
    // Теперь пакет должен пройти (новый лимит, токены сброшены до 2000)
    EXPECT_TRUE(_default_bearer->check_uplink_rate(packet.size()));
    
    // Следующий пакет такого же размера тоже должен пройти (осталось 1000 токенов)
    EXPECT_TRUE(_default_bearer->check_uplink_rate(packet.size()));
    
    // Третий пакет не должен пройти (токенов 0)
    EXPECT_FALSE(_default_bearer->check_uplink_rate(packet.size()));
}

TEST_F(data_plane_test, rate_limiter_allows_burst) {
    // Устанавливаем скорость 8000 бит/сек (1 КБ/сек)
    _default_bearer->set_uplink_rate(8000);
    
    // Пакет 800 бит (100 байт)
    data_plane::Packet packet(100, 0xBB);
    
    // Первые 10 пакетов должны пройти (10*800 = 8000 бит)
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(_default_bearer->check_uplink_rate(packet.size()))
            << "Packet " << i << " should pass";
    }
    
    // Следующий пакет должен быть отклонён (токенов 0)
    EXPECT_FALSE(_default_bearer->check_uplink_rate(packet.size()));
    
    // Ждём 1 секунду для накопления токенов
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // После ожидания снова можно отправить пакеты
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(_default_bearer->check_uplink_rate(packet.size()))
            << "Packet after sleep " << i << " should pass";
    }
}