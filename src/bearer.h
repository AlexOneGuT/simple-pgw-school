#pragma once

#include <boost/asio/ip/address_v4.hpp>
#include <chrono>
#include <atomic>
#include <mutex>

class pdn_connection;

class RateLimiter {
public:
    explicit RateLimiter(uint32_t rate_bps) 
        : _rate_bps(rate_bps),
          _tokens(static_cast<double>(rate_bps)),
          _last_update(std::chrono::steady_clock::now()) {}

    bool consume(uint32_t packet_size_bits) {
        if (_rate_bps.load(std::memory_order_relaxed) == 0) {
            return true;
        }

        std::lock_guard<std::mutex> lock(_mutex);
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - _last_update).count();
        _last_update = now;

        // Добавляем новые токены
        const uint32_t current_rate = _rate_bps.load(std::memory_order_relaxed);
        _tokens += current_rate * elapsed;
        
        // Ограничиваем максимум
        if (_tokens > current_rate) {
            _tokens = static_cast<double>(current_rate);
        }

        // Проверяем достаточно ли токенов
        if (_tokens >= packet_size_bits) {
            _tokens -= packet_size_bits;
            return true;
        }
        return false;
    }

    void set_rate(uint32_t rate_bps) {
        std::lock_guard<std::mutex> lock(_mutex);
        _rate_bps.store(rate_bps, std::memory_order_relaxed);
        // При изменении скорости сбрасываем токены
        _tokens = static_cast<double>(rate_bps);
    }

private:
    std::atomic<uint32_t> _rate_bps;
    double _tokens;
    std::chrono::steady_clock::time_point _last_update;
    std::mutex _mutex;
};

class bearer {
public:
    bearer(uint32_t dp_teid, pdn_connection &pdn, uint32_t uplink_rate = 0, uint32_t downlink_rate = 0);

    [[nodiscard]] uint32_t get_sgw_dp_teid() const;
    void set_sgw_dp_teid(uint32_t sgw_cp_teid);

    [[nodiscard]] uint32_t get_dp_teid() const;

    [[nodiscard]] std::shared_ptr<pdn_connection> get_pdn_connection() const;

    // Проверка скорости для uplink трафика (размер в байтах)
    [[nodiscard]] bool check_uplink_rate(uint32_t packet_size_bytes) {
        return _uplink_limiter.consume(packet_size_bytes * 8); // Конвертируем в биты
    }
    
    // Проверка скорости для downlink трафика (размер в байтах)
    [[nodiscard]] bool check_downlink_rate(uint32_t packet_size_bytes) {
        return _downlink_limiter.consume(packet_size_bytes * 8);
    }
    
    // Установка новой скорости для uplink
    void set_uplink_rate(uint32_t rate_bps) {
        _uplink_limiter.set_rate(rate_bps);
    }
    
    // Установка новой скорости для downlink
    void set_downlink_rate(uint32_t rate_bps) {
        _downlink_limiter.set_rate(rate_bps);
    }

private:
    uint32_t _dp_teid{};
    pdn_connection &_pdn;
    RateLimiter _uplink_limiter;  // Для uplink трафика
    RateLimiter _downlink_limiter; // Для downlink трафика
    uint32_t _sgw_dp_teid{};
};
