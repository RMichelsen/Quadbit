#version 460

#define M_TWOPI 6.283185307179586476925286766559

layout(local_size_x_id = 0) in;
layout(local_size_y_id = 1) in;
layout(local_size_z_id = 2) in;
layout(constant_id = 3) const int LENGTH = 512;
layout(constant_id = 4) const int PASS_COUNT = 9;
layout(constant_id = 5) const int VERTICAL_PASS = 0;

layout(push_constant) uniform PushConstants {
	int iteration;
} pc;

layout(binding = 0, rgba32f) readonly uniform image2D input_images[5];
layout(binding = 1, rgba32f) writeonly uniform image2D output_images[5];

struct complex {
	float re;
	float im;
};

complex add(complex z, complex w) {
	return complex(z.re + w.re, z.im + w.im);
}

complex mul(complex z, complex w) {
	return complex((z.re * w.re) - (z.im * w.im), (z.re * w.im) + (w.re * z.im));
}

complex mul(complex z, float fac) {
	return complex(z.re * fac, z.im * fac);
}

// SLM approach outlined in https://software.intel.com/en-us/articles/fast-fourier-transform-for-image-processing-in-directx-11
shared complex pingpong[2][LENGTH];
void get_butterfly_values(int pass_index, uint pos, out uvec2 indices, out complex w) {
	const uint sec_width = 2 << pass_index;
	const uint half_sec_width = sec_width / 2;

	const uint sec_start_offset = pos & ~(sec_width - 1);
	const uint half_sec_offset = pos & (half_sec_width - 1);
	const uint sec_offset = pos & (sec_width - 1);

	const float k = (M_TWOPI * float(sec_offset)) / float(sec_width);
	// Euler...
	w = complex(cos(k), sin(k));

	indices.x = sec_start_offset + half_sec_offset;
	indices.y = sec_start_offset + half_sec_offset + half_sec_width;

	if(pass_index == 0) {	
		indices = bitfieldReverse(indices) >> (32 - PASS_COUNT) & (LENGTH - 1);
	}
}

// Perform the butterfly calculation (multiply by 0.5 since its an inverse FFT, its possible to scale at the end by 1/N^2 instead)
void butterfly_pass(int pass_index, uint local_index, int pingpong_index, out complex res) {
	uvec2 indices;
	complex w;
	get_butterfly_values(pass_index, local_index, indices, w);

	complex c0 = pingpong[pingpong_index][indices.x];
	complex c1 = pingpong[pingpong_index][indices.y];
	res = mul(add(c0, mul(c1, w)), 0.5f);
}

void main() {
	// Fetch position and pixel based on direction and load row or column into the shared ping pong arrays
	uint local_index = gl_GlobalInvocationID.x;
	uvec2 pos = (VERTICAL_PASS == 0) ? gl_GlobalInvocationID.xy : gl_GlobalInvocationID.yx;

	vec2 entry = imageLoad(input_images[pc.iteration], ivec2(pos)).rg;

	pingpong[0][local_index] = complex(entry.x, entry.y);

	ivec2 pingpong_indices = ivec2(0, 1);

	for(int i = 0; i < PASS_COUNT - 1; i++) {
		groupMemoryBarrier();
		barrier();

		butterfly_pass(i, local_index, pingpong_indices.x, pingpong[pingpong_indices.y][local_index]);

		// Swap pingpong textures for next write
		pingpong_indices.xy = pingpong_indices.yx;
	}
	
	groupMemoryBarrier();
	barrier();

	// Final pass writes directly to the output textures
	complex res;
	if(VERTICAL_PASS == 1) {
		float sign_factor = (gl_GlobalInvocationID.x + gl_GlobalInvocationID.y) % 2 == 0 ? -1.0f : 1.0f;
		butterfly_pass(PASS_COUNT - 1, local_index, pingpong_indices.x, res);
		imageStore(output_images[pc.iteration], ivec2(pos), vec4(res.re * sign_factor, res.re * sign_factor, res.re * sign_factor, 0.0f));
	}
	else {
		butterfly_pass(PASS_COUNT - 1, local_index, pingpong_indices.x, res);
		imageStore(output_images[pc.iteration], ivec2(pos), vec4(res.re, res.im, 0.0f, 0.0f));
	}
}