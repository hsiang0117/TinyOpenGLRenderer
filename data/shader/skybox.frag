#version 450 core

in vec3 texCoords;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

uniform samplerCube skybox;

void main(){
	fragColor = texture(skybox, texCoords);
	brightColor = vec4(vec3(0.0),1.0);
}