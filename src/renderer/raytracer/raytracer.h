#pragma once

#include "resource.h"
#include <iostream>
#include <linalg.h>
#include <memory>
#include <omp.h>
#include <random>

using namespace linalg::aliases;

namespace cg::renderer
{
	struct ray
	{
		ray(float3 position, float3 direction) : position(position)
		{
			this->direction = normalize(direction);
		}
		float3 position;
		float3 direction;
	};

	struct payload
	{
		float t;
		float3 bary;
		cg::color color;
	};

	template<typename VB>
	struct triangle
	{
		triangle(const VB& vertex_a, const VB& vertex_b, const VB& vertex_c);

		float3 a;
		float3 b;
		float3 c;
		float3 ba;
		float3 ca;
		float3 na;
		float3 nb;
		float3 nc;
		float3 ambient;
		float3 diffuse;
		float3 emissive;
	};

	template<typename VB>
	inline triangle<VB>::triangle(const VB& vertex_a, const VB& vertex_b, const VB& vertex_c)
	{
		a = vertex_a.position;
		b = vertex_b.position;
		c = vertex_c.position;

		ba = b - a;
		ca = c - a;

		na = vertex_a.normal;
		nb = vertex_b.normal;
		nc = vertex_c.normal;

		ambient = vertex_a.ambient;
		diffuse = vertex_a.diffuse;
		emissive = vertex_a.emissive;
	}

	template<typename VB>
	class aabb
	{
	public:
		void add_triangle(const triangle<VB>& triangle);
		const std::vector<triangle<VB>>& get_triangles() const;
		bool aabb_test(const ray& ray) const;

	protected:
		std::vector<triangle<VB>> triangles;
		float3 aabb_min;
		float3 aabb_max;
	};

	template<typename VB>
	inline void aabb<VB>::add_triangle(const triangle<VB>& triangle)
	{
		triangles.push_back(triangle);
		aabb_min = min(aabb_min, min(triangle.a, min(triangle.b, triangle.c)));
		aabb_max = max(aabb_max, max(triangle.a, max(triangle.b, triangle.c)));
	}

	template<typename VB>
	inline const std::vector<triangle<VB>>& aabb<VB>::get_triangles() const
	{
		return triangles;
	}

	template<typename VB>
	inline bool aabb<VB>::aabb_test(const ray& ray) const
	{
		float tmin = (aabb_min.x - ray.position.x) / ray.direction.x;
		float tmax = (aabb_max.x - ray.position.x) / ray.direction.x;
		if (tmin > tmax) std::swap(tmin, tmax);

		float tymin = (aabb_min.y - ray.position.y) / ray.direction.y;
		float tymax = (aabb_max.y - ray.position.y) / ray.direction.y;
		if (tymin > tymax) std::swap(tymin, tymax);

		if ((tmin > tymax) || (tymin > tmax)) return false;

		if (tymin > tmin) tmin = tymin;
		if (tymax < tmax) tmax = tymax;

		float tzmin = (aabb_min.z - ray.position.z) / ray.direction.z;
		float tzmax = (aabb_max.z - ray.position.z) / ray.direction.z;
		if (tzmin > tzmax) std::swap(tzmin, tzmax);

		if ((tmin > tzmax) || (tzmin > tmax)) return false;

		return true;
	}

	struct light
	{
		float3 position;
		float3 color;
	};

	template<typename VB, typename RT>
	class raytracer
	{
	public:
		raytracer(){};
		~raytracer(){};

		void set_render_target(std::shared_ptr<resource<RT>> in_render_target);
		void clear_render_target(const RT& in_clear_value);
		void set_viewport(size_t in_width, size_t in_height);

		void set_vertex_buffers(std::vector<std::shared_ptr<cg::resource<VB>>> in_vertex_buffers);
		void set_index_buffers(std::vector<std::shared_ptr<cg::resource<unsigned int>>> in_index_buffers);
		void build_acceleration_structure();
		std::vector<aabb<VB>> acceleration_structures;

		void ray_generation(float3 position, float3 direction, float3 right, float3 up, size_t depth, size_t accumulation_num);

		payload trace_ray(const ray& ray, size_t depth, float max_t = 1000.f, float min_t = 0.001f) const;
		payload intersection_shader(const triangle<VB>& triangle, const ray& ray) const;

		std::function<payload(const ray& ray)> miss_shader = nullptr;
		std::function<payload(const ray& ray, payload& payload, const triangle<VB>& triangle, size_t depth)>
				closest_hit_shader = nullptr;
		std::function<payload(const ray& ray, payload& payload, const triangle<VB>& triangle)> any_hit_shader =
				nullptr;

