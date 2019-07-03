#version 460

#define M_PI 3.1415926535897932384626433832795
#define M_G 9.81

layout (local_size_x = 32, local_size_y = 32) in;

layout(binding = 0) uniform UBO {
	int N;
	int L;
	float RT;
	float T;
} ubo;

layout(binding = 1, rgba32f) readonly uniform image2D h0tilde;
layout(binding = 2, rgba32f) readonly uniform image2D h0tilde_conj;
layout(binding = 3, rgba32f) writeonly uniform image2D h0tilde_tx;
layout(binding = 4, rgba32f) writeonly uniform image2D h0tilde_ty;
layout(binding = 5, rgba32f) writeonly uniform image2D h0tilde_tz;

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

float dispersion(vec2 k) {
	float w_0 = 2.0f * M_PI / ubo.RT;
	return floor(sqrt(M_G * sqrt((k.x * k.x) + (k.y * k.y))) / w_0) * w_0;
}

void main() {
	float n_prime = gl_GlobalInvocationID.x;
	float m_prime = gl_GlobalInvocationID.y;
	vec2 k = vec2(M_PI * (2 * n_prime - ubo.N) / ubo.L, M_PI * (2 * m_prime - ubo.N) / ubo.L);

	vec4 h0tilde = imageLoad(h0tilde, ivec2(gl_GlobalInvocationID.xy));
	vec4 h0tilde_conj = imageLoad(h0tilde_conj, ivec2(gl_GlobalInvocationID.xy));

	complex c_h0tilde = complex(h0tilde.r, h0tilde.g);
	complex c_h0tilde_conj = complex(h0tilde_conj.r, h0tilde_conj.g);

	// Use Eulers formula to compute the time-dependent variables hkt_(x,y,z)
	float w = dispersion(k);
	complex hkt_x, hkt_y, hkt_z;
	float cosine = cos(w * ubo.T);
	float sine = sin(w * ubo.T);

	hkt_y = add(mul(c_h0tilde, complex(cosine, sine)), mul(c_h0tilde_conj, complex(cosine, -sine)));

	float k_len = sqrt((k.x * k.x) + (k.y * k.y));
	if(k_len < 0.0000001f) {
		hkt_x = complex(0.0f, 0.0f);
		hkt_z = complex(0.0f, 0.0f);
	}
	else {
		hkt_x = mul(hkt_y, complex(0.0f, -k.x / k_len));
		hkt_z = mul(hkt_y, complex(0.0f, -k.y / k_len));
	}

	imageStore(h0tilde_tx, ivec2(gl_GlobalInvocationID.xy), vec4(hkt_x.re, hkt_x.im, 0.0f, 0.0f));
	imageStore(h0tilde_ty, ivec2(gl_GlobalInvocationID.xy), vec4(hkt_y.re, hkt_y.im, 0.0f, 0.0f));
	imageStore(h0tilde_tz, ivec2(gl_GlobalInvocationID.xy), vec4(hkt_z.re, hkt_z.im, 0.0f, 0.0f));
}