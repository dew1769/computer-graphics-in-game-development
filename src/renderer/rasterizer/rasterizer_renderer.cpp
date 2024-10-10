#include "rasterizer_renderer.h"
#include "utils/resource_utils.h"

void cg::renderer::rasterization_renderer::init()
{
    render_target = std::make_shared<cg::resource<cg::unsigned_color>>(width, height);
    depth_buffer = std::make_shared<cg::resource<float>>(width, height);
    rasterizer->set_render_target(render_target, depth_buffer);
    rasterizer->set_viewport(width, height);
}

void cg::renderer::rasterization_renderer::render()
{
    rasterizer->clear_render_target(cg::unsigned_color{0, 0, 0}, DEFAULT_DEPTH);
}

void cg::renderer::rasterization_renderer::destroy()
{
    render_target.reset();
    depth_buffer.reset();
}

void cg::renderer::rasterization_renderer::update()
{
}
