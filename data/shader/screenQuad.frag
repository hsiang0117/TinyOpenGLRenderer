#version 430 core

in vec2 texCoords;

uniform sampler2D directionLightDepth;

out vec4 fragColor;

void main(){
	float color = texture(directionLightDepth, texCoords).r;
	fragColor = vec4(vec3(color),1.0);
}