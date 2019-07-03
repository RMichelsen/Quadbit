#version 460

#define M_PI 3.1415926535897932384626433832795
#define M_TWOPI 6.283185307179586476925286766559

layout(local_size_x_id = 0) in;
layout(local_size_y_id = 1) in;
layout(local_size_z_id = 2) in;
layout(constant_id = 3) const int LENGTH = 512;
layout(constant_id = 4) const int PASS_COUNT = 9;
layout(constant_id = 5) const int VERTICAL_PASS = 0;

//layout(binding = 0) uniform UBO {
//	uint VERTICAL_PASS;
//} ubo;
layout(binding = 0, rgba32f) readonly uniform image2D h0tilde_tx;
layout(binding = 1, rgba32f) readonly uniform image2D h0tilde_ty;
layout(binding = 2, rgba32f) readonly uniform image2D h0tilde_tz;

layout(binding = 3, rgba32f) writeonly uniform image2D Dx;
layout(binding = 4, rgba32f) writeonly uniform image2D Dy;
layout(binding = 5, rgba32f) writeonly uniform image2D Dz;

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

shared complex pingpong_tx[2][LENGTH];
shared complex pingpong_ty[2][LENGTH];
shared complex pingpong_tz[2][LENGTH];
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

// Perform the butterfly calculation (0.5 added since its an inverse FFT, its possible to scale at the end by 1/N^2 instead)
void butterfly_pass(int pass_index, uint local_index, int pingpong_index, out complex txRes, out complex tyRes, out complex tzRes) {
	uvec2 indices;
	complex w;
	get_butterfly_values(pass_index, local_index, indices, w);

	complex tx1 = pingpong_tx[pingpong_index][indices.x];
	complex tx2 = pingpong_tx[pingpong_index][indices.y];
	txRes = mul(add(tx1, mul(tx2, w)), 0.5f);

	complex ty1 = pingpong_ty[pingpong_index][indices.x];
	complex ty2 = pingpong_ty[pingpong_index][indices.y];
	tyRes = mul(add(ty1, mul(ty2, w)), 0.5f);

	complex tz1 = pingpong_tz[pingpong_index][indices.x];
	complex tz2 = pingpong_tz[pingpong_index][indices.y];
	tzRes = mul(add(tz1, mul(tz2, w)), 0.5f);
}

//void butterfly_pass_tx(int pingpong_index, uvec2 indices, complex w, out complex res) {
//	complex c1 = pingpong_tx[pingpong_index][indices.x];
//	complex c2 = pingpong_tx[pingpong_index][indices.y];
//
//	// Perform the butterfly calculation (0.5 added since its an inverse FFT, its possible to scale at the end by 1/N^2 instead)
//	res = mul(add(c1, mul(c2, w)), 0.5f);
//}
//
//void butterfly_pass_ty(int pingpong_index, uvec2 indices, complex w, out complex res) {
//	complex c1 = pingpong_ty[pingpong_index][indices.x];
//	complex c2 = pingpong_ty[pingpong_index][indices.y];
//
//	// Perform the butterfly calculation (0.5 added since its an inverse FFT, its possible to scale at the end by 1/N^2 instead)
//	res = mul(add(c1, mul(c2, w)), 0.5f);
//}
//
//void butterfly_pass_tz(int pingpong_index, uvec2 indices, complex w, out complex res) {
//	complex c1 = pingpong_tz[pingpong_index][indices.x];
//	complex c2 = pingpong_tz[pingpong_index][indices.y];
//
//	// Perform the butterfly calculation (0.5 added since its an inverse FFT, its possible to scale at the end by 1/N^2 instead)
//	res = mul(add(c1, mul(c2, w)), 0.5f);
//}

void main() {
	// Fetch position and pixel based on direction and load row or column into the shared ping pong array
	uint local_index = gl_GlobalInvocationID.x;
	uvec2 pos = (VERTICAL_PASS == 0) ? gl_GlobalInvocationID.xy : gl_GlobalInvocationID.yx;

	vec2 tx_entry = imageLoad(h0tilde_tx, ivec2(pos)).rg;
	vec2 ty_entry = imageLoad(h0tilde_ty, ivec2(pos)).rg;
	vec2 tz_entry = imageLoad(h0tilde_tz, ivec2(pos)).rg;

	pingpong_tx[0][local_index] = complex(tx_entry.x, tx_entry.y);
	pingpong_ty[0][local_index] = complex(ty_entry.x, ty_entry.y);
	pingpong_tz[0][local_index] = complex(tz_entry.x, tz_entry.y);

	ivec2 pingpong_indices = ivec2(0, 1);

	for(int i = 0; i < PASS_COUNT - 1; i++) {
		groupMemoryBarrier();
		barrier();

		butterfly_pass(i, local_index, pingpong_indices.x, pingpong_tx[pingpong_indices.y][local_index], pingpong_ty[pingpong_indices.y][local_index], 
			pingpong_tz[pingpong_indices.y][local_index]);

		// Swap pingpong textures for next write
		pingpong_indices.xy = pingpong_indices.yx;
	}
	
	groupMemoryBarrier();
	barrier();

	// Final pass writes directly to the output texture
	complex DxRes, DyRes, DzRes;
	if(VERTICAL_PASS == 1) {
		float sign_factor = (gl_GlobalInvocationID.x + gl_GlobalInvocationID.y) % 2 == 0 ? -1.0f : 1.0f;
		butterfly_pass(PASS_COUNT - 1, local_index, pingpong_indices.x, DxRes, DyRes, DzRes);
		imageStore(Dx, ivec2(pos), vec4(DxRes.re * sign_factor, DxRes.re * sign_factor, DxRes.re * sign_factor, 0.0f));
		imageStore(Dy, ivec2(pos), vec4(DyRes.re * sign_factor, DyRes.re * sign_factor, DyRes.re * sign_factor, 0.0f));
		imageStore(Dz, ivec2(pos), vec4(DzRes.re * sign_factor, DzRes.re * sign_factor, DzRes.re * sign_factor, 0.0f));
	}
	else {
		butterfly_pass(PASS_COUNT - 1, local_index, pingpong_indices.x, DxRes, DyRes, DzRes);
		imageStore(Dx, ivec2(pos), vec4(DxRes.re, DxRes.im, 0.0f, 0.0f));
		imageStore(Dy, ivec2(pos), vec4(DyRes.re, DyRes.im, 0.0f, 0.0f));
		imageStore(Dz, ivec2(pos), vec4(DzRes.re, DzRes.im, 0.0f, 0.0f));
	}
}