		float2 get_jitter(int frame_id);

	protected:
		std::shared_ptr<cg::resource<RT>> render_target;
		std::shared_ptr<cg::resource<float3>> history;
		std::vector<std::shared_ptr<cg::resource<unsigned int>>> index_buffers;
		std::vector<std::shared_ptr<cg::resource<VB>>> vertex_buffers;
		std::vector<triangle<VB>> triangles;

		size_t width = 1920;
		size_t height = 1080;
	};

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::set_render_target(std::shared_ptr<resource<RT>> in_render_target)
	{
		render_target = in_render_target;
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::set_viewport(size_t in_width, size_t in_height)
	{
		width = in_width;
		height = in_height;
		history = std::make_shared<resource<float3>>(width, height);
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::clear_render_target(const RT& in_clear_value)
	{
		for (size_t i = 0; i < render_target->count(); ++i)
		{
			render_target->item(i) = in_clear_value;
		}
		for (size_t i = 0; i < history->count(); ++i)
		{
			history->item(i) = float3{0.f, 0.f, 0.f};
		}
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::set_vertex_buffers(std::vector<std::shared_ptr<cg::resource<VB>>> in_vertex_buffers)
	{
		vertex_buffers = in_vertex_buffers;
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::set_index_buffers(std::vector<std::shared_ptr<cg::resource<unsigned int>>> in_index_buffers)
	{
		index_buffers = in_index_buffers;
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::build_acceleration_structure()
	{
		for (size_t i = 0; i < vertex_buffers.size(); ++i)
		{
			for (size_t j = 0; j < index_buffers[i]->count(); j += 3)
			{
				VB v0 = vertex_buffers[i]->item(index_buffers[i]->item(j));
				VB v1 = vertex_buffers[i]->item(index_buffers[i]->item(j + 1));
				VB v2 = vertex_buffers[i]->item(index_buffers[i]->item(j + 2));
				triangle<VB> tri(v0, v1, v2);
				triangles.push_back(tri);
			}
		}
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::ray_generation(float3 position, float3 direction, float3 right, float3 up, size_t depth, size_t accumulation_num)
	{
		#pragma omp parallel for
		for (int x = 0; x < width; x++)
		{
			for (int y = 0; y < height; y++)
			{
				float2 jitter = get_jitter(accumulation_num);
				float u = (x + jitter.x) / width - 0.5f;
				float v = (y + jitter.y) / height - 0.5f;

				float3 ray_dir = normalize(direction + u * right + v * up);
				ray r(position, ray_dir);
				payload p = trace_ray(r, depth);

				render_target->item(x, y) = p.color;
			}
		}
	}

	template<typename VB, typename RT>
	inline payload raytracer<VB, RT>::trace_ray(const ray& ray, size_t depth, float max_t, float min_t) const
	{
		payload closest_payload;
		closest_payload.t = max_t;
		for (const auto& tri : triangles)
		{
			payload p = intersection_shader(tri, ray);
			if (p.t > min_t && p.t < closest_payload.t)
			{
				closest_payload = p;
				if (any_hit_shader)
				{
					return any_hit_shader(ray, closest_payload, tri);
				}
			}
		}
		if (closest_payload.t == max_t && miss_shader)
		{
			return miss_shader(ray);
		}
		return closest_payload;
	}

	template<typename VB, typename RT>
	inline payload raytracer<VB, RT>::intersection_shader(const triangle<VB>& triangle, const ray& ray) const
	{
		float3 pvec = cross(ray.direction, triangle.ca);
		float det = dot(triangle.ba, pvec);

		if (fabs(det) < 1e-8) return payload{};

		float inv_det = 1.0f / det;
		float3 tvec = ray.position - triangle.a;
		float u = dot(tvec, pvec) * inv_det;
		if (u < 0 || u > 1) return payload{};

		float3 qvec = cross(tvec, triangle.ba);
		float v = dot(ray.direction, qvec) * inv_det;
		if (v < 0 || u + v > 1) return payload{};

		float t = dot(triangle.ca, qvec) * inv_det;
		return payload{t, float3{u, v, 1 - u - v}, triangle.diffuse};
	}

	template<typename VB, typename RT>
	inline float2 raytracer<VB, RT>::get_jitter(int frame_id)
	{
		std::mt19937 gen(frame_id);
		std::uniform_real_distribution<float> dis(0.0f, 1.0f);
		return float2{dis(gen), dis(gen)};
	}
} // namespace cg::renderer
