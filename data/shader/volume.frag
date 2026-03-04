#version 450 core

layout(location = 0) out vec4 fragColor;

in vec3 WorldPos;

layout (std140, binding = 0) uniform Matrices{
	mat4 view;
	mat4 projection;
};

uniform vec3 cameraPos;

uniform vec3 aabbMin;
uniform vec3 aabbMax;

float rayBoxDst(vec3 boundsMin, vec3 boundsMax, vec3 rayOrigin, vec3 rayDir) {
	vec3 invRayDir = 1.0 / rayDir;
	vec3 t0 = (boundsMin - rayOrigin) * invRayDir;
	vec3 t1 = (boundsMax - rayOrigin) * invRayDir;
	vec3 tmin = min(t0, t1);
	vec3 tmax = max(t0, t1);

	float dstA = max(max(tmin.x, tmin.y), tmin.z);
	float dstB = min(min(tmax.x, tmax.y), tmax.z);

	float dstToBox = max(0.0, dstA);
	float dstInsideBox = max(0.0, dstB - dstToBox);

	return dstInsideBox;
}

float cloudRayMarching(vec3 rayOrigin, vec3 rayDirection){
	vec3 testPoint = rayOrigin;
	float sum = 0.0;
	vec3 stepSize = rayDirection * 0.1;
	float rayBoxDst = rayBoxDst(aabbMin, aabbMax, rayOrigin, rayDirection);
	float marchingDst = 0.0;
	for(int i = 0; i < 256; i++){
		testPoint += stepSize;
		marchingDst += length(stepSize);

		if (testPoint.x >= aabbMin.x && testPoint.x <= aabbMax.x &&
			testPoint.y >= aabbMin.y && testPoint.y <= aabbMax.y &&
			testPoint.z >= aabbMin.z && testPoint.z <= aabbMax.z)
		{
			sum += 0.1;
		}

		if (marchingDst > rayBoxDst) break;
	}
	return sum;
}

void main()
{
	vec3 rayDir = normalize(WorldPos - cameraPos);

	float density = cloudRayMarching(WorldPos, rayDir);

	if (density < 0.01) {
		discard;
	}

	vec3 cloudColor = vec3(1.0, 1.0, 1.0);
	fragColor = vec4(cloudColor * density, 1.0);
}
