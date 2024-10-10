#define _USE_MATH_DEFINES

#include "camera.h"

#include "utils/error_handler.h"

#include <math.h>


using namespace cg::world;

cg::world::camera::camera() : theta(0.f), phi(0.f), height(1080.f), width(1920.f),
							  aspect_ratio(1920.f / 1080.f), angle_of_view(1.04719f),
							  z_near(0.001f), z_far(100.f), position(float3{0.f, 0.f, 0.f})
{
}

cg::world::camera::~camera() {}

void cg::world::camera::set_position(float3 in_position)
{
	position = in_position;
}

void cg::world::camera::set_theta(float in_theta)
{
	theta = in_theta;
}

void cg::world::camera::set_phi(float in_phi)
{
	phi = in_phi;
}

void cg::world::camera::set_angle_of_view(float in_aov)
{
	angle_of_view = in_aov;
}

void cg::world::camera::set_height(float in_height)
{
	height = in_height;
    aspect_ratio = width / height;
}

void cg::world::camera::set_width(float in_width)
{
	width = in_width;
    aspect_ratio = width / height;
}

void cg::world::camera::set_z_near(float in_z_near)
{
	z_near = in_z_near;
}

void cg::world::camera::set_z_far(float in_z_far)
{
	z_far = in_z_far;
}

const float4x4 cg::world::camera::get_view_matrix() const
{
    float3 direction = get_direction();
    float3 right = get_right();
    float3 up = get_up();

    return float4x4{
        {right.x, right.y, right.z, -dot(right, position)},
        {up.x, up.y, up.z, -dot(up, position)},
        {-direction.x, -direction.y, -direction.z, dot(direction, position)},
        {0.f, 0.f, 0.f, 1.f}
    };
}

#ifdef DX12
const DirectX::XMMATRIX cg::world::camera::get_dxm_view_matrix() const
{
	// TODO Lab: 3.08 Implement `get_dxm_view_matrix`, `get_dxm_projection_matrix`, and `get_dxm_mvp_matrix` methods of `camera`
	return  DirectX::XMMatrixIdentity();
}

const DirectX::XMMATRIX cg::world::camera::get_dxm_projection_matrix() const
{
	// TODO Lab: 3.08 Implement `get_dxm_view_matrix`, `get_dxm_projection_matrix`, and `get_dxm_mvp_matrix` methods of `camera`
	return  DirectX::XMMatrixIdentity();
}

const DirectX::XMMATRIX camera::get_dxm_mvp_matrix() const
{
	// TODO Lab: 3.08 Implement `get_dxm_view_matrix`, `get_dxm_projection_matrix`, and `get_dxm_mvp_matrix` methods of `camera`
	return  DirectX::XMMatrixIdentity();
}
#endif

const float4x4 cg::world::camera::get_projection_matrix() const
{
    float f = 1.f / tan(angle_of_view / 2.f);
    return float4x4{
        {f / aspect_ratio, 0.f, 0.f, 0.f},
        {0.f, f, 0.f, 0.f},
        {0.f, 0.f, z_far / (z_far - z_near), (-z_far * z_near) / (z_far - z_near)},
        {0.f, 0.f, 1.f, 0.f}
    };
}

const float3 cg::world::camera::get_position() const
{
	return position;
}

const float3 cg::world::camera::get_direction() const
{
    return float3{
        cos(phi) * sin(theta),
        sin(phi),
        cos(phi) * cos(theta)
    };
}

const float3 cg::world::camera::get_right() const
{
    return float3{
        sin(theta - static_cast<float>(M_PI) / 2.f),
        0.f,
        cos(theta - static_cast<float>(M_PI) / 2.f)
    };
}

const float3 cg::world::camera::get_up() const
{
    float3 right = get_right();
    float3 direction = get_direction();
    return cross(right, direction);
}
const float camera::get_theta() const
{
	return theta;
}
const float camera::get_phi() const
{
	return phi;
}
