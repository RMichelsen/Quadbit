#version 460

layout(location = 0) out vec4 outColour;
// layout(location = 0) out float outDepth;

void main() {
    // outDepth = gl_FragCoord.z;
	outColour = vec4(1.0, 0.0, 0.0, 1.0);
}