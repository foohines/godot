#[vertex]

#version 450

#VERSION_DEFINES

#include "canvas_uniforms_inc.glsl"

layout(location = 8) in vec4 world_matrix_ab;  // xy=world_x, zw=world_y
layout(location = 9) in vec4 world_matrix_c;   // xy=world_ofs
layout(location = 10) in vec4 modulation;       // r=base_height
layout(location = 12) in vec4 dst_rect;
layout(location = 13) in vec4 src_rect;

layout(location = 0) out vec2 uv_interp;
layout(location = 1) out flat float base_height_interp;

void main() {
    vec2 vertex_base_arr[4] = vec2[](
        vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0)
    );
    vec2 vertex_base = vertex_base_arr[gl_VertexIndex];

    vec2 uv = src_rect.xy + abs(src_rect.zw) * vertex_base;
    vec2 vertex = dst_rect.xy + abs(dst_rect.zw) * mix(
        vertex_base,
        vec2(1.0, 1.0) - vertex_base,
        lessThan(src_rect.zw, vec2(0.0, 0.0))
    );

    mat4 model_matrix = mat4(
        vec4(world_matrix_ab.xy, 0.0, 0.0),
        vec4(world_matrix_ab.zw, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(world_matrix_c.xy, 0.0, 1.0)
    );

    vertex = (model_matrix * vec4(vertex, 0.0, 1.0)).xy;
    vertex = (canvas_data.canvas_transform * vec4(vertex, 0.0, 1.0)).xy;

    uv_interp = uv;
    base_height_interp = 1.0 - modulation.r;

    gl_Position = canvas_data.screen_transform * vec4(vertex, 0.0, 1.0);
}

#[fragment]

#version 450

#VERSION_DEFINES

#include "canvas_uniforms_inc.glsl"

layout(location = 0) in vec2 uv_interp;
layout(location = 1) in flat float base_height_interp;

layout(location = 0) out vec4 height_out;

void main() {
    float alpha = texture(sampler2D(color_texture, texture_sampler), uv_interp).a;
    float local_height = texture(sampler2D(height_texture, texture_sampler), uv_interp).r;
    
    
    if (alpha < 0.9) {
        discard;
    }

    // height_out = vec4(uv_interp.x, uv_interp.y, 0.0, 1.0);
    height_out = vec4(local_height + base_height_interp, 0.0, 0.0, 1.0);
    // height_out = vec4(0.0, uv_interp.x, uv_interp.y, 1.0);
}