#version 460

#define M_PI 3.1415926535897932384626433832795
#define M_G 9.81

layout (local_size_x = 32, local_size_y = 32) in;

layout(binding = 0) uniform UBO {
	vec2 W;
	int N;
	float A;
	int L;
} ubo;
layout(binding = 1, rgba32f) writeonly uniform image2D h0tilde;
layout(binding = 2, rgba32f) writeonly uniform image2D h0tilde_conj;
layout(binding = 3) buffer UNIF_RAND_STORAGE_BUF {
	vec4 data[];
} unif_randoms;

struct complex {
	float re;
	float im;
};

complex conj(complex w) {
	return complex(w.re, -w.im);
}

float phillips(vec2 k) {
	float k_len = sqrt((k.x * k.x) + (k.y * k.y));
	if(k_len == 0.0f) return 0.0f;
	float k_len_squared = k_len * k_len;

	// Largest possible wave from constant wind of velocity V
	float w_len = sqrt((ubo.W.x * ubo.W.x) + (ubo.W.y * ubo.W.y));
	float L = (w_len * w_len) / M_G;

	vec2 k_unit = vec2(k.x / k_len, k.y / k_len);
	vec2 w_unit = vec2(ubo.W.x / w_len, ubo.W.y / w_len);
	float k_dot_w = dot(k_unit, w_unit);
	float k_dot_w6 = k_dot_w * k_dot_w * k_dot_w * k_dot_w * k_dot_w * k_dot_w;

	float damping = 0.00001f;

	return ubo.A * (exp(-1.0f / (k_len_squared * L * L)) / (k_len_squared * k_len_squared)) * k_dot_w6 * exp(-k_len_squared * damping);
}

vec4 gauss(vec4 unif_randoms) {
	float u0 = 2.0f * M_PI * unif_randoms.x;
	float v0 = sqrt(-2.0f * log(unif_randoms.y));
	float u1 = 2.0f * M_PI * unif_randoms.z;
	float v1 = sqrt(-2.0f * log(unif_randoms.w));

	return vec4(v0 * cos(u0), v0 * sin(u0), v1 * cos(u1), v1 * sin(u1));
}


complex compute_h0tilde(vec2 randoms, vec2 k) {
	float pf = sqrt(phillips(k) / 2.0f);
	return complex(randoms.x * pf, randoms.y * pf);
}


void main() {
	uint globalIndex = gl_GlobalInvocationID.y * ubo.N + gl_GlobalInvocationID.x;
	float n_prime = gl_GlobalInvocationID.x;
	float m_prime = gl_GlobalInvocationID.y;
	vec2 k = vec2(M_PI * (2 * n_prime - ubo.N) / ubo.L, M_PI * (2 * m_prime - ubo.N) / ubo.L);

	vec4 gauss_rnds = gauss(unif_randoms.data[globalIndex]);

	complex c_h0tilde = compute_h0tilde(gauss_rnds.xy, k);
	complex c_h0tilde_conj = conj(compute_h0tilde(gauss_rnds.zw, -k));

	imageStore(h0tilde, ivec2(gl_GlobalInvocationID.xy), vec4(c_h0tilde.re, c_h0tilde.im, 0.0f, 0.0f));
	imageStore(h0tilde_conj, ivec2(gl_GlobalInvocationID.xy), vec4(c_h0tilde_conj.re, c_h0tilde_conj.im, 0.0f, 0.0f));
}