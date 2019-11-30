#version 460

#define PI 3.14159265359


layout(location = 0) in vec3 inViewVec;
layout(location = 1) in vec3 inSunViewVec;

layout(location = 0) out vec4 outColour;

vec3 YxyToXYZ(vec3 v) {
	float X = v.x * (v.y / v.z);
	float Z = (X / v.y) - X - v.x;
	return vec3(X, v.x, Z);
}

vec3 XYZToSRGB(vec3 v) {
	mat3 M = mat3(3.2404542, -1.5371385, -0.4985314,
				 -0.9692660,  1.8760108,  0.0415560,
				  0.0556434, -0.2040259,  1.0572252);

	return v * M;
}

void CalculateLuminanceDistribution(float T, out vec3 A, out vec3 B, out vec3 C, out vec3 D, out vec3 E) {
	A = vec3(0.1787 * T - 1.4630, -0.0193 * T - 0.2592, -0.0167 * T - 0.2608);
	B = vec3(-0.3554 * T + 0.4275, -0.0665 * T + 0.0008, -0.0950 * T + 0.0092);
	C = vec3(-0.0227 * T + 5.3251, -0.0004 * T + 0.2125, -0.0079 * T + 0.2102);
	D = vec3(0.1206 * T - 2.5771, -0.0641 * T - 0.8989, -0.0441 * T - 1.6537);
	E = vec3(-0.0670 * T + 0.3703, -0.0033 * T + 0.0452, -0.0109 * T + 0.0529);
}

vec3 CalculatePerezLuminance(vec3 A, vec3 B, vec3 C, vec3 D, vec3 E, float theta, float gamma) {
	return (1.0 + A * exp(B / cos(theta))) * (1.0 + C * exp(D * gamma) + E * (cos(gamma) * cos(gamma))); 
}

vec3 CalculateZenithLuminance(float T, float theta_s) {
	float chi = (0.4444444 - T / 120) * (PI - 2 * theta_s);
	float Y_z = (4.0453 * T - 4.9710) * tan(chi) - 0.2155 * T + 2.4192;

	float T2 = T * T;
	float theta_s2 = theta_s * theta_s;
	float theta_s3 = theta_s2 * theta_s;

	mat3x4 xConstants = mat3x4 ( 0.00166, -0.00375,  0.00209, 0,
								-0.02903,  0.06377, -0.03202, 0.00394,
								 0.11693, -0.21196,  0.06052, 0.25886);

	mat3x4 yConstants = mat3x4 ( 0.00275, -0.00610,  0.00317, 0,
								-0.04214,  0.08970, -0.04153, 0.00516,
								 0.15346, -0.26756,  0.06670, 0.26688);

	vec3 turbidities = vec3(T2, T, 1);
	vec4 thetas = vec4(theta_s3, theta_s2, theta_s, 1);

	float x_z = dot(thetas, xConstants * turbidities);
	float y_z = dot(thetas, yConstants * turbidities);

	return vec3(Y_z, x_z, y_z);
}

float saturatedDot( in vec3 a, in vec3 b )
{
	return max( dot( a, b ), 0.0 );   
}

vec3 CalculateSkyLuminance(vec3 V, vec3 S, float T) {
	vec3 A, B, C, D, E;
	CalculateLuminanceDistribution(T, A, B, C, D, E);

	vec3 up = vec3(0, 1, 0);
	float mag_up = length(up);
	float mag_S = length(S);
	float mag_V = length(V);

	float theta = acos(clamp(dot(V, up), 0.0, 1.0) / (mag_V * mag_up));
	float theta_s = acos(clamp(dot(S, up), 0.0, 1.0) / (mag_S * mag_up));
	float gamma = acos(clamp(dot(S, V), 0.0, 1.0) / (mag_S * mag_V));

	vec3 Y = CalculateZenithLuminance(T, theta_s);
	vec3 F_0 = CalculatePerezLuminance(A, B, C, D, E, theta, gamma);
	vec3 F_1 = CalculatePerezLuminance(A, B, C, D, E, 0, theta_s);

	vec3 Y_p = Y * (F_0 / F_1);

	return XYZToSRGB(YxyToXYZ(Y_p));
}

void main() {
	float turbidity = 2.0f;
	vec3 V = normalize(-inViewVec);
	vec3 S = normalize(inSunViewVec);

	vec3 luminance = CalculateSkyLuminance(V, S, turbidity);
	luminance *= 0.015;
	luminance = vec3(1.0) - exp(-luminance * 5.0);
	outColour = vec4(luminance, 1.0);
}