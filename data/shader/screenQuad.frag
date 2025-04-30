#version 450 core

in vec2 texCoords;

uniform samplerCubeArray depthMap;

out vec4 fragColor;

void main(){
	float color = 1.0;
	fragColor = vec4(vec3(color),1.0);
}