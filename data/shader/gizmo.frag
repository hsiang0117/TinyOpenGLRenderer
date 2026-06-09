#version 450 core

in vec3 vColor;
out vec4 fragColor;

void main() {
	// Alpha 1 is load-bearing: CompositePass blends as color*(1-a) + rgb, so a=1
	// makes the gizmo color fully replace the scene (always-on-top overlay).
	fragColor = vec4(vColor, 1.0);
}
