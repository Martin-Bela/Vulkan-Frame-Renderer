#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
	float x = gl_FragCoord.x / 800.0;
	float y = gl_FragCoord.y / 800.0;
    outColor = texture(texSampler, vec2(x, y));
}