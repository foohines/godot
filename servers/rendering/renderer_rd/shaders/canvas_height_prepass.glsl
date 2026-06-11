#[vertex]

#version 450

#VERSION_DEFINES

#include "canvas_uniforms_inc.glsl"

layout(location = 8) in vec4 world_matrix_ab;
layout(location = 9) in vec4 world_matrix_c;
layout(location = 10) in vec4 modulation;
layout(location = 12) in vec4 dst_rect;
layout(location = 13) in vec4 src_rect;
layout(location = 14) in uvec4 attrib_G;

layout(location = 0) out vec2 uv_interp;
layout(location = 1) out flat float base_height_interp;
layout(location = 2) out flat vec4 modulation_interp;

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
    base_height_interp = uintBitsToFloat(attrib_G.x);
    modulation_interp = modulation;

    gl_Position = canvas_data.screen_transform * vec4(vertex, 0.0, 1.0);
}

#[fragment]

#version 450

#VERSION_DEFINES

#include "canvas_uniforms_inc.glsl"

layout(location = 0) in vec2 uv_interp;
layout(location = 1) in flat float base_height_interp;
layout(location = 2) in flat vec4 modulation_interp;

layout(location = 0) out vec2 height_out;

vec4 smooth_texel_hp(texture2D texture, sampler samp, vec2 uv, vec2 pixel_size)
{
	vec2 ddx = dFdx(uv);
	vec2 ddy = dFdy(uv);
	vec2 lxy = sqrt(ddx * ddx + ddy * ddy);

	vec2 uv_pixels = uv / pixel_size;

	vec2 uv_pixels_floor = round(uv_pixels) - vec2(0.5f);
	vec2 uv_dxy_pixels = uv_pixels - uv_pixels_floor;

	uv_dxy_pixels = clamp((uv_dxy_pixels - vec2(0.5f)) * pixel_size / lxy + vec2(0.5f), 0.0f, 1.0f);

	uv = uv_pixels_floor * pixel_size;

	return textureGrad(sampler2D(texture, samp), uv + uv_dxy_pixels * pixel_size, ddx, ddy);
}

void main() {
    float alpha = smooth_texel_hp(color_texture, texture_sampler, uv_interp, params.color_texture_pixel_size).a * modulation_interp.a;
    float local_height = smooth_texel_hp(height_texture, texture_sampler, uv_interp, params.color_texture_pixel_size).r * 255.0;
    // float alpha = texture(sampler2D(color_texture, texture_sampler), uv_interp).a;
    // float local_height = texture(sampler2D(height_texture, texture_sampler), uv_interp).r * 255.0;
    
    
    if (alpha < 0.99) {
        discard;
    }

    // height_out = vec4(uv_interp.x, uv_interp.y, 0.0, 1.0);
    // height_out = vec4(0.0, uv_interp.x, uv_interp.y, 1.0);
    height_out = vec2(base_height_interp + local_height, 0.0);
}