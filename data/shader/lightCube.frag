#version 450 core

uniform vec3 color;
uniform float brightness;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

void main(){
	fragColor = vec4(color * brightness, 1.0);
	float bright = dot(fragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
	if(bright > 1.0){
		brightColor = vec4(fragColor.rgb,1.0);
	}else{
		brightColor = vec4(vec3(0.0),1.0);
	}
}