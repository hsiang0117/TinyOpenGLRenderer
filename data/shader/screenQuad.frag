#version 430 core

in vec2 texCoords;

uniform sampler2DArray shadowArray;

out vec4 fragColor;

void main(){
	float color = texture(shadowArray, vec3(texCoords, 0)).r;
	fragColor = vec4(vec3(color),1.0);
}