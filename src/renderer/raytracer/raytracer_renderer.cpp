#include "raytracer_renderer.h"

#include "utils/resource_utils.h"

#include <iostream>


void cg::renderer::ray_tracing_renderer::init()
{
	render_target = std::make_shared<resource<unsigned_color>>(width, height);
	camera = std::make_shared<world::camera>();

	raytracer = std::make_shared<raytracer<vertex, unsigned_color>>();
	raytracer->set_render_target(render_target);
	raytracer->set_viewport(width, height);

	raytracer->build_acceleration_structure();

	lights.push_back({float3{5.0f, 5.0f, 5.0f}, float3{1.0f, 1.0f, 1.0f}});
}

void cg::renderer::ray_tracing_renderer::destroy() {}

void cg::renderer::ray_tracing_renderer::update() {}

void cg::renderer::ray_tracing_renderer::render()
{
	raytracer->miss_shader = [](const ray& ray) -> payload {
		return payload{.color = color{0.3f, 0.5f, 0.7f}};
	};

	raytracer->closest_hit_shader = [this](const ray& ray, payload& p, const triangle<vertex>& tri, size_t depth) -> payload {
		float3 hit_position = ray.position + ray.direction * p.t;
		float3 normal = normalize(p.bary.x * tri.na + p.bary.y * tri.nb + p.bary.z * tri.nc);

		float3 final_color = tri.ambient;

		for (const auto& light : lights)
		{
			ray shadow_ray(hit_position, light.position - hit_position);
			payload shadow_payload = shadow_raytracer->trace_ray(shadow_ray, depth);

			if (shadow_payload.t >= length(light.position - hit_position)) {
				float3 light_direction = normalize(light.position - hit_position);
				float intensity = max(dot(normal, light_direction), 0.0f);
				final_color += tri.diffuse * light.color * intensity;
			}
		}

		p.color = color::from_float3(final_color);
		return p;
	};

	shadow_raytracer->any_hit_shader = [](const ray& ray, payload& payload, const triangle<vertex>& triangle) -> payload {
		payload.t = 0.0f;
		return payload;
	};

	raytracer->clear_render_target(unsigned_color{0, 0, 0});
	raytracer->ray_generation(camera->get_position(), camera->get_direction(), camera->get_right(), camera->get_up(), 3, 1);

	utils::save_resource(*render_target, "output.png");
}

