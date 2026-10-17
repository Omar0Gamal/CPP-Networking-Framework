#pragma once

#include <darkfire/darkfire.hpp>
#include <cstdint>

enum class SpatialMsg : uint16_t {
    ClientInput   = 1,
    WorldSnapshot = 2
};

struct EntityState {
    uint32_t entityId = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float velX = 0.0f;
    float velY = 0.0f;
    float velZ = 0.0f;

    static EntityState Interpolate(const EntityState& a, const EntityState& b, double alpha) noexcept {
        float f = static_cast<float>(alpha);
        EntityState res = b;
        res.posX = a.posX + (b.posX - a.posX) * f;
        res.posY = a.posY + (b.posY - a.posY) * f;
        res.posZ = a.posZ + (b.posZ - a.posZ) * f;
        res.velX = a.velX + (b.velX - a.velX) * f;
        res.velY = a.velY + (b.velY - a.velY) * f;
        res.velZ = a.velZ + (b.velZ - a.velZ) * f;
        return res;
    }
};
DARKFIRE_DECLARE_TRIVIALLY_SERIALIZABLE(EntityState);